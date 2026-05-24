/*
 * 蓝牙命令模块 - 实现
 * 
 * 全部功能: PID调试 / Flash存取 / 状态日志 / 传感器推送 / 误差推送 / 预设 / 圈速
 *  V3.3新增: 避障开关O= / 转弯模拟TURN_ / P=命令
 */

#include "stm32f10x.h"
#include "Serial.h"
#include "Delay.h"
#include "FlashStore.h"
#include "SmartCar.h"
#include "Encoder.h"
#include "BluetoothCmd.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ==================== 全局PID参数 ====================
float  g_line_kp     = 25.0f;
float  g_line_ki     = 0.15f;
float  g_line_kd     = 30.0f;
int8_t g_base_speed  = 55;

// ==================== 避障与转弯全局参数 ====================
uint8_t g_obstacle_enable = 1;
int8_t  g_turn_inner = 25;
int8_t  g_turn_outer = 50;
int8_t  g_sharp_rotate_speed = 55;
uint16_t g_turn_lock = 6;
uint8_t g_pending_mode = 0;  // 文本命令触发的模式切换

// ==================== 命令缓冲 ====================
#define CMD_BUF_SIZE 32
static char   cmd_buf[CMD_BUF_SIZE];
static uint8_t cmd_idx = 0;

// ==================== 开关 ====================
static uint8_t log_enabled    = 1;
static uint8_t sensor_enabled = 1;
static uint8_t trace_enabled  = 0;

// ==================== 实时上报节流 ====================
static uint16_t tick_counter    = 0;

// ==================== 圈速 ====================
static uint32_t lap_ticks  = 0;
static uint8_t  lap_active = 1;

// ==================== 圈速 ====================
static int is_text_cmd_start(uint8_t ch)
{
	if (ch >= '0' && ch <= '9') return 1;
	if (ch == 'K' || ch == 'k') return 1;
	// C/c不再拦截 → C是左转单键, CFG已改名SET
	if (ch == 'T' || ch == 't') return 1;
	if (ch == 'L' || ch == 'l') return 1;
	if (ch == 'S' || ch == 's') return 1;
	if (ch == 'P' || ch == 'p') return 1;
	if (ch == 'R' || ch == 'r') return 1;
	if (ch == 'O' || ch == 'o') return 1;
	if (ch == '?' || ch == '=') return 1;
	return 0;
}

// ==================== 命令执行 ====================
static void load_preset(int n)
{
	switch (n) {
	case 1: g_line_kp=18.0f; g_line_ki=0.30f; g_line_kd=12.0f; g_base_speed=45;
	        g_sharp_rotate_speed=45; g_turn_lock=7; break;
	case 2: g_line_kp=25.0f; g_line_ki=0.15f; g_line_kd=30.0f; g_base_speed=55;
	        g_sharp_rotate_speed=55; g_turn_lock=6; break;
	case 3: g_line_kp=32.0f; g_line_ki=0.20f; g_line_kd=14.0f; g_base_speed=60;
	        g_sharp_rotate_speed=60; g_turn_lock=5; break;
	default: return;
	}
	BluetoothCmd_SendPID();
}

static void sync_to_flash(void)
{
	g_flash_cfg.kp                 = g_line_kp;
	g_flash_cfg.ki                 = g_line_ki;
	g_flash_cfg.kd                 = g_line_kd;
	g_flash_cfg.base_speed         = g_base_speed;
	g_flash_cfg.sharp_rotate_speed = g_sharp_rotate_speed;
	g_flash_cfg.turn_lock          = g_turn_lock;
	FlashStore_Save();
}

#define FW_VERSION "LWv5.0"

static void send_version(void)
{
	Serial_SendString("VER=" FW_VERSION "\r\n");
}

static void cmd_execute(const char *cmd)
{
	float val;

	if      (strncmp(cmd, "KP=", 3) == 0)
		{ val=(float)atof(cmd+3); if(val>0&&val<=300) g_line_kp=val; BluetoothCmd_SendPID(); }
	else if (strncmp(cmd, "P=", 2) == 0)
		{ val=(float)atof(cmd+2); if(val>0&&val<=300) g_line_kp=val; BluetoothCmd_SendPID(); }
	else if (strncmp(cmd, "KI=", 3) == 0)
		{ val=(float)atof(cmd+3); if(val>=0&&val<=100) g_line_ki=val; BluetoothCmd_SendPID(); }
	else if (strncmp(cmd, "KD=", 3) == 0)
		{ val=(float)atof(cmd+3); if(val>=0&&val<=300) g_line_kd=val; BluetoothCmd_SendPID(); }
	else if (strncmp(cmd, "SP=", 3) == 0)
		{ int sp=atoi(cmd+3); if(sp>=20&&sp<=99) g_base_speed=(int8_t)sp; BluetoothCmd_SendPID(); }

	// O=1/O=0: 避障开关
	else if (strncmp(cmd, "O=", 2) == 0)
		{ g_obstacle_enable = (cmd[2]=='1') ? 1 : 0; BluetoothCmd_SendConfig(); }

	// TURN_IN=N: 转弯内轮速度
	else if (strncmp(cmd, "TURN_IN=", 8) == 0)
		{ int v=atoi(cmd+8); if(v>=0&&v<=99) g_turn_inner=(int8_t)v; BluetoothCmd_SendConfig(); }
	// TURN_OUT=N: 转弯外轮速度
	else if (strncmp(cmd, "TURN_OUT=", 9) == 0)
		{ int v=atoi(cmd+9); if(v>=0&&v<=99) g_turn_outer=(int8_t)v; BluetoothCmd_SendConfig(); }
	// RSP=N: 锐角原地旋转速度
	else if (strncmp(cmd, "RSP=", 4) == 0)
		{ int v=atoi(cmd+4); if(v>=0&&v<=99) g_sharp_rotate_speed=(int8_t)v; BluetoothCmd_SendConfig(); }
	// TL=N: 转弯方向锁定轮数 (0~999, 默认10≈20ms, 0=不锁)
	else if (strncmp(cmd, "TL=", 3) == 0)
		{ int v=atoi(cmd+3); if(v>=0&&v<=999) g_turn_lock=(uint16_t)v; BluetoothCmd_SendConfig(); }
	// TURN_GO: 执行转弯模拟 (进入持续转弯模式，按B停止)
	else if (strcmp(cmd, "TURN_GO") == 0)
		{ g_pending_mode = 12; }

	// SAVE: 保存当前参数到Flash
	else if (strcmp(cmd, "SAVE") == 0)
		{ sync_to_flash(); Serial_SendString("SAVED\r\n"); }
	// RST: 恢复出厂默认
	else if (strcmp(cmd, "RST") == 0)
		{ FlashStore_LoadDefaults();
		  g_line_kp=g_flash_cfg.kp; g_line_ki=g_flash_cfg.ki;
		  g_line_kd=g_flash_cfg.kd; g_base_speed=g_flash_cfg.base_speed;
		  g_sharp_rotate_speed=g_flash_cfg.sharp_rotate_speed;
		  g_turn_lock=g_flash_cfg.turn_lock;
		  BluetoothCmd_SendPID(); }

	else if (strncmp(cmd, "LOG=", 4) == 0)
		{ log_enabled=(cmd[4]=='1'); BluetoothCmd_SendConfig(); }
	else if (strncmp(cmd, "SENS=", 5) == 0)
		{ sensor_enabled=(cmd[5]=='1'); BluetoothCmd_SendConfig(); }
	else if (strncmp(cmd, "TRC=", 4) == 0)
		{ trace_enabled=(cmd[4]=='1'); BluetoothCmd_SendConfig(); }
	// CHUNK=N: 编码器脉冲块大小 (2-20)
	else if (strncmp(cmd, "CHUNK=", 6) == 0)
		{ int v=atoi(cmd+6); if(v>=2&&v<=20) { g_pulse_chunk=(uint8_t)v; BluetoothCmd_SendConfig(); } }

	else if (strcmp(cmd, "PR1") == 0) load_preset(1);
	else if (strcmp(cmd, "PR2") == 0) load_preset(2);
	else if (strcmp(cmd, "PR3") == 0) load_preset(3);

	else if (strcmp(cmd, "KP?")==0 || strcmp(cmd,"KP")==0)
		{ Serial_SendString("KP="); Serial_SendNumber((int)(g_line_kp*10),3); Serial_SendString("\r\n"); }
	else if (strcmp(cmd, "KI?")==0 || strcmp(cmd,"KI")==0)
		{ Serial_SendString("KI="); Serial_SendNumber((int)(g_line_ki*100),3); Serial_SendString("\r\n"); }
	else if (strcmp(cmd, "KD?")==0 || strcmp(cmd,"KD")==0)
		{ Serial_SendString("KD="); Serial_SendNumber((int)(g_line_kd*10),3); Serial_SendString("\r\n"); }
	else if (strcmp(cmd, "SP?")==0 || strcmp(cmd,"SP")==0)
		{ Serial_SendString("SP="); Serial_SendNumber(g_base_speed,3); Serial_SendString("\r\n"); }
	else if (strcmp(cmd, "PID?")==0 || strcmp(cmd,"PID")==0)
		{ BluetoothCmd_SendPID(); }
	else if (strncmp(cmd, "TIME?",5)==0 || strcmp(cmd,"TIME")==0)
		{ char b[32]; sprintf(b,"T=%d.%ds\r\n",(int)(lap_ticks/1000),(int)(lap_ticks%1000)/100); Serial_SendString(b); }
	else if (strcmp(cmd,"HELP")==0 || strcmp(cmd,"?")==0)
	{
		Serial_SendString("VER=" FW_VERSION "\r\n");
		Serial_SendString("PID:KP/KI/KD/SP=N  GET:KP?KI?KD?SP?PID?\r\n");
		Serial_SendString("SAVE RST  PRE:PR1 PR2 PR3\r\n");
		Serial_SendString("SW:LOG/SENS/TRC=1|0  LAP:TIME?\r\n");
	}
	else if (strcmp(cmd,"VER?")==0 || strcmp(cmd,"VER")==0)
		{ send_version(); }
	else if (strcmp(cmd, "SET?")==0 || strcmp(cmd, "SET")==0)
		{ BluetoothCmd_SendConfig(); }
	else Serial_SendString("UNKNOWN\r\n");
}

// ==================== 公开接口 ====================

void BluetoothCmd_Init(void)
{
	cmd_idx = 0;
	log_enabled    = 1;
	sensor_enabled = 1;
	trace_enabled  = 0;
	tick_counter   = 0;
	lap_ticks      = 0;
	lap_active     = 1;
	memset(cmd_buf, 0, CMD_BUF_SIZE);

	// 从全局g_flash_cfg加载PID参数（由main.c中的FlashStore_Init()预先读取）
	g_line_kp    = g_flash_cfg.kp;
	g_line_ki    = g_flash_cfg.ki;
	g_line_kd    = g_flash_cfg.kd;
	g_base_speed = g_flash_cfg.base_speed;
	g_sharp_rotate_speed = g_flash_cfg.sharp_rotate_speed;
	g_turn_lock  = g_flash_cfg.turn_lock;

	// 上电报固件版本和全部机载参数
	Delay_ms(300);
	Serial_SendString("VER=" FW_VERSION "\r\n");
	Delay_ms(50);
	BluetoothCmd_SendConfig();
}

int BluetoothCmd_FeedByte(uint8_t ch)
{
	if (cmd_idx == 0) { if (!is_text_cmd_start(ch)) return 0; }
	if (ch == ' ') return 1;
	if (ch == '\r' || ch == '\n') {
		if (cmd_idx == 0) return 1;
		cmd_buf[cmd_idx] = '\0';
		cmd_idx = 0;
		cmd_execute(cmd_buf);
		return 1;
	}
	if (ch == 0x08 || ch == 0x7F) { if (cmd_idx>0) cmd_idx--; return 1; }
	if (cmd_idx < CMD_BUF_SIZE - 1) cmd_buf[cmd_idx++] = (char)ch;
	return 1;
}

void BluetoothCmd_SendPID(void)
{
	char buf[64];
	sprintf(buf, "KP=%.1f,KI=%.2f,KD=%.1f,SP=%d\r\n",
	        g_line_kp, g_line_ki, g_line_kd, g_base_speed);
	Serial_SendString(buf);
}

void BluetoothCmd_SendConfig(void)
{
	char buf[96];
	sprintf(buf, "SET=KP%.1f,KI%.2f,KD%.1f,SP%d,O%d,TIN%d,TOUT%d,RSP%d,TL%d,CH%d,LOG%d,SENS%d,TRC%d,VER=" FW_VERSION "\r\n",
	        g_line_kp, g_line_ki, g_line_kd, g_base_speed,
	        g_obstacle_enable, g_turn_inner, g_turn_outer, g_sharp_rotate_speed, g_turn_lock, g_pulse_chunk,
	        log_enabled, sensor_enabled, trace_enabled);
	Serial_SendString(buf);
}

// V4.6.0 统一循迹状态上报 (替代旧的传感器+锁存+PID分离上报)
// 格式: S=1110011111|STATE|LOCK|L|R
// STATE: LINE/R_ANGLE/L_ANGLE/CROSS/R_SHARP/L_SHARP/ROT_CW/ROT_CCW/RECOVER/LOST
// LOCK: 锁存窗口激活=1
// 去重: 传感器/状态/LOCK 之一变化才发送, 避免重复蓝牙数据
static uint8_t  last_trace[10];
static char     last_st[16];
static uint8_t  last_lock = 0xFF;
static int8_t   last_L, last_R;

void BluetoothCmd_ReportTrace(uint8_t X1,uint8_t X2,uint8_t X3,uint8_t X4,
                              uint8_t X5,uint8_t X6,uint8_t X7,uint8_t X8,
                              uint8_t TL, uint8_t TR,
                              const char *st, uint8_t locked,
                              int8_t L, int8_t R)
{
	if (!log_enabled) return;
	uint8_t arr[10] = {X1,X2,X3,X4,X5,X6,X7,X8,TL,TR};
	if (memcmp(arr, last_trace, 10) == 0 &&
	    strcmp(st, last_st) == 0 &&
	    locked == last_lock &&
	    L == last_L && R == last_R)
		return;
	memcpy(last_trace, arr, 10);
	strncpy(last_st, st, 15); last_st[15] = '\0';
	last_lock = locked;  last_L = L;  last_R = R;
	char buf[56];
	sprintf(buf, "S=%c%c%c%c%c%c%c%c%c%c|%s|%d|%d|%d\r\n",
	        (X1?'1':'0'),(X2?'1':'0'),(X3?'1':'0'),(X4?'1':'0'),
	        (X5?'1':'0'),(X6?'1':'0'),(X7?'1':'0'),(X8?'1':'0'),
	        (TL?'1':'0'),(TR?'1':'0'),
	        st, locked, L, R);
	Serial_SendString(buf);
}

void BluetoothCmd_TickLap(void)
{
	tick_counter++;
	if (lap_active) lap_ticks++;
}

// ==================== 模式变化状态上报 ====================
void BluetoothCmd_ReportSTA(uint8_t running)
{
	if (running)
		Serial_SendString("STA=1\r\n");
	else
		Serial_SendString("STA=0\r\n");
}
