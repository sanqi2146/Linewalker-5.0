/*
 * OLED显示模块 - 头文件
 * 
 * 功能：通过I2C协议驱动0.96寸OLED显示屏（SSD1306驱动芯片）。
 * 显示尺寸：128x64像素，可显示4行×16列英文字符。
 * 引脚连接：SCL-PB8, SDA-PB9（软件模拟I2C通信）。
 */

#ifndef __OLED_H
#define __OLED_H
#include "stdint.h"

void OLED_Init(void);                                                      // 初始化OLED
void OLED_Clear(void);                                                     // 清屏
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);              // 显示单个字符
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);         // 显示字符串

#endif
