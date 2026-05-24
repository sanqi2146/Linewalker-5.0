/*
 * 电机驱动模块 - 实现
 * 
 * ==================== 硬件接线 ====================
 * 使用TB6612双路电机驱动板控制两个直流减速电机:
 * 
 *   左电机: PB14=IN1  PB15=IN2  PA1=TIM2_CH2(PWM调速)
 *   右电机: PA6 =IN1  PA7 =IN2  PA2=TIM2_CH3(PWM调速)
 * 
 * TB6612 真值表 (以单路为例):
 *   IN1=0 IN2=1 → 正转(前进)
 *   IN1=1 IN2=0 → 反转(后退)
 *   IN1=1 IN2=1 → 刹车/停止
 *   IN1=0 IN2=0 → 滑行/停止
 * 
 * ==================== PWM频率 ====================
 * TIM2: PSC=36-1, ARR=100-1 → 72MHz/36/100 = 20KHz
 * 20KHz超出人耳可听范围(20Hz~20KHz)，电机运转时无高频啸叫
 * Speed参数范围 -99~+99 对应 PWM占空比 0~99%
 */

#include "stm32f10x.h"   // STM32标准外设库
#include "PWM.h"         // PWM模块 (TIM2_CH2, TIM2_CH3)

// ==================== 初始化 ====================
void Motor_Init(void)
{
	// 开启 PA 和 PB 端口的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;  // 推挽输出: 可直接驱动TB6612逻辑输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  // 50MHz输出速率

	// 左电机方向引脚: PB14(IN1) + PB15(IN2)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	// 右电机方向引脚: PA6(IN1) + PA7(IN2)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// PWM初始化: TIM2_CH2(PA1左电机) + TIM2_CH3(PA2右电机)
	PWM_Init();
}

// ==================== 左电机速度控制 ====================
void LeftMotor_Speed(int8_t Speed)
{
	if (Speed > 0)  // 正转 = 前进
	{
		GPIO_ResetBits(GPIOB, GPIO_Pin_14);  // PB14(IN1) = 0
		GPIO_SetBits(GPIOB, GPIO_Pin_15);    // PB15(IN2) = 1  → H桥: 正转
		PWM_SetCompare2(Speed);              // PA1 TIM2_CH2 输出PWM
	}
	else if (Speed == 0)  // 停止 = 刹车
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_14);    // PB14(IN1) = 1
		GPIO_SetBits(GPIOB, GPIO_Pin_15);    // PB15(IN2) = 1  → H桥: 刹车
		PWM_SetCompare2(Speed);              // 占空比=0
	}
	else  // 负值 = 后退
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_14);    // PB14(IN1) = 1
		GPIO_ResetBits(GPIOB, GPIO_Pin_15);  // PB15(IN2) = 0  → H桥: 反转
		PWM_SetCompare2(-Speed);             // 取绝对值作为PWM占空比
	}
}

// ==================== 右电机速度控制 ====================
void RightMotor_Speed(int8_t Speed)
{
	if (Speed > 0)  // 正转 = 前进
	{
		GPIO_ResetBits(GPIOA, GPIO_Pin_6);   // PA6(IN1) = 0
		GPIO_SetBits(GPIOA, GPIO_Pin_7);     // PA7(IN2) = 1  → H桥: 正转
		PWM_SetCompare3(Speed);              // PA2 TIM2_CH3 输出PWM
	}
	else if (Speed == 0)  // 停止 = 刹车
	{
		GPIO_SetBits(GPIOA, GPIO_Pin_6);     // PA6(IN1) = 1
		GPIO_SetBits(GPIOA, GPIO_Pin_7);     // PA7(IN2) = 1  → H桥: 刹车
		PWM_SetCompare3(Speed);              // 占空比=0
	}
	else  // 负值 = 后退
	{
		GPIO_SetBits(GPIOA, GPIO_Pin_6);     // PA6(IN1) = 1
		GPIO_ResetBits(GPIOA, GPIO_Pin_7);   // PA7(IN2) = 0  → H桥: 反转
		PWM_SetCompare3(-Speed);             // 取绝对值作为PWM占空比
	}
}
