/*
 * 电机驱动模块 - 头文件
 * 
 * 功能：通过TB6612（或类似）电机驱动板控制两个直流电机的速度和方向。
 * 使用PWM调速，控制引脚 PB14(左IN1), PB15(左IN2), PA6(右IN1), PA7(右IN2)
 */

#ifndef __MOTOR_H
#define __MOTOR_H
#include "stdint.h"

void Motor_Init(void);                      // 初始化电机GPIO和PWM
void LeftMotor_Speed(int8_t Speed);         // 设置左电机速度（正=前进, 0=停止, 负=后退）
void RightMotor_Speed(int8_t Speed);        // 设置右电机速度（正=前进, 0=停止, 负=后退）

#endif
