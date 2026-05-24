/*
 * 延时函数模块 - 实现
 * 
 * 使用Cortex-M3内核的SysTick系统滴答定时器实现精确延时。
 * 原理：
 *   SysTick是一个24位递减定时器，时钟源=HCLK=72MHz
 *   Delay_us: 设置重载值为 72*xus，等待计数归零（72MHz ÷ 72 = 1us）
 *   Delay_ms: 调用1000次Delay_us
 *   Delay_s:  调用1000次Delay_ms
 * 
 * 注意：此方式延时期间CPU被占用（阻塞式延时），不适用于多任务场景。
 */

#include "stm32f10x.h"

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~233015
  * @retval 无
  */
void Delay_us(uint32_t xus)
{
	SysTick->LOAD = 72 * xus;				// 设置定时器重装值（72MHz时钟，72个周期=1us）
	SysTick->VAL = 0x00;					// 清空当前计数值
	SysTick->CTRL = 0x00000005;				// 设置时钟源为HCLK，启动定时器
	while(!(SysTick->CTRL & 0x00010000));	// 等待计数到0（COUNTFLAG标志位置位）
	SysTick->CTRL = 0x00000004;				// 关闭定时器
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t xms)
{
	while(xms--)
	{
		Delay_us(1000);  // 1ms = 1000us
	}
}

