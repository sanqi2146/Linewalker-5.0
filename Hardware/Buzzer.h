/*
 * 蜂鸣器模块 - 头文件 (非阻塞版)
 * 
 * Buzzer_Tick() 需在主循环中调用, 约1ms间隔
 * Buzzer_StartAlarm() 启动滴滴报警, Buzzer_StopAlarm() 停止
 */
#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

void Buzzer_Init(void);
void Buzzer(void);               // 短鸣一声(阻塞, 仅开机用)
void Buzzer_Tick(void);          // 非阻塞tick, 主循环调用
void Buzzer_StartAlarm(void);    // 启动滴滴报警
void Buzzer_StopAlarm(void);     // 停止报警

#endif
