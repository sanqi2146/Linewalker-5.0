/*
 * 循迹模块 v3 - 头文件
 * 
 * 4路对地红外传感器，从左到右：L2—L1—R1—R2
 *   L1/R1：居中紧靠，用于正常寻线
 *   L2/R2：外侧稍远，用于识别急弯和十字路口
 * 
 * 检测：上拉输入，压黑线=0(LOW)，在白底=1(HIGH)
 * 
 * v3改进：PID控制 + 十字直行 + 急弯锁死
 */

#ifndef _LINEWALKING_H
#define _LINEWALKING_H

#include "stdint.h"

void LineWalking_Init(void);  // 初始化传感器引脚
void LineWalking_Reset(void); // 重置PID+偏差+丢线计数(新循迹回合)
void Get_LineWalking(uint8_t *L1, uint8_t *L2, uint8_t *R1, uint8_t *R2,
                     uint8_t *Top_L, uint8_t *Top_R);  // 读取所有传感器
void LineWalking(void);       // 循迹主逻辑

#endif
