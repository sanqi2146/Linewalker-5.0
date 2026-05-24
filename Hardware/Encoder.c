/*
 * 编码器测速模块 - 实现
 * 
 * PA4 左轮 EXTI4_IRQHandler  下降沿中断
 * PA5 右轮 EXTI9_5_IRQHandler 下降沿中断
 * TIM4 用作自由运行us计数器 (1MHz)
 * 4脉冲=42mm, 速度=42000*chunk/dt (um/s→mm/s)
 */
#include "Encoder.h"
#include "Serial.h"
#include <stdio.h>

volatile uint32_t g_left_pulses;
volatile uint32_t g_right_pulses;
volatile uint32_t g_left_t0;       // 本chunk起始TIM4时间戳
volatile uint32_t g_right_t0;
volatile uint8_t  g_left_first;    // 是否已记录第一个脉冲的时间戳
volatile uint8_t  g_right_first;
uint8_t  g_pulse_chunk = 4;
uint32_t g_left_total_mm;
uint32_t g_right_total_mm;
uint32_t g_left_speed_mms;
uint32_t g_right_speed_mms;

void Encoder_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

	GPIO_InitTypeDef g;
	g.GPIO_Mode  = GPIO_Mode_IPU;
	g.GPIO_Pin   = GPIO_Pin_4 | GPIO_Pin_5;
	g.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &g);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource4);
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource5);

	EXTI_InitTypeDef e;
	e.EXTI_Line    = EXTI_Line4;
	e.EXTI_Mode    = EXTI_Mode_Interrupt;
	e.EXTI_Trigger = EXTI_Trigger_Falling;
	e.EXTI_LineCmd = ENABLE;
	EXTI_Init(&e);

	e.EXTI_Line    = EXTI_Line5;
	EXTI_Init(&e);

	NVIC_InitTypeDef n;
	n.NVIC_IRQChannel                   = EXTI4_IRQn;
	n.NVIC_IRQChannelPreemptionPriority = 1;
	n.NVIC_IRQChannelSubPriority        = 2;
	n.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init(&n);

	n.NVIC_IRQChannel = EXTI9_5_IRQn;
	NVIC_Init(&n);

	TIM_TimeBaseInitTypeDef t;
	TIM_TimeBaseStructInit(&t);
	t.TIM_Prescaler = 71;       // 72MHz / 72 = 1MHz = 1us
	t.TIM_Period    = 0xFFFF;   // 16位自动重载
	TIM_TimeBaseInit(TIM4, &t);
	TIM_Cmd(TIM4, ENABLE);

	g_left_first  = 0;
	g_right_first = 0;
}

void Encoder_Process(void)
{
	static uint32_t left_speed_mms, right_speed_mms, left_dt_us, right_dt_us;
	uint32_t now, dt, cnt;
	uint8_t changed = 0;

	__disable_irq();
	cnt  = g_left_pulses;
	g_left_pulses = 0;
	__enable_irq();

	if (cnt >= g_pulse_chunk && g_left_first) {
		now   = TIM_GetCounter(TIM4);
		dt    = (now >= g_left_t0) ? (now - g_left_t0) : (0x10000 - g_left_t0 + now);
		if (dt > 0 && dt < 500000) {
			left_speed_mms  = (uint32_t)(g_pulse_chunk * 42000UL) / dt;
			g_left_speed_mms = left_speed_mms;  // 暴露给外部模块
			left_dt_us     = dt;
			g_left_total_mm += g_pulse_chunk * 42 / 4;
			changed = 1;
		}
		g_left_first = 0;
	} else if (cnt > 0 && !g_left_first) {
		g_left_t0    = TIM_GetCounter(TIM4);
		g_left_first = 1;
		g_left_pulses = cnt;
	}

	__disable_irq();
	cnt  = g_right_pulses;
	g_right_pulses = 0;
	__enable_irq();

	if (cnt >= g_pulse_chunk && g_right_first) {
		now   = TIM_GetCounter(TIM4);
		dt    = (now >= g_right_t0) ? (now - g_right_t0) : (0x10000 - g_right_t0 + now);
		if (dt > 0 && dt < 500000) {
			right_speed_mms  = (uint32_t)(g_pulse_chunk * 42000UL) / dt;
			g_right_speed_mms = right_speed_mms;
			right_dt_us     = dt;
			g_right_total_mm += g_pulse_chunk * 42 / 4;
			changed = 1;
		}
		g_right_first = 0;
	} else if (cnt > 0 && !g_right_first) {
		g_right_t0    = TIM_GetCounter(TIM4);
		g_right_first = 1;
		g_right_pulses = cnt;
	}

	if (changed) {
		char s[32];
		sprintf(s, "SPD=L%lu,%lu,%lu,R%lu,%lu,%lu\r\n",
			left_speed_mms, left_dt_us / 1000, (uint32_t)g_left_total_mm,
			right_speed_mms, right_dt_us / 1000, (uint32_t)g_right_total_mm);
		Serial_SendString(s);
	}
}

void EXTI4_IRQHandler(void)
{
	if (EXTI_GetITStatus(EXTI_Line4) != RESET) {
		g_left_pulses++;
		EXTI_ClearITPendingBit(EXTI_Line4);
	}
}

void EXTI9_5_IRQHandler(void)
{
	if (EXTI_GetITStatus(EXTI_Line5) != RESET) {
		g_right_pulses++;
		EXTI_ClearITPendingBit(EXTI_Line5);
	}
}
