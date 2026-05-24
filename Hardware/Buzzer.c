/*
 * 蜂鸣器模块 - 实现 (非阻塞版)
 * 
 * 有源蜂鸣器PA8推挽输出, 高电平响
 * 报警模式: 200ms响/200ms停循环, 直到Buzzer_StopAlarm()
 */
#include "stm32f10x.h"
#include "Delay.h"

static uint8_t  alarm_on;
static uint16_t tick;

void Buzzer_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef g;
	g.GPIO_Mode  = GPIO_Mode_Out_PP;
	g.GPIO_Pin   = GPIO_Pin_8;
	g.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &g);
	GPIO_ResetBits(GPIOA, GPIO_Pin_8);
	alarm_on = 0;
	tick     = 0;
}

void Buzzer(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_8);
	Delay_ms(500);
	GPIO_ResetBits(GPIOA, GPIO_Pin_8);
	Delay_ms(500);
}

void Buzzer_StartAlarm(void) { if (!alarm_on) { alarm_on = 1; tick = 0; } }
void Buzzer_StopAlarm(void)  { alarm_on = 0; GPIO_ResetBits(GPIOA, GPIO_Pin_8); }

// 主循环每~1ms调用一次
void Buzzer_Tick(void)
{
	if (!alarm_on) return;
	if (++tick < 200) { GPIO_SetBits(GPIOA, GPIO_Pin_8); }
	else if (tick < 400) { GPIO_ResetBits(GPIOA, GPIO_Pin_8); }
	else { tick = 0; }
}
