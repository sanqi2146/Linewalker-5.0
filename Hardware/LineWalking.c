/*
 * 循迹模块 — 让小车沿着黑线跑
 * ==============================================================
 *
 * 【硬件接线】
 *   8路巡线模块 → I2C (PB6=时钟, PB7=数据)
 *   避障传感器   → PC14=右前, PC15=左前 (碰到东西就停)
 *   电机驱动     → TB6612 (PA1控制左轮, PA2控制右轮)
 *
 * 【传感器怎么看】
 *   车头朝前, 从左边往右边数:
 *     X1    X2    X3    X4    X5    X6    X7    X8
 *    最左 ←—— 中间区域 ————→ 最右
 *
 *   0 = 压到黑线(灯亮)    1 = 白底(灯灭)
 *
 *   举个例子: "11100111"
 *     X1白 X2白 X3白 X4黑 X5黑 X6白 X7白 X8白
 *                    ↑线在偏右一点
 *
 * 【偏差是什么意思】
 *   dev < 0  →  车偏左, 需要往右打
 *   dev > 0  →  车偏右, 需要往左打
 *   dev = 0  →  车在线的正中间
 *
 *   偏差怎么变成轮子动作:
 *     左轮 = 基础速度 + 纠偏量    (correction>0 → 左轮快 → 左转)
 *     右轮 = 基础速度 - 纠偏量
 *
 *   比如基础速度45, dev=+2.0(车偏右), 纠偏量=+20:
 *     左轮=45+20=65 右轮=45-20=25 → 车往左转, 回到线中间
 *
 * 【三个看线策略, 按优先级排列, 第一命中立即返回】
 *
 *   策略1 (锐角): 两段黑线中间有白缝 (gap>=1)
 *     → 判断方向(看最边缘探头 x1/x2 vs x7/x8)
 *     → 进入状态机: 直行穿过缝隙 → 全白后原地旋转找另一边
 *
 *   策略2 (直角): 线在最边上, 对面是白的 (cnt<4)
 *     → 只记 g_turn_dir 和清 PID 旧积分, 不控制轮子
 *     → 轮子继续由策略4的质心法驱动
 *     → 再次触发P2时方向保护: 锁活跃时反向P2不覆盖
 *
 *   策略4 (质心法): 上面都没命中时
 *     → 黑探头平均位置 - 3.5 = 偏差, 死区±0.20
 *
 * 【方向变量 — 丢了线往哪转】
 *
 *   g_turn_dir — 转弯方向 (锁)
 *     策略1或策略2触发时记下: +1右弯 -1左弯
 *     锁期间: 反向P2不覆盖  /  丢了线优先用它旋转
 *     解锁: 连续正常直行 -> g_turn_lock 帧后到期
 *
 *   g_line_drift — 老线漂移方向 (跨帧积累)
 *     每帧追老线质心, 慢累加漂移量
 *     丢线时兜底用, 比单帧快照更可信
 *
 *   g_last_dev_dir — 探头快照方向 (每帧更新)
 *     看左3路(x1~x3) vs 右3路(x6~x8)哪边黑多
 *     锁活跃时不更新, 防转头污染
 *     丢线时最后的兜底
 *
 *   丢线方向优先级: g_turn_dir > g_line_drift > g_last_dev_dir
 *
 * 【锁 — 三项条件同时满足才解锁】
 *   1) 偏差小 (adev < 1.5)
 *   2) 线在中间区域 (x3~x6 有黑)
 *   3) 连续满足 -> g_turn_lock 帧 (App TL=可调)
 *   任意一条不满足 g_turn_ok 归零, 锁还在, 只是重新数
 *
 * 【老线追踪】
 *   g_line_centroid 追踪跟上一帧相近的黑探头(±2.5格以内)
 *   每帧算出老线质心, 漂移量慢累积(×0.3), 不抖动
 *   锁不解冻, 始终追踪; ±2.5窗口自带抗新线干扰
 *
 * 【保护机制】
 *   避障:       车头碰到东西 → 停车+蜂鸣
 *   丢线有方向: 全白旋转超时(5000帧) → 停车
 *   丢线无方向: 全白干等超时(3000帧) → 停车
 *   找回线缓转: base/2速度续转, 线回中间(x3~x6有黑)才交PID
 *   十字:       8个全黑 → 直行冲过+清PID
 *   出弯复位:   adev>=3回落 → 清PID
 *   入口复位:   按G重进 → 所有状态清零
 */
#include "stm32f10x.h"
#include "stm32f10x_iwdg.h"
#include "Motor.h"
#include "SmartCar.h"
#include "Buzzer.h"
#include "PID.h"
#include "BluetoothCmd.h"
#include "EightWayTrack.h"

#define HIGH (1)
#define LOW  (0)

/* ================================================================
 *  变量定义
 * ================================================================ */

// 旧代码兼容, 不要动
uint8_t Top_L = 1, Top_R = 1;

// PID控制器 — 把偏差变成轮子速度的"大脑"
static PID_Controller line_pid;
static float  prev_kp = 25.0f, prev_ki = 0.15f, prev_kd = 30.0f;

// 电机速度不能超过 ±99, 这个函数负责截断
static int8_t clamp_motor(int16_t val)
{
	if (val >  99) return  99;
	if (val < -99) return -99;
	return (int8_t)val;
}

// 丢线计时器 — 全白了多少帧了
static uint16_t lost_counter;

// ── 老线追踪 ──
// 跨帧追老线质心, 只算靠近上帧质心(±2.5格)的黑探头
// g_line_drift 慢累加漂移(×0.3), 丢线时比单帧快照更可信
static float  g_line_centroid = 3.5f;
static float  g_line_drift    = 0.0f;

// ── 锐角状态机 ──
#define SHARP_IDLE     0   // 空闲, 没有锐角
#define SHARP_DETECTED 1   // 发现锐角, 直行穿过缝隙中
#define SHARP_ROTATE   2   // 穿过了, 原地旋转找另一边

static uint8_t g_sharp_state = SHARP_IDLE;
static int8_t  g_sharp_dir  = 0;    // 锐角旋转方向: +1右 -1左

// ── 丢线旋转相关 ──
#define RECOVER_TIMEOUT 5000      // 有方向时丢线最多转这么久(超时停车)

static int8_t  g_last_dev_dir = 0;  // 探头方向: +1线在右 -1线在左 (锁住时不动)
static int8_t  g_turn_dir  = 0;     // 转弯方向(锁): +1右弯 -1左弯 (P2/锐角记下)
static uint8_t g_turn_ok   = 0;     // 正常直行连满计数, ≥g_turn_lock 解锁
static uint8_t g_turn_pend = 0;     // 锁到期标记, 本帧先报告·到期, 下帧真清零
static uint8_t g_was_sharp = 0;     // 急弯中(adev≥3触发), 线回中间(adev≤1)才清PID
static uint8_t g_was_lost   = 0;    // 上帧丢了线, 找回时清PID+启动缓转
static uint8_t g_turn_hold  = 0;    // 找回线缓转中, 等线回中间再放行PID

/* ================================================================
 *  TR — 统一上报到App
 * ================================================================ */
static void TR(const uint8_t *x, const char *st, uint8_t lock, int8_t L, int8_t R)
{
	BluetoothCmd_ReportTrace(x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7],
	                         Top_L, Top_R,
	                         st, lock, L, R);
}

/* ================================================================
 *  ComputeDeviation — 算出小车偏多少
 *
 *  4个策略从上到下, 谁先命中就用谁:
 *    P1 锐角  → P2 直角  → P4 质心法
 *
 *  返回: [-5.5, +5.5]  负=偏左需右转, 正=偏右需左转
 * ================================================================ */
static float ComputeDeviation(EightWayData d)
{
	uint8_t x1=d.x1,x2=d.x2,x3=d.x3,x4=d.x4,x5=d.x5,x6=d.x6,x7=d.x7,x8=d.x8;

	/* ── 锐角状态机 (P1的延续) ──
	 * ROTATE(旋转中):  中间探头看到线了 → 回到正常, 标记让主循环清PID
	 * DETECTED(穿过中): 全白出现 → 进入旋转; 没全白 → 继续直行冲
	 */
	if (g_sharp_state == SHARP_ROTATE) {
		if (x3==LOW||x4==LOW||x5==LOW||x6==LOW) {
			g_sharp_state = SHARP_IDLE;
			g_was_lost = 1;
		} else {
			return 0.0f;
		}
	}
	if (g_sharp_state == SHARP_DETECTED) {
		if (x1==HIGH&&x2==HIGH&&x3==HIGH&&x4==HIGH&&x5==HIGH&&x6==HIGH&&x7==HIGH&&x8==HIGH) {
			g_sharp_state = SHARP_ROTATE;
			return 0.0f;
		}
		return 0.0f;
	}

	/* ── 全白 / 全黑 → 偏差直接0, 丢线由主循环兜底 ── */
	if (x1==HIGH&&x2==HIGH&&x3==HIGH&&x4==HIGH&&x5==HIGH&&x6==HIGH&&x7==HIGH&&x8==HIGH)
		return 0.0f;
	if (x1==LOW&&x2==LOW&&x3==LOW&&x4==LOW&&x5==LOW&&x6==LOW&&x7==LOW&&x8==LOW)
		return 0.0f;

	/* ═══════ 策略1: 锐角检测 ═══════
	 * 前提: 锁没活跃(g_turn_dir==0), 锐角状态空闲
	 * 判断: 黑探头分成左右两坨, 中间有白缝(gap>=1)
	 * 方向: 看最边缘探头 x1/x2 vs x7/x8, 哪边更极端往哪转
	 *       (不用左右黑总数, 车头角度会影响总数)
	 *       平手时看哪边黑探头更少(更稀疏=新方向)
	 * 结果: 记 g_sharp_dir → 锁 g_turn_dir → 进入DETECTED直行冲
	 */
	if (g_sharp_state == SHARP_IDLE && g_turn_dir == 0)
	{
		uint8_t xs[8] = {x1,x2,x3,x4,x5,x6,x7,x8};
		uint8_t L = 0, R = 0, cnt = 0, i;
		int8_t foundL = 0;
		for (i = 0; i < 8; i++) {
			if (xs[i] == LOW) {
				cnt++;
				if (!foundL) { L = i; foundL = 1; }
				R = i;
			}
		}
		if (cnt >= 2) {
			int8_t gap = (int8_t)(R - L) - (int8_t)(cnt - 1);
			if (gap >= 1) {
				uint8_t le = (x1==LOW)+(x2==LOW);
				uint8_t re = (x7==LOW)+(x8==LOW);
				if      (le > re) g_sharp_dir = -1;
				else if (re > le) g_sharp_dir = +1;
				else {
					int8_t leftB = 0, rightB = 0;
					for (i = 0; i < 4; i++) { if (xs[i] == LOW) leftB++; }
					for (i = 4; i < 8; i++) { if (xs[i] == LOW) rightB++; }
					g_sharp_dir = (leftB < rightB) ? -1 : +1;
				}
				g_sharp_state = SHARP_DETECTED;
				g_turn_dir = g_sharp_dir;
				return 0.0f;
			}
		}
	}

	/* ═══════ 策略2: 直角记忆 ═══════
	 * 左直角: (x1或x2黑) 且 x8白, 总黑探头<4
	 * 右直角: (x7或x8黑) 且 x1白, 总黑探头<4
	 *
	 * 只记方向(g_turn_dir), PID在首次(0→±1)那帧清旧积分
	 * 锁活跃时: 反向P2不覆盖(防转弯中蹭线反转方向)
	 * 后续帧继续走P4质心法, 不直接控制轮子
	 */
	if ((x1==LOW||x2==LOW) && x8==HIGH) {
		int cnt = (x1==LOW)+(x2==LOW)+(x3==LOW)+(x4==LOW)+(x5==LOW)+(x6==LOW)+(x7==LOW)+(x8==LOW);
		if (cnt < 4 && g_turn_dir <= 0) {
			if (g_turn_dir == 0) PID_Reset(&line_pid);
			g_turn_dir = -1;  g_turn_ok = 0;
		}
	}
	if ((x7==LOW||x8==LOW) && x1==HIGH) {
		int cnt = (x1==LOW)+(x2==LOW)+(x3==LOW)+(x4==LOW)+(x5==LOW)+(x6==LOW)+(x7==LOW)+(x8==LOW);
		if (cnt < 4 && g_turn_dir >= 0) {
			if (g_turn_dir == 0) PID_Reset(&line_pid);
			g_turn_dir = +1;  g_turn_ok = 0;
		}
	}

	/* ═══════ 策略4: 质心法 (最常用) ═══════
	 * 所有黑探头的平均位置(0~7) - 中心3.5 = 偏差
	 *
	 * 比如 10001100:
	 *   黑探头: x1(0) x5(4) x6(5) → 平均=3.0
	 *   偏差 = 3.0 - 3.5 = -0.5 (线偏左, 需右转)
	 *
	 * 死区 ±0.20: 线基本在中间就不纠, 省得车来回晃
	 */
	{
		int8_t sum_pos = 0, cnt = 0;
		if (x1==LOW) { sum_pos += 0; cnt++; }
		if (x2==LOW) { sum_pos += 1; cnt++; }
		if (x3==LOW) { sum_pos += 2; cnt++; }
		if (x4==LOW) { sum_pos += 3; cnt++; }
		if (x5==LOW) { sum_pos += 4; cnt++; }
		if (x6==LOW) { sum_pos += 5; cnt++; }
		if (x7==LOW) { sum_pos += 6; cnt++; }
		if (x8==LOW) { sum_pos += 7; cnt++; }
		float centroid = (float)sum_pos / (float)cnt;
		float raw_dev  = (centroid - 3.5f) * 1.0f;
		if (raw_dev <  0.20f && raw_dev > -0.20f) raw_dev = 0.0f;

		// ── 老线追踪: 只算靠近上帧老线质心(±2.5格)的探头 ──
		// 漂移方向慢积累(×0.3), 不受锁冻结
		float ns = 0; int nc = 0; float gc = g_line_centroid;
		float d;
		d = 0 - gc; if (x1==LOW && d > -2.5f && d < 2.5f) { ns += 0; nc++; }
		d = 1 - gc; if (x2==LOW && d > -2.5f && d < 2.5f) { ns += 1; nc++; }
		d = 2 - gc; if (x3==LOW && d > -2.5f && d < 2.5f) { ns += 2; nc++; }
		d = 3 - gc; if (x4==LOW && d > -2.5f && d < 2.5f) { ns += 3; nc++; }
		d = 4 - gc; if (x5==LOW && d > -2.5f && d < 2.5f) { ns += 4; nc++; }
		d = 5 - gc; if (x6==LOW && d > -2.5f && d < 2.5f) { ns += 5; nc++; }
		d = 6 - gc; if (x7==LOW && d > -2.5f && d < 2.5f) { ns += 6; nc++; }
		d = 7 - gc; if (x8==LOW && d > -2.5f && d < 2.5f) { ns += 7; nc++; }
		if (nc >= 1) {
			float new_c = ns / (float)nc;
			g_line_drift += (new_c - g_line_centroid) * 0.3f;
			g_line_centroid = new_c;
		}

		return raw_dev;
	}
}

/* ================================================================
 *  参数同步 — App改了PID参数就自动更新
 * ================================================================ */
static void sync_pid_params(void)
{
	if (g_line_kp != prev_kp || g_line_ki != prev_ki || g_line_kd != prev_kd)
	{
		line_pid.Kp = g_line_kp;
		line_pid.Ki = g_line_ki;
		line_pid.Kd = g_line_kd;
		prev_kp = g_line_kp;
		prev_ki = g_line_ki;
		prev_kd = g_line_kd;
	}
}

/* ================================================================
 *  初始化 — 开机时调用一次
 * ================================================================ */
void LineWalking_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

	GPIO_InitTypeDef g;
	g.GPIO_Mode  = GPIO_Mode_IPU;
	g.GPIO_Speed = GPIO_Speed_50MHz;
	g.GPIO_Pin   = GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOC, &g);

	EightWayTrack_Init();

	PID_Init(&line_pid, g_line_kp, g_line_ki, g_line_kd, -99.0f, 99.0f, 40.0f);
	prev_kp = g_line_kp; prev_ki = g_line_ki; prev_kd = g_line_kd;
}

/* ================================================================
 *  重置 — 按G重进时调用, 所有状态清零
 * ================================================================ */
void LineWalking_Reset(void)
{
	PID_Reset(&line_pid);
	lost_counter = 0;
	g_sharp_state = SHARP_IDLE;
	g_sharp_dir  = 0;
	g_last_dev_dir = 0;
	g_turn_dir = 0;
	g_turn_pend = 0;
	g_turn_ok = 0;
	g_line_centroid = 3.5f;
	g_line_drift    = 0.0f;
	g_was_sharp     = 0;
	g_was_lost      = 0;
	g_turn_hold     = 0;
}

/* ================================================================
 *  Get_LineWalking — 给OLED显示用, 吐出传感器数据和避障状态
 * ================================================================ */
void Get_LineWalking(uint8_t *pL1, uint8_t *pL2,
                     uint8_t *pR1, uint8_t *pR2,
                     uint8_t *pTop_L, uint8_t *pTop_R)
{
	EightWayData ewd = EightWayTrack_Read();
	if (!ewd.ok) return;
	*pL2    = ewd.x1;
	*pL1    = ewd.x4;
	*pR1    = ewd.x5;
	*pR2    = ewd.x8;
	*pTop_L = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15);
	*pTop_R = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_14);
}

/* ================================================================
 *  LineWalking — 循迹主循环, 每帧调用 (~500Hz)
 *
 *  流程:
 *    1-2) 喂狗, 同步App参数, 读传感器
 *    3)   避障检查
 *    4)   计算偏差 (ComputeDeviation)
 *    5)   锐角旋转状态机 (ROTATE → 原地转)
 *    6)   锁到期处理 (g_turn_pend → 本帧真清)
 *    7)   探头方向更新 (锁活跃时冻结, lost_counter一定清)
 *    8)   丢线兜底 (全白 → 优先级旋转找线)
 *        找回后: 清PID + 直角方向缓转(base/2), 等线回中间
 *    9)   十字直行 (全黑冲过)
 *   10)   出弯PID复位 (adev≥3回落)
 *   11)   PID差速 → 驱动电机
 *   12)   锁计数 (三项条件满足才累加)
 *   13)   状态上报 (蓝牙)
 * ================================================================ */
void LineWalking(void)
{
	/* 1-2) 喂看门狗 + 圈速计时 + 同步参数 + 读传感器 */
	IWDG_ReloadCounter();
	BluetoothCmd_TickLap();

	sync_pid_params();
	int8_t base = g_base_speed;

	EightWayData ewd = EightWayTrack_Read();
	if (!ewd.ok) return;

	Top_L = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15);
	Top_R = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_14);

	/* 3) 避障: 车头碰到东西就停车 */
	if (g_obstacle_enable && (Top_L == LOW || Top_R == LOW))
	{
		Car_Stop(); Buzzer_StartAlarm();
		return;
	}
	Buzzer_StopAlarm();

	/* 4) 计算偏差 */
	float dev = ComputeDeviation(ewd);

	/* 5) 锐角旋转状态机 */
	if (g_sharp_state == SHARP_ROTATE) {
		if (++lost_counter > 5000) {
			Car_Stop();
			g_sharp_state = SHARP_IDLE;
			PID_Reset(&line_pid);
			TR(&ewd.x1, "LOST", 0, 0, 0);
			return;
		}
		int8_t rs = g_sharp_rotate_speed;
		if (g_sharp_dir > 0) {
			LeftMotor_Speed(clamp_motor(rs));
			RightMotor_Speed(clamp_motor(-rs));
			TR(&ewd.x1, "ROT_CW", 0, clamp_motor(rs), clamp_motor(-rs));
		} else {
			LeftMotor_Speed(clamp_motor(-rs));
			RightMotor_Speed(clamp_motor(rs));
			TR(&ewd.x1, "ROT_CCW", 0, clamp_motor(-rs), clamp_motor(rs));
		}
		return;
	}

	/* 6) 锁到期: 上帧标记的, 本帧真正清零 */
	if (g_turn_pend) { g_turn_dir = 0;  g_turn_pend = 0; }

	/* 7) 探头方向: 锁活跃时不更新(防转头污染方向), lost_counter一定清 */
	{
		uint8_t aw = (ewd.x1==HIGH && ewd.x2==HIGH && ewd.x3==HIGH && ewd.x4==HIGH &&
		              ewd.x5==HIGH && ewd.x6==HIGH && ewd.x7==HIGH && ewd.x8==HIGH);
		uint8_t ab = (ewd.x1==LOW  && ewd.x2==LOW  && ewd.x3==LOW  && ewd.x4==LOW  &&
		              ewd.x5==LOW  && ewd.x6==LOW  && ewd.x7==LOW  && ewd.x8==LOW );
		if (!aw && !ab) {
			lost_counter = 0;
			if (g_turn_dir == 0) {
				int8_t lc = (ewd.x1==LOW)+(ewd.x2==LOW)+(ewd.x3==LOW);
				int8_t rc = (ewd.x6==LOW)+(ewd.x7==LOW)+(ewd.x8==LOW);
				if      (rc > lc) g_last_dev_dir = +1;
				else if (lc > rc) g_last_dev_dir = -1;
			}
		} else if (ab) {
			lost_counter = 0;
		}
	}

	/* 8) 丢线兜底: 全白了, 按优先级旋转找回
	 *    1. RECT(找转角) — g_turn_dir      P2/锐角锁的方向
	 *    2. LREC(找老线) — g_line_drift    老线漂移方向(跨帧积累)
	 *    3. RECP(找线)   — g_last_dev_dir  探头快照兜底
	 */
	if (ewd.x1==HIGH && ewd.x2==HIGH && ewd.x3==HIGH && ewd.x4==HIGH &&
	    ewd.x5==HIGH && ewd.x6==HIGH && ewd.x7==HIGH && ewd.x8==HIGH)
	{
		g_was_lost = 1;
		if (g_turn_dir != 0 || (g_line_drift < -0.25f || g_line_drift > 0.25f) || g_last_dev_dir != 0) {
			int8_t dir;
			const char *recover_st;
			if (g_turn_dir != 0) {
				dir = g_turn_dir;  recover_st = "RECT";
			} else if (g_line_drift < -0.25f || g_line_drift > 0.25f) {
				dir = (g_line_drift > 0) ? +1 : -1;  recover_st = "LREC";
			} else {
				dir = g_last_dev_dir;  recover_st = "RECP";
			}
			if (++lost_counter > RECOVER_TIMEOUT) {
				Car_Stop();
				PID_Reset(&line_pid);
				g_last_dev_dir = 0;
				g_turn_dir = 0;
				g_turn_pend = 0;
				g_turn_ok = 0;
				g_line_drift = 0.0f;
				lost_counter = 0;
				TR(&ewd.x1, "LOST", 0, 0, 0);
				return;
			}
			int8_t rs = g_sharp_rotate_speed;
			if (dir > 0) {
				LeftMotor_Speed(clamp_motor(rs));
				RightMotor_Speed(clamp_motor(-rs));
				TR(&ewd.x1, recover_st, 0, clamp_motor(rs), clamp_motor(-rs));
			} else {
				LeftMotor_Speed(clamp_motor(-rs));
				RightMotor_Speed(clamp_motor(rs));
				TR(&ewd.x1, recover_st, 0, clamp_motor(-rs), clamp_motor(rs));
			}
			return;
		}
		// 没有任何方向记忆, 干等
		if (++lost_counter > 3000) {
			Car_Stop();
			PID_Reset(&line_pid);
			TR(&ewd.x1, "LOST", 0, 0, 0);
			return;
		}
	}

	// ── 丢线找回后: 清PID残分, 有锁方向就启动缓转 ──
	if (g_was_lost) {
		g_was_lost = 0;
		PID_Reset(&line_pid);
		if (g_turn_dir != 0) g_turn_hold = 1;
	}

	// ── 缓转: base/2慢速继续转, 线回到中间(x3~x6有黑)或超时才放行PID ──
	if (g_turn_hold && g_turn_dir != 0) {
		int8_t rs = base / 2;
		if (g_turn_dir > 0) {
			LeftMotor_Speed(clamp_motor( rs));
			RightMotor_Speed(clamp_motor(-rs));
			TR(&ewd.x1, "RECT", 0, clamp_motor(rs), clamp_motor(-rs));
		} else {
			LeftMotor_Speed(clamp_motor(-rs));
			RightMotor_Speed(clamp_motor( rs));
			TR(&ewd.x1, "RECT", 0, clamp_motor(-rs), clamp_motor(rs));
		}
		if (ewd.x3==LOW||ewd.x4==LOW||ewd.x5==LOW||ewd.x6==LOW)
			g_turn_hold = 0;
		if (++lost_counter > 500) { g_turn_hold = 0; lost_counter = 0; }
		return;
	}

	/* 9) 十字: 暂禁用 — 全黑走正常PID
	if (ewd.x1==LOW && ewd.x2==LOW && ewd.x3==LOW && ewd.x4==LOW &&
	    ewd.x5==LOW && ewd.x6==LOW && ewd.x7==LOW && ewd.x8==LOW)
	{
		LeftMotor_Speed(base); RightMotor_Speed(base);
		PID_Reset(&line_pid);
		TR(&ewd.x1, "CROSS", 0, base, base);
		return;
	}
	*/

	/* 10) 出弯复位: 双阈值滞回, 进弯adev≥3.0 出弯adev≤1.0, 防中间闪现误清PID */
	float adev = (dev > 0) ? dev : -dev;
	if (adev >= 3.0f)
		g_was_sharp = 1;
	else if (adev <= 1.0f && g_was_sharp) {
		g_was_sharp = 0;
		PID_Reset(&line_pid);
	}

	/* 11) PID差速: 偏差 → 纠偏量 → 左右轮速度 → 电机
	 *     左轮 = 基础速度 + 纠偏量   correction>0 → 左轮快 → 左转
	 *     右轮 = 基础速度 - 纠偏量
	 */
	float correction = PID_Compute(&line_pid, dev);
	int16_t left_speed  = (int16_t)base + (int16_t)correction;
	int16_t right_speed = (int16_t)base - (int16_t)correction;

	LeftMotor_Speed(clamp_motor(left_speed));
	RightMotor_Speed(clamp_motor(right_speed));

	/* 12) 锁计数器
	 *     偏差小(adev<1.5) + 线在中间(x3~x6有黑) + 非SHARP_DETECTED
	 *     → g_turn_ok 累加
	 *     任意一条不满足 → g_turn_ok 归零 (锁还在, 重新数)
	 *     累加到阈值 → 标记到期 (本帧 lock_val=2, 下帧真清 g_turn_dir)
	 */
	uint8_t lock_val = (g_turn_dir != 0) ? 1 : 0;
	if (g_sharp_state != SHARP_DETECTED && adev < 1.5f &&
	    (ewd.x3==LOW||ewd.x4==LOW||ewd.x5==LOW||ewd.x6==LOW)) {
		if (++g_turn_ok >= g_turn_lock && g_turn_dir != 0) {
			g_turn_pend = 1;  lock_val = 2;
		}
	} else {
		g_turn_ok = 0;
	}

	/* 13) 状态上报
	 *     lock_val: 0=无锁, 1=锁中, 2=刚到期(·到期)
	 *     st: LINE=直行 / L_SHARP/R_SHARP=锐角 / L_ANGLE/R_ANGLE=大偏差
	 */
	const char *st = "LINE";
	if (g_sharp_state == SHARP_DETECTED)
		st = g_sharp_dir > 0 ? "R_SHARP" : "L_SHARP";
	else if (adev >= 3.0f)
		st = dev > 0 ? "R_ANGLE" : "L_ANGLE";
	TR(&ewd.x1, st, lock_val, clamp_motor(left_speed), clamp_motor(right_speed));
}
