/*
 * 通用PID控制器模块 - 实现
 * 
 * 位置式PID：output = Kp*error + Ki*∫error + Kd*Δerror
 * 
 * 抗积分饱和：积分项累加时会被clamp到[-integral_max, +integral_max]，
 * 防止长时间偏差导致积分累积过大（windup现象）。
 */

#include "PID.h"
#include "stdint.h"

// 初始化PID参数
void PID_Init(PID_Controller *pid,
              float kp, float ki, float kd,
              float out_min, float out_max,
              float integral_max)
{
	pid->Kp           = kp;
	pid->Ki           = ki;
	pid->Kd           = kd;
	pid->integral     = 0.0f;
	pid->prev_error   = 0.0f;
	pid->out_min      = out_min;
	pid->out_max      = out_max;
	pid->integral_max = integral_max;
}

// 计算PID输出
// error = 目标值 - 当前值（循迹场景：0 - 位置偏差 = -偏差）
// 返回值被clamp到[out_min, out_max]
float PID_Compute(PID_Controller *pid, float error)
{
	float p_out, i_out, d_out, output;

	p_out = pid->Kp * error;
	d_out = pid->Kd * (error - pid->prev_error);
	pid->prev_error = error;

	// 条件抗积分饱和：P+D 已趋近饱和边界且误差推向同方向 → 不累加积分
	float pd_sum = p_out + d_out;
	if (!((pd_sum >= pid->out_max && error > 0) || (pd_sum <= pid->out_min && error < 0)))
	{
		pid->integral += error;
		if (pid->integral >  pid->integral_max) pid->integral =  pid->integral_max;
		if (pid->integral < -pid->integral_max) pid->integral = -pid->integral_max;
	}
	i_out = pid->Ki * pid->integral;

	output = pd_sum + i_out;
	if (output > pid->out_max) output = pid->out_max;
	if (output < pid->out_min) output = pid->out_min;

	return output;
}

// 重置PID状态（过十字路口、切换模式时调用）
void PID_Reset(PID_Controller *pid)
{
	pid->integral   = 0.0f;
	pid->prev_error = 0.0f;
}
