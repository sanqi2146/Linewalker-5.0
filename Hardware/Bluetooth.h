/*
 * 蓝牙控制模块 - 头文件
 * 
 * 功能：接收蓝牙串口指令，解析后控制小车执行对应的运动模式。
 * 将指令解析和模式切换逻辑从main.c中独立出来，保持主程序整洁。
 * 
 * 蓝牙指令对照表：
 *   0x40(字符@) → 前进        0x41(字符A) → 后退
 *   0x42(字符B) → 停止        0x43(字符C) → 左转
 *   0x44(字符D) → 右转        0x45(字符E) → 顺时针旋转
 *   0x46(字符F) → 逆时针旋转   0x47(字符G) → 循迹模式
 *   0x48(字符H) → 自动避障     0x49(字符I) → 跟随模式
 *   0x50(字符P) → 保留
 */

#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H

#include "stdint.h"

void Bluetooth_Init(void);    // 初始化蓝牙通信（串口1）
void Bluetooth_Process(void); // 检测蓝牙指令并执行对应模式（在主循环中反复调用）

#endif
