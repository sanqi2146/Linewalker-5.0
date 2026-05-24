/*
 * 蓝牙串口通信模块 - 头文件
 * 
 * 功能：使用STM32的USART1与蓝牙模块（如HC-05/HC-06）通信。
 * 配置：波特率9600，8位数据位，1位停止位，无校验。
 * 接收使用中断方式，收到数据后置标志位Serial_RxFlag=1。
 */

#ifndef __SERIAL_H
#define __SERIAL_H
#include "stdint.h"
#include <stdio.h>

void Serial_Init(void);
void Serial_SetBaud(uint32_t baud);     // 运行时切换波特率
void Serial_ClearRxBuf(void);           // 清除接收缓冲区
void Serial_SendByte(uint8_t Byte);
void Serial_SendString(char *String);
uint32_t Serial_Pow(uint32_t x,uint32_t y);
void Serial_SendNumber(uint32_t Number,uint8_t Length);
uint8_t Serial_GetRxFlag(void);
uint8_t Serial_GetRxData(void);

#endif
