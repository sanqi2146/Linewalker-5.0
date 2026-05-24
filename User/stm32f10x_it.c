/*
 * 中断服务函数 - 实现
 * 
 * 定义Cortex-M3处理器的异常处理函数。
 * 大部分异常处理函数为空或进入死循环（用于捕获严重错误）。
 * 外设中断（如USART1）在其各自的模块文件中实现。
 */
/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

#include "stm32f10x_it.h"

/******************************************************************************/
/*            Cortex-M3 处理器异常处理函数                                    */
/******************************************************************************/

// 不可屏蔽中断（NMI）：由系统硬件严重错误触发
void NMI_Handler(void)
{
}

// 硬件错误中断：由总线错误、内存管理错误等触发，这里进入死循环方便调试
void HardFault_Handler(void)
{
  /* 发生硬件错误时进入死循环，调试时可在此设置断点 */
  while (1)
  {
  }
}

// 内存管理错误中断：访问非法内存地址时触发
void MemManage_Handler(void)
{
  while (1)
  {
  }
}

// 总线错误中断：总线访问错误时触发
void BusFault_Handler(void)
{
  while (1)
  {
  }
}

// 用法错误中断：如除零、无效指令等
void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

// 系统服务调用中断（SVC）：由SVC指令触发，常用于RTOS任务切换
void SVC_Handler(void)
{
}

// 调试监视中断
void DebugMon_Handler(void)
{
}

// 可挂起的系统调用中断（PendSV）：常用于RTOS上下文切换
void PendSV_Handler(void)
{
}

// 系统滴答定时器中断（SysTick）：Delay.c模块使用此中断实现延时
void SysTick_Handler(void)
{
}

/******************************************************************************/
/*                 STM32F10x 外设中断处理函数                                 */
/*  在此处添加外设中断处理函数（PPP_IRQHandler）                               */
/*  可用的外设中断名称请参考启动文件 startup_stm32f10x_xx.s                   */
/******************************************************************************/

// 模板示例：(本项目USART1中断在Serial.c中实现)
/*void PPP_IRQHandler(void)
{
}*/

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
