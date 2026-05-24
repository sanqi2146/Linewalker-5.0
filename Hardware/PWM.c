/*
 * PWM模块 - 实现
 * 
 * 使用STM32的TIM2和TIM3定时器产生PWM信号：
 * - TIM2通道2(PA1)→左电机PWM，TIM2通道3(PA2)→右电机PWM，频率20KHz
 * - TIM3通道4(PB1)→舵机PWM，频率50Hz（舵机标准频率周期=20ms）
 * 
 * 频率计算：PWM频率 = 72MHz / (PSC+1) / (ARR+1)
 *   电机PWM: 72M / 36 / 100 = 20KHz  (人耳听不到20KHz以上声音，避免噪音)
 *   舵机PWM: 72M / 72 / 20000 = 50Hz  (舵机要求50Hz)
 */

#include "stm32f10x.h"                  // Device header

void PWM_Init(void)
{
	// 1. 开启TIM2和TIM3的时钟，以及对应GPIO口的时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);  // TIM2挂载在APB1总线
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);  // TIM3挂载在APB1总线
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE); // PA1,PA2在APB2总线
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE); // PB1在APB2总线
	
	// 2. 配置GPIO为复用推挽输出（PWM输出模式）
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;   // 复用推挽输出，只有在这个模式下PWM外设才能控制引脚
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_1|GPIO_Pin_2; // PA1→CH2, PA2→CH3
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;   // 复用推挽输出
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_1;          // PB1→TIM3_CH4
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStructure);
	
	// 3. 选择定时器的时钟源（内部时钟72MHz）
	TIM_InternalClockConfig(TIM2);  // TIM2时钟源=72MHz
	TIM_InternalClockConfig(TIM3);  // TIM3时钟源=72MHz
	
	// 4. 配置TIM2的时基单元（用于电机PWM）
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;     // 时钟不分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up; // 向上计数
	TIM_TimeBaseInitStructure.TIM_Period=100-1;        // ARR自动重装值=100-1，决定PWM分辨率
	TIM_TimeBaseInitStructure.TIM_Prescaler=36-1;      // PSC预分频器=36-1，决定定时器计数频率
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter=0; // 重复计数器=0（不用）
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitStructure);
	
	// 5. 配置TIM3的时基单元（用于舵机PWM）
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period=20000-1;      // ARR=20000-1，使PWM周期=20ms（50Hz）
	TIM_TimeBaseInitStructure.TIM_Prescaler=72-1;      // PSC=72-1，72MHz/72=1MHz计数频率
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter=0;
	TIM_TimeBaseInit(TIM3,&TIM_TimeBaseInitStructure);
	
	// 6. 初始化输出比较单元（OC）—— 配置PWM模式
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);              // 先把结构体里所有成员设为初始值
	TIM_OCInitStructure.TIM_OCMode=TIM_OCMode_PWM1;      // PWM模式1：CNT<CCR时输出有效电平
	TIM_OCInitStructure.TIM_OCPolarity=TIM_OCPolarity_High; // 有效电平为高电平
	TIM_OCInitStructure.TIM_OutputState=TIM_OutputState_Enable; // 使能输出
	TIM_OCInitStructure.TIM_Pulse=0;                     // CCR初始值=0（初始不转）
	TIM_OC2Init(TIM2,&TIM_OCInitStructure);  // TIM2通道2 → 左电机PWM
	TIM_OC3Init(TIM2,&TIM_OCInitStructure);  // TIM2通道3 → 右电机PWM
	TIM_OC4Init(TIM3,&TIM_OCInitStructure);  // TIM3通道4 → 舵机PWM
	
	// 7. 启动定时器
	TIM_Cmd(TIM2,ENABLE);
	TIM_Cmd(TIM3,ENABLE);
}

// 设置左电机PWM占空比（TIM2通道2）
void PWM_SetCompare2(uint16_t Compare)
{
	TIM_SetCompare2(TIM2,Compare);  // Compare就是CCR的值，范围0~99（对应0~100%占空比）
}

// 设置右电机PWM占空比（TIM2通道3）
void PWM_SetCompare3(uint16_t Compare)
{
	TIM_SetCompare3(TIM2,Compare);  // Compare就是CCR的值，范围0~99
}

// 设置舵机PWM脉宽（TIM3通道4）
void PWM_SetCompare4(uint16_t Compare)
{
	TIM_SetCompare4(TIM3,Compare);  // Compare就是CCR的值，范围500~2500对应0~180度
}
