/*
 * 智能小车运动控制 - 头文件
 * 
 * 功能：封装小车的基本运动指令，通过调用电机驱动层实现各种运动模式。
 * 运动模式包括：前进、后退、停止、左转、右转、顺时针旋转、逆时针旋转。
 * 两个轮子通过差速方式实现转向和旋转。
 */

#ifndef __SMARTCAR_H
#define __SMARTCAR_H

void SmartCar_Init(void);                 // 初始化小车（调用电机初始化）
void Move_Forward(void);                  // 前进：两轮同向正转
void Move_Backward(void);                 // 后退：两轮同向反转
void Car_Stop(void);                      // 停止：两轮速度为0
void Turn_Left(int8_t Speed);             // 左转：左轮停，右轮转（速度可调）
void Turn_Right(int8_t Speed);            // 右转：右轮停，左轮转（速度可调）
void Rotate180_Clockwise(void);           // 顺时针转180° — 咬到黑线自停
void Rotate180_CounterClockwise(void);    // 逆时针转180° — 咬到黑线自停

#endif
