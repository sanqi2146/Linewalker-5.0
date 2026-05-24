/*
 * FLASH参数存储模块 - 头文件
 * 
 * 使用STM32F103C8T6内部FLASH最后一页(0x0800FC00)存储PID参数。
 * 断电不丢失，上电自动恢复上次调试好的参数。
 */

#ifndef __FLASHSTORE_H
#define __FLASHSTORE_H

#include "stdint.h"

// 存储的参数结构体（16字节，对齐Flash半字写入）
typedef struct {
	float  kp;                // 比例系数
	float  ki;                // 积分系数
	float  kd;                // 微分系数
	int8_t base_speed;        // 基准速度
	int8_t sharp_rotate_speed;// 转弯旋转速度
	uint16_t turn_lock;       // 锁存轮数
} FlashConfig;

extern FlashConfig g_flash_cfg;  // 全局配置（从Flash读取或默认值）

void FlashStore_Init(void);             // 开机读取Flash参数→写入g_flash_cfg
void FlashStore_Save(void);             // 将全局PID写入Flash保存
void FlashStore_LoadDefaults(void);     // 恢复出厂默认值

#endif
