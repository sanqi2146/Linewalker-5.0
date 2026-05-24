/*
 * 中断服务函数 - 头文件
 * 
 * 声明STM32的异常处理函数和中断处理函数。
 * 本项目中主要使用的用户中断：
 *   USART1_IRQHandler - 蓝牙串口接收中断（在Serial.c中实现）
 */
/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.h 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   This file contains the headers of the interrupt handlers.
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

/* 防止头文件被重复包含 */
#ifndef __STM32F10x_IT_H
#define __STM32F10x_IT_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* 包含STM32标准外设库头文件 */
#include "stm32f10x.h"

/* 以下为Cortex-M3内核异常处理函数声明 */

void NMI_Handler(void);           // 不可屏蔽中断处理
void HardFault_Handler(void);     // 硬件错误中断处理
void MemManage_Handler(void);     // 内存管理错误中断处理
void BusFault_Handler(void);      // 总线错误中断处理
void UsageFault_Handler(void);    // 用法错误中断处理
void SVC_Handler(void);           // 系统服务调用中断处理
void DebugMon_Handler(void);      // 调试监视中断处理
void PendSV_Handler(void);        // 可挂起系统调用中断处理
void SysTick_Handler(void);       // 系统滴答定时器中断处理（Delay.c使用）

#ifdef __cplusplus
}
#endif

#endif /* __STM32F10x_IT_H */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
