/*
 * 外设库配置文件
 * 
 * 选择需要使用的STM32标准外设库模块。
 * 本项目中已启用：ADC、BKP、CAN、CEC、CRC、DAC、DBGMCU、
 *                 DMA、EXTI、FLASH、FSMC、GPIO、I2C、IWDG、
 *                 PWR、RCC、RTC、SDIO、SPI、TIM、USART、WWDG
 *                 以及 misc（NVIC中断管理）。
 */
/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_conf.h 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Library configuration file.
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
#ifndef __STM32F10x_CONF_H
#define __STM32F10x_CONF_H

/* 以下为启用的外设库头文件（未注释=启用，注释掉=禁用） */
// #include "stm32f10x_adc.h"      // ADC模数转换器
// #include "stm32f10x_bkp.h"      // BKP备份寄存器
// #include "stm32f10x_can.h"      // CAN总线控制器
// #include "stm32f10x_cec.h"      // CEC消费电子控制
// #include "stm32f10x_crc.h"      // CRC循环冗余校验
// #include "stm32f10x_dac.h"      // DAC数模转换器
// #include "stm32f10x_dbgmcu.h"   // 调试MCU
#include "stm32f10x_dma.h"      // DMA直接内存访问
#include "stm32f10x_exti.h"     // EXTI外部中断
#include "stm32f10x_flash.h"    // FLASH闪存
// #include "stm32f10x_fsmc.h"     // FSMC灵活静态存储器控制器
#include "stm32f10x_gpio.h"     // GPIO通用输入输出
#include "stm32f10x_i2c.h"      // I2C总线
#include "stm32f10x_iwdg.h"     // IWDG独立看门狗
#include "stm32f10x_pwr.h"      // PWR电源控制
#include "stm32f10x_rcc.h"      // RCC复位与时钟控制
#include "stm32f10x_rtc.h"      // RTC实时时钟
// #include "stm32f10x_sdio.h"     // SDIO安全数字输入输出
// #include "stm32f10x_spi.h"      // SPI串行外设接口
#include "stm32f10x_tim.h"      // TIM定时器（本项目核心模块）
#include "stm32f10x_usart.h"    // USART串口（蓝牙通信核心模块）
// #include "stm32f10x_wwdg.h"     // WWDG窗口看门狗
#include "misc.h"                // NVIC中断控制器和SysTick配置

/* 取消下面这行的注释将启用参数断言检查（开发调试用） */
/* #define USE_FULL_ASSERT    1 */

#ifdef  USE_FULL_ASSERT
  /* 断言宏：表达式为假时调用assert_failed报告错误位置 */
  #define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t* file, uint32_t line);
#else
  #define assert_param(expr) ((void)0)  // 不启用断言时，什么都不做
#endif /* USE_FULL_ASSERT */

#endif /* __STM32F10x_CONF_H */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
