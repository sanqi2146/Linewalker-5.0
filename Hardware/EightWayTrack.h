#ifndef __EIGHT_WAY_TRACK_H
#define __EIGHT_WAY_TRACK_H

#include "stdint.h"

// I2C 从机地址 0x12 (8路巡线模块默认地址)
#define EWT_I2C_ADDR  0x12

// 寄存器 0x30: 8路数字值, 按位排列 bit7=L1(最左) ... bit0=L8(最右)
//  0=检测到黑线(灯亮), 1=白底(灯不亮)
#define EWT_REG_DIGITAL 0x30

// 8路传感器数据结构 (L1=最左, L8=最右)
// 0=压黑, 1=在白底
typedef struct {
	uint8_t x1, x2, x3, x4, x5, x6, x7, x8;
	uint8_t ok; // I2C读成功标志, 1=通信正常, 0=失败(全0可能是假数据)
} EightWayData;

// 初始化 硬件I2C1 (PB6=SCL, PB7=SDA, 100KHz)
void EightWayTrack_Init(void);

// 读寄存器0x30返回8路数字值
EightWayData EightWayTrack_Read(void);

#endif
