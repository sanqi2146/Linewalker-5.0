/*
 * 编码器测速模块 - 头文件
 * 
 * PA4=左轮编码器, PA5=右轮编码器, 下降沿EXTI中断累加脉冲
 * 凑满 g_pulse_chunk 个脉冲后计算时间差 → 速度 mm/s
 * 里程累积: 每 chunk = chunk * 10.5mm (轮周210mm/20格=10.5mm/格)
 */
#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f10x.h"

void Encoder_Init(void);
void Encoder_Process(void);       // 主循环调用: 检查是否凑满chunk→上报SPD=

extern uint8_t  g_pulse_chunk;     // 默认4, 可通过CHUNK=N蓝牙命令修改
extern uint32_t g_left_total_mm;   // 左轮累计里程(mm)
extern uint32_t g_right_total_mm;  // 右轮累计里程(mm)
extern uint32_t g_left_speed_mms;  // 左轮实时速度 mm/s
extern uint32_t g_right_speed_mms; // 右轮实时速度 mm/s

#endif
