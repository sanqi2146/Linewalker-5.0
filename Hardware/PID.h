/*
 * 通用PID控制器模块 - 头文件
 * 
 * 位置式PID，带抗积分饱和(anti-windup)保护。
 * 可用于循迹误差校正、电机转速控制等场景。
 */

#ifndef __PID_H
#define __PID_H

typedef struct {
	float Kp;            // 比例系数
	float Ki;            // 积分系数
	float Kd;            // 微分系数
	float integral;      // 积分累加值
	float prev_error;    // 上次误差（用于微分计算）
	float out_min;       // 输出下限
	float out_max;       // 输出上限
	float integral_max;  // 积分上限（抗饱和）
} PID_Controller;

/* 初始化PID控制器 */
void PID_Init(PID_Controller *pid,
              float kp, float ki, float kd,
              float out_min, float out_max,
              float integral_max);

/* 计算PID输出，输入参数为当前误差 */
float PID_Compute(PID_Controller *pid, float error);

/* 重置PID状态（积分清零、上次误差清零） */
void PID_Reset(PID_Controller *pid);

#endif
