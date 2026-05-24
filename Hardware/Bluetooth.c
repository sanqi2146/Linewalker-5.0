/*
 * 蓝牙控制模块 - 实现
 * 
 * 通过USART1串口接收数据，分两路处理：
 *   A. 文本命令(如KP=12.0) → BluetoothCmd_FeedByte() → PID调试/查询
 *   B. 单字节模式切换(如@ABCDEFG) → 传统模式切换
 * 
 * 核心函数Bluetooth_Process()在主循环中反复调用。
 *  V3.3: 模式切换时上报STA=1/STA=0
 */

#include "stm32f10x.h"
#include "stm32f10x_iwdg.h"
#include "Serial.h"
#include "OLED.h"
#include "Motor.h"
#include "SmartCar.h"
#include "LineWalking.h"
#include "Buzzer.h"
#include "Encoder.h"
#include "BluetoothCmd.h"
#include "EightWayTrack.h"

static uint8_t Num = 3;  // 开机默认停止

// PB12轻触开关: 按下=启动G(循迹), 再按=停止B
static uint8_t btn_last = 1;       // 上帧电平
static uint16_t btn_debounce = 0;  // 消抖计数器

void Bluetooth_Init(void)
{
	Serial_Init();

	// PB12 → 轻触开关(IPU上拉, 按下=0)
	GPIO_InitTypeDef g;
	g.GPIO_Mode  = GPIO_Mode_IPU;
	g.GPIO_Speed = GPIO_Speed_50MHz;
	g.GPIO_Pin   = GPIO_Pin_12;
	GPIO_Init(GPIOB, &g);
}

// 将单个模式切换字节解析为Num
static void parse_mode_byte(uint8_t ch)
{
	uint8_t prev = Num;
	if(ch==0x40)    { Num=1; }   // '@' 前进
	if(ch==0x41)    { Num=2; }   // 'A' 后退
	if(ch==0x42)    { Num=3; }   // 'B' 停止
	if(ch==0x43)    { Num=4; }   // 'C' 左转
	if(ch==0x44)    { Num=5; }   // 'D' 右转
	if(ch==0x45)    { Num=6; }   // 'E' 顺时针旋转
	if(ch==0x46)    { Num=7; }   // 'F' 逆时针旋转
	if(ch==0x47)    { Num=8; }   // 'G' 循迹
	if(ch==0x48)    { Num=9; }   // 'H' 避障
	if(ch==0x49)    { Num=10; }  // 'I' 跟随
	if(ch==0x50)    { Num=11; }  // 'P' 保留
	if (Num != prev) {
		if (Num == 3) BluetoothCmd_ReportSTA(0);
		else BluetoothCmd_ReportSTA(1);
	}
}

// 读取一个串口字节并分发处理
static int dispatch_serial_byte(void)
{
	if (!Serial_GetRxFlag()) return 0;

	uint8_t ch = Serial_GetRxData();

	if (BluetoothCmd_FeedByte(ch))
		return 0;
	parse_mode_byte(ch);
	return 1;
}

// Housekeeping: 所有模式循环里都要调
// 负责传感器全局上报(不论模式) + 编码器 + 蜂鸣器 + 心跳 + PB12按钮
static void housekeeping(void)
{
	static uint16_t hb_cnt = 0;  // 心跳计数器

	Buzzer_Tick();
	Encoder_Process();

	// PB12轻触开关: 按下→启动/停止循迹
	{
		uint8_t btn_now = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12);
		if (btn_now == btn_last) {
			if (++btn_debounce >= 20) {  // 20ms稳定
				if (btn_debounce == 20 && btn_now == 0) {  // 下降沿
					if (Num == 8) {  // 循迹中→停止
						Car_Stop();
						Num = 3;
						BluetoothCmd_ReportSTA(0);
					} else {         // 停止/其他→启动循迹
						BluetoothCmd_ReportSTA(1);
						Num = 8;
					}
				}
			}
		} else {
			btn_debounce = 0;
			btn_last = btn_now;
		}
	}

	// 心跳: 约500ms一次, HB=1运行中 HB=0静止
	if (++hb_cnt >= 500) {
		hb_cnt = 0;
		Serial_SendString(Num == 3 ? "HB=0\r\n" : "HB=1\r\n");
	}
}

// 执行当前Num对应的运动模式
static void execute_mode(void)
{
	switch(Num)
	{
		case 1:  // 前进
		OLED_ShowString(1,1,"               ");
		OLED_ShowString(3,1,"Status:        ");
		OLED_ShowString(4,1,"Move_Forward   ");
		OLED_ShowString(2,1,"               ");
		Move_Forward();
		break;

		case 2:  // 后退
		OLED_ShowString(1,1,"               ");
		OLED_ShowString(3,1,"Status:        ");
		OLED_ShowString(4,1,"Move_Backward  ");
		OLED_ShowString(2,1,"               ");
		Move_Backward();
		break;

		case 3:  // 停止 — 默认状态, 显示PID, 可接受App调参
		{
		OLED_ShowString(1,1,"STOP           ");
		OLED_ShowString(2,1,"Kp:");
		OLED_ShowString(3,1,"Ki:");
		OLED_ShowString(4,1,"Kd:SP:");
		Car_Stop();
		uint16_t oled_cnt = 0;
		char buf[17];
		while(Num == 3)
		{
			housekeeping();
			IWDG_ReloadCounter();
			if (dispatch_serial_byte()) {Car_Stop();break;}
			if (++oled_cnt > 200)
			{
				oled_cnt = 0;
				sprintf(buf, "Kp=%.1f      ", g_line_kp);
				OLED_ShowString(2, 1, buf);
				sprintf(buf, "Ki=%.2f     ", g_line_ki);
				OLED_ShowString(3, 1, buf);
				sprintf(buf, "Kd=%.1f SP=%d", g_line_kd, g_base_speed);
				OLED_ShowString(4, 1, buf);
			}
		}
		}
		break;

		case 4:  // 左转
		OLED_ShowString(1,1,"              ");
		OLED_ShowString(3,1,"Status:       ");
		OLED_ShowString(4,1,"Turn_Lef      ");
		OLED_ShowString(2,1,"              ");
		Turn_Left(50);
		break;

		case 5:  // 右转
		OLED_ShowString(1,1,"               ");
		OLED_ShowString(3,1,"Status:        ");
		OLED_ShowString(4,1,"Turn_Right     ");
		OLED_ShowString(2,1,"               ");
		Turn_Right(50);
		break;

		case 6:  // 顺时针180°旋转（咬线自停）
		OLED_ShowString(1,1,"               ");
		OLED_ShowString(3,1,"Status:        ");
		OLED_ShowString(4,1,"Rotate180_CW   ");
		OLED_ShowString(2,1,"               ");
		Rotate180_Clockwise();
		Num = 3;  // 旋转完毕,切回停止
		break;

		case 7:  // 逆时针180°旋转（咬线自停）
		OLED_ShowString(1,1,"               ");
		OLED_ShowString(3,1,"Status:        ");
		OLED_ShowString(4,1,"Rotate180_CCW  ");
		OLED_ShowString(2,1,"               ");
		Rotate180_CounterClockwise();
		Num = 3;  // 旋转完毕,切回停止
		break;

		case 8:  // 循迹模式（PID控制+日志上报）
		{
		OLED_ShowString(1,1,"               ");
		OLED_ShowString(2,1,"Kp:");
		OLED_ShowString(3,1,"Ki:");
		OLED_ShowString(4,1,"Kd:SP:");
		LineWalking_Reset();  // 每轮新循迹清零PID状态
		uint16_t oled_cnt = 0;
		char buf[17];
		while(Num == 8)
		{
			housekeeping();
			LineWalking();
			if (dispatch_serial_byte()) {Car_Stop();break;}

			// 每~200次刷新OLED PID显示
			if (++oled_cnt > 200)
			{
				oled_cnt = 0;
				sprintf(buf, "Kp=%.1f      ", g_line_kp);
				OLED_ShowString(2, 1, buf);
				sprintf(buf, "Ki=%.2f      ", g_line_ki);
				OLED_ShowString(3, 1, buf);
				sprintf(buf, "Kd=%.1f SP=%d", g_line_kd, g_base_speed);
				OLED_ShowString(4, 1, buf);
			}
		}
		}
		break;

		case 9:   // 避障模式 — 已废弃
		case 10:  // 跟随模式 — 已废弃
		{
			OLED_ShowString(1,1,"NOT AVAILABLE  ");
			OLED_ShowString(2,1,"Mode Removed   ");
			OLED_ShowString(3,1,"              ");
			OLED_ShowString(4,1,"              ");
			while(Num == 9 || Num == 10)
			{
				housekeeping();
				IWDG_ReloadCounter();
				LineWalking();
				if (dispatch_serial_byte()) {Car_Stop();break;}
			}
		}
		break;

		case 12:  // 转弯模拟 — 持续差速旋转，按B停止
		{
		char buf[17];
		sprintf(buf, "Turn In=%-3d    ", g_turn_inner);
		OLED_ShowString(1,1, buf);
		sprintf(buf, "Out=%-3d       ", g_turn_outer);
		OLED_ShowString(2,1, buf);
		OLED_ShowString(3,1, "B to stop      ");
		OLED_ShowString(4,1, "               ");
		while(Num == 12)
		{
			housekeeping();
			IWDG_ReloadCounter();
			LeftMotor_Speed(g_turn_inner);
			RightMotor_Speed(g_turn_outer);
			if (dispatch_serial_byte()) {Car_Stop(); break;}
		}
		}
		break;

		default:
		break;
	}
}

/*
 * 蓝牙处理主函数——在主循环中反复调用
 * 
 * 每次调用：
 *   1. 读串口数据 → 分发到文本命令 或 模式切换
 *   2. 全局传感器上报 (不论模式)
 *   3. 执行当前模式
 */
void Bluetooth_Process(void)
{
	IWDG_ReloadCounter();
	dispatch_serial_byte();
	if (g_pending_mode) { Num = g_pending_mode; g_pending_mode = 0; }
	execute_mode();
}
