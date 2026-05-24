/*
 * 智能小车 - 三合一主程序（循迹 + 避障 + 蓝牙遥控）
 * 
 * ==================== 启动流程 ====================
 * 1. IWDG 独立看门狗配置（约2秒超时，防死机自动重启）
 * 2. FlashStore_Init() 从Flash(0x0800FC00)读取上次保存的PID参数
 * 3. 硬件初始化顺序:
 *    - OLED 屏幕 (PB8=SCL, PB9=SDA 软件I2C, SSD1306 128x64)
 *    - SmartCar 电机+方向引脚
 *    - Bluetooth 串口初始化(USART1 9600 8N1)
 *    - BluetoothCmd 从Flash同步PID参数到运行变量
 *    - LineWalking 8路I2C巡线传感器
 *    - Buzzer 蜂鸣器(PA8)
 *    - Encoder 编码器(PA4左 PA5右 EXTI中断)
 * 4. HC-05退出AT命令模式 → 进入数据透传
 * 5. OLED显示启动画面 "LineWalker v5.0"
 * 6. 蜂鸣器短鸣确认启动
 * 7. 进入主循环 while(1) { Bluetooth_Process(); }
 * 
 * ==================== 主循环架构 ====================
 * Bluetooth_Process() 每次调用做三件事:
 *   1. dispatch_serial_byte() 取出一个串口字节分发
 *   2. g_pending_mode处理文本命令触发的模式切换
 *   3. execute_mode() 执行当前Num对应的运动模式
 *      - Num=3 停止(默认) → 循环等待App指令, housekeeping持续上报
 *      - Num=8 循迹 → LineWalking() PID控制
 *      - 其他: 前进/后退/左转/右转/旋转/转弯模拟
 * 
 * 硬件平台：STM32F103C8T6 (72MHz, 64KB Flash, 20KB SRAM)
 * 通信方式：HC-05蓝牙SPP透传 → USART1 → 9600 bps
 * ====================================================
 */

#include "stm32f10x.h"          // STM32标准外设库主头文件
#include "stm32f10x_iwdg.h"     // 独立看门狗IWDG驱动
#include "Delay.h"              // SysTick阻塞延时函数(Delay_us/ms/s)
#include "OLED.h"               // SSD1306 OLED 128x64 显示屏
#include "SmartCar.h"           // 小车基础运动控制(前进/后退/转弯)
#include "Bluetooth.h"          // 蓝牙主调度(模式切换+housekeeping)
#include "BluetoothCmd.h"       // 蓝牙文本命令解析(PID调参/状态上报)
#include "FlashStore.h"         // Flash参数持久化(0x0800FC00)
#include "LineWalking.h"        // 循迹PID控制(8路I2C巡线+2路避障)
#include "Buzzer.h"             // 有源蜂鸣器(PA8, 非阻塞报警)
#include "Encoder.h"            // 编码器测速(PA4/PA5 EXTI中断)
#include "Serial.h"             // 串口USART1驱动(环形缓冲区)

// ==================== HC-05蓝牙模块初始化 ====================
// 目的：退出AT命令模式，进入数据透传模式
// HC-05上电默认可能是AT模式(38400波特率)或透传模式(9600)
// 该方法强制用38400波特率发送AT命令退出AT，然后切回9600
static void HC05_ExitAT(void)
{
	Serial_SetBaud(38400);
	Delay_ms(50);

	Serial_SendString("AT\r\n");
	IWDG_ReloadCounter();
	Delay_ms(200);
	Serial_SendString("AT\r\n");
	IWDG_ReloadCounter();
	Delay_ms(200);
	Serial_SendString("AT+RESET\r\n");
	IWDG_ReloadCounter();
	Delay_ms(400);  // 分两段喂狗
	IWDG_ReloadCounter();
	Delay_ms(400);

	Serial_SetBaud(9600);
	Serial_ClearRxBuf();
	Delay_ms(100);
}

// ==================== 主函数入口 ====================
int main(void)
{
	// ------ 0. 配置独立看门狗 (IWDG) ------
	// IWDG由内部40KHz RC振荡器驱动，超时自动复位MCU
	// 所有while/for循环中必须调用 IWDG_ReloadCounter() 喂狗
	// 本配置超时约2秒: 40KHz/64=625Hz, 1250/625=2.0秒
	// 需要开写访问才能配置，配置后立即使能
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);    // 允许配置IWDG寄存器
	IWDG_SetPrescaler(IWDG_Prescaler_64);            // 预分频64: 40KHz→625Hz
	IWDG_SetReload(1250);                            // 重载值1250: 2秒超时
	IWDG_ReloadCounter();                            // 初始喂狗
	IWDG_Enable();                                   // 使能看门狗(不可逆)

	// ------ 1. 从Flash读取保存的PID参数 ------
	// FlashStore_Init() 从0x0800FC00读取FlashConfig结构体到g_flash_cfg
	// 如果是首次上电(Flash全0xFF)，则用出厂默认值初始化
	FlashStore_Init();

	// ------ 2. 依次初始化所有硬件模块 (关中断防半初始化触发ISR)------
	__disable_irq();

	OLED_Init();           // OLED初始化: PB8(SCL)+PB9(SDA), SSD1306
	SmartCar_Init();       // 电机方向引脚(PB14/15,PA6/7)+PWM初始化

	Bluetooth_Init();      // USART1串口初始化(9600 8N1)
	HC05_ExitAT();         // 强制HC-05退出AT模式进入透传, 防波特率混乱→乱码
	BluetoothCmd_Init();   // 命令模块初始化: 从g_flash_cfg加载PID到g_line_*
	                       // 内部有300ms延时用于等待HC-05稳定
	IWDG_ReloadCounter();  // 喂狗

	LineWalking_Init();    // 循迹传感器 + I2C1硬件初始化
	                       // PID控制器初始参数加载
	Buzzer_Init();         // 蜂鸣器初始化: PA8推挽输出
	Encoder_Init();        // 编码器初始化: PA4/PA5 EXTI中断+TIM4 1MHz计时

	__enable_irq();        // 全部外设就绪, 开中断

	// ------ 3. 启动画面 + 自检 ------
	OLED_ShowString(1, 1, "LineWalker v5.0");      // OLED第1行显示版本
	OLED_ShowString(2, 1, "FlashLoad OK");         // OLED第2行: Flash读取成功
	IWDG_ReloadCounter();                          // 喂狗
	Delay_ms(800);                                 // 展示启动画面800ms
	OLED_Clear();                                  // 清除OLED准备进入界面
	Buzzer();                                      // 蜂鸣器短鸣一声确认启动

	// 丢弃启动期间HC-05可能产生的垃圾字节(含硬件DR残留)
	while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
		(void)USART_ReceiveData(USART1);
	Serial_ClearRxBuf();

	// ------ 4. 主循环 ------
	// 开机默认 Num=3 (停止状态)，小车静止等待App连接
	// Bluetooth_Process() 内部负责:
	//   - 读取串口数据并分发(文本命令或单字节模式切换)
	//   - 执行当前模式(停止/前进/后退/转弯/循迹等)
	//   - 全局housekeeping(传感器上报+编码器+蜂鸣器报警)
	while (1)
	{
		Bluetooth_Process();
	}
}
