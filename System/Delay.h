/*
 * 延时函数模块 - 头文件
 * 
 * 使用SysTick系统定时器实现微秒、毫秒、秒级别的延时。
 */

#ifndef __DELAY_H
#define __DELAY_H

void Delay_us(uint32_t us);  // 微秒延时
void Delay_ms(uint32_t ms);  // 毫秒延时

#endif
