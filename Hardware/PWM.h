/*
 * PWM模块 - 头文件
 * 
 * 功能：使用STM32的TIM2和TIM3定时器产生PWM信号，用于控制电机转速和舵机角度。
 * PWM频率：TIM2电机PWM=20KHz(人耳听不到), TIM3舵机PWM=50Hz(舵机标准频率)
 */

#ifndef __PWM_H
#define __PWM_H
#include "stdint.h"

void PWM_Init(void);                   // 初始化TIM2和TIM3的PWM输出
void PWM_SetCompare2(uint16_t Compare); // 设置TIM2通道2的CCR值（左电机速度）
void PWM_SetCompare3(uint16_t Compare); // 设置TIM2通道3的CCR值（右电机速度）
void PWM_SetCompare4(uint16_t Compare); // 设置TIM3通道4的CCR值（舵机角度）

#endif
