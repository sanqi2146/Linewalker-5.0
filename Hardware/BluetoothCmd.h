/*
 * 蓝牙命令模块 - 头文件
 * 
 * 功能：
 *   1. PID参数在线调试
 *   2. 循迹状态日志（变化时上报）
 *   3. 传感器原始值实时推送
 *   4. PID误差/修正量曲线数据推送
 *   5. PID参数三档预设
 *   6. 圈速计时
 *   7. LOGON/LOGOFF 等开关控制
 *   8. 避障开关 O=1/0
 *   9. 转弯模拟 TURN_IN/TURN_OUT/TURN_GO
 * 
 * 命令协议（文本格式，\r\n 结尾）：
 *   PID:  KP=15.5   KI=0.3   KD=7.0   SP=45   P=15.5
 *   查询: KP?  KI?  KD?  SP?  PID?  TIME?
 *   预设: PR1(保守)  PR2(默认)  PR3(激进)
 *   开关: LOG=1/0  SENS=1/0  TRC=1/0  O=1/0
 *   转弯: TURN_IN=N  TURN_OUT=N  TURN_GO
 * 
 * 实时推送格式:
 *   S=101011 (L2L1R1R2TLTR) — 约20Hz
 *   E=+0.5,C=-8.2           — 约50Hz
 *   [直行] [右直角] ...      — 变化时
 *   T=12.35s                 — 终点时
 *   STA=1 / STA=0            — 模式切换时
 */

#ifndef __BLUETOOTHCMD_H
#define __BLUETOOTHCMD_H

#include <stdint.h>

// ==================== PID全局参数 ====================
extern float  g_line_kp;
extern float  g_line_ki;
extern float  g_line_kd;
extern int8_t g_base_speed;

// ==================== 避障与转弯参数 ====================
extern uint8_t g_obstacle_enable;
extern int8_t  g_turn_inner;
extern int8_t  g_turn_outer;
extern int8_t  g_sharp_rotate_speed;  // 锐角旋转速度
extern uint16_t g_turn_lock;          // 转弯方向锁定轮数(0~999, 默认150)

// ==================== 日志枚举 ====================
typedef enum {
	LOG_STRAIGHT   = 0,
	LOG_LEFT_SLIGHT,
	LOG_RIGHT_SLIGHT,
	LOG_LEFT_ARC,
	LOG_RIGHT_ARC,
	LOG_LEFT_SHARP,
	LOG_RIGHT_SHARP,
	LOG_CROSS,
	LOG_LOST,
	LOG_FINISH
} LogState;

// ==================== 接口 ====================

// 外部模块可通过此变量触发模式切换
extern uint8_t g_pending_mode;

void BluetoothCmd_Init(void);
int  BluetoothCmd_FeedByte(uint8_t ch);     // 喂字节→文本命令解析
void BluetoothCmd_Log(LogState state);      // 状态日志(去重)
void BluetoothCmd_SendPID(void);            // 发送PID全部值
void BluetoothCmd_SendConfig(void);         // 发送全部机载参数(SET)
void BluetoothCmd_ReportTrace(uint8_t X1,uint8_t X2,uint8_t X3,uint8_t X4,
                              uint8_t X5,uint8_t X6,uint8_t X7,uint8_t X8,
                              uint8_t TL, uint8_t TR,
                              const char *st, uint8_t locked,
                              int8_t L, int8_t R);  // V4.6.0 统一循迹状态上报

// 实时数据上报(每轮循迹调用, 内部节流)
// 八路巡线 X1~X8 (0=黑线,1=白底) + 两路避障 TL/TR (0=障碍)
void BluetoothCmd_ReportSensor(uint8_t X1, uint8_t X2, uint8_t X3, uint8_t X4,
                               uint8_t X5, uint8_t X6, uint8_t X7, uint8_t X8,
                               uint8_t TL, uint8_t TR);
void BluetoothCmd_ReportPID(float error, float correction);

// 圈速：每轮循迹调用一次, 到达终点时上报
void BluetoothCmd_TickLap(void);
void BluetoothCmd_FinishLap(void);

// 转弯模拟执行
void BluetoothCmd_ExecTurn(void);

// 模式变化上报 STA=1/STA=0
void BluetoothCmd_ReportSTA(uint8_t running);

#endif
