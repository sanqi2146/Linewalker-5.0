/*
 * 蓝牙串口通信模块 - 实现
 * 
 * 使用STM32的USART1外设与蓝牙模块（HC-05/HC-06等）通信。
 * 
 * 配置参数：
 *   - TX引脚: PA9 （复用推挽输出）
 *   - RX引脚: PA10（上拉输入）
 *   - 波特率: 9600
 *   - 数据位: 8位
 *   - 停止位: 1位
 *   - 校验位: 无
 * 
 * 接收机制（v2 环形缓冲区）：
 *   蓝牙连续发送多字节 → ISR 写入环形缓冲区（不丢字节）
 *   主循环逐字节从缓冲区取出消费
 */

#include "stm32f10x.h"                  // Device header
#include <stdio.h>                       // 标准输入输出库

#define RX_BUF_SIZE 64
static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;

void Serial_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate=9600;  // HC-05 默认波特率
	USART_InitStructure.USART_HardwareFlowControl=USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode=USART_Mode_Tx|USART_Mode_Rx;
	USART_InitStructure.USART_Parity=USART_Parity_No;
	USART_InitStructure.USART_StopBits=USART_StopBits_1;
	USART_InitStructure.USART_WordLength=USART_WordLength_8b;
	USART_Init(USART1,&USART_InitStructure);
	
	USART_ITConfig(USART1,USART_IT_RXNE,ENABLE);
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel= USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=1;
	NVIC_Init(&NVIC_InitStructure);
	
	USART_Cmd(USART1,ENABLE);
}

// 运行时切换波特率 (用于HC-05 AT命令模式切换)
void Serial_SetBaud(uint32_t baud)
{
	USART_Cmd(USART1, DISABLE);
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = baud;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &USART_InitStructure);
	USART_Cmd(USART1, ENABLE);
}

// 清除接收环形缓冲区
void Serial_ClearRxBuf(void)
{
	__disable_irq();
	rx_head = 0;
	rx_tail = 0;
	__enable_irq();
}

void Serial_SendByte(uint8_t Byte)
{
	USART_SendData(USART1,Byte);
	while(USART_GetFlagStatus(USART1,USART_FLAG_TXE)==RESET);
}

void Serial_SendString(char *String)
{
	uint8_t i;
	for(i=0;String[i]!='\0';i++) Serial_SendByte(String[i]);
}

uint32_t Serial_Pow(uint32_t x,uint32_t y)
{
	uint32_t Result=1;
	while(y--) Result*=x;
	return Result;
}

void Serial_SendNumber(uint32_t Number,uint8_t Length)
{
	uint8_t i;
	for(i=0;i<Length;i++)
		Serial_SendByte(Number/Serial_Pow(10,Length-i-1)%10+0x30);
}

int fputc(int ch,FILE*f)
{
	Serial_SendByte(ch);
	return ch;
}

uint8_t Serial_GetRxFlag(void)
{
	return (rx_head != rx_tail) ? 1 : 0;
}

uint8_t Serial_GetRxData(void)
{
	uint8_t ch;
	__disable_irq();
	ch = rx_buf[rx_tail];
	rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
	__enable_irq();
	return ch;
}

void USART1_IRQHandler(void)
{
	if(USART_GetITStatus(USART1,USART_IT_RXNE)==SET)
	{
		uint8_t ch = USART_ReceiveData(USART1);
		uint8_t next = (rx_head + 1) % RX_BUF_SIZE;
		if (next != rx_tail) {  // 未满
			rx_buf[rx_head] = ch;
			rx_head = next;
		}
		// 满了就丢弃这个字节（防止覆盖未读数据）
		USART_ClearITPendingBit(USART1,USART_IT_RXNE);
	}
}
