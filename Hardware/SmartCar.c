/*
 * 智能小车运动控制 - 实现
 */

#include "stm32f10x.h"
#include "Motor.h"
#include "LineWalking.h"
#include "Buzzer.h"
#include "Serial.h"
#include "stm32f10x_iwdg.h"

void SmartCar_Init(void)
{
	Motor_Init();
}

void Move_Forward(void)
{
	LeftMotor_Speed(40);
	RightMotor_Speed(40);
}

void Move_Backward(void)
{
	LeftMotor_Speed(-50);
	RightMotor_Speed(-50);
}

void Car_Stop(void)
{
	LeftMotor_Speed(0);
	RightMotor_Speed(0);
	Buzzer_StopAlarm();
}

void Turn_Left(int8_t Speed)
{
	LeftMotor_Speed(0);
	RightMotor_Speed(Speed);
}

void Turn_Right(int8_t Speed)
{
	LeftMotor_Speed(Speed);
	RightMotor_Speed(0);
}

/*
 * 顺时针原地旋转 — 直到内侧传感器咬到黑线后自动停止
 * 途中收到'B'立即停止
 */
void Rotate180_Clockwise(void)
{
	uint8_t l1,l2,r1,r2,tl,tr;
	uint16_t i;

	// 阶段1: 等待离开黑线
	for (i=0; i<3000; i++) {
		IWDG_ReloadCounter();
		Get_LineWalking(&l1,&l2,&r1,&r2,&tl,&tr);
		LeftMotor_Speed(40);
		RightMotor_Speed(-40);
		// 收到'B'立即停止
		if (Serial_GetRxFlag()) { uint8_t ch=Serial_GetRxData(); if(ch==0x42){Car_Stop();return;} }
		if (l1==1 && r1==1) break;
	}

	// 阶段2: 旋转直到咬到黑线
	for (i=0; i<10000; i++) {
		IWDG_ReloadCounter();
		Get_LineWalking(&l1,&l2,&r1,&r2,&tl,&tr);
		LeftMotor_Speed(40);
		RightMotor_Speed(-40);
		if (Serial_GetRxFlag()) { uint8_t ch=Serial_GetRxData(); if(ch==0x42){Car_Stop();return;} }
		if (l1==0 || r1==0) {Car_Stop(); return;}
	}
	Car_Stop();
}

void Rotate180_CounterClockwise(void)
{
	uint8_t l1,l2,r1,r2,tl,tr;
	uint16_t i;

	// 阶段1: 等待离开黑线
	for (i=0; i<3000; i++) {
		IWDG_ReloadCounter();
		Get_LineWalking(&l1,&l2,&r1,&r2,&tl,&tr);
		LeftMotor_Speed(-40);
		RightMotor_Speed(40);
		if (Serial_GetRxFlag()) { uint8_t ch=Serial_GetRxData(); if(ch==0x42){Car_Stop();return;} }
		if (l1==1 && r1==1) break;
	}

	// 阶段2: 旋转直到咬到黑线
	for (i=0; i<10000; i++) {
		IWDG_ReloadCounter();
		Get_LineWalking(&l1,&l2,&r1,&r2,&tl,&tr);
		LeftMotor_Speed(-40);
		RightMotor_Speed(40);
		if (Serial_GetRxFlag()) { uint8_t ch=Serial_GetRxData(); if(ch==0x42){Car_Stop();return;} }
		if (l1==0 || r1==0) {Car_Stop(); return;}
	}
	Car_Stop();
}
