#include "stm32f10x.h"
#include "EightWayTrack.h"

// ==================== 硬件 I2C1 (PB6=SCL, PB7=SDA, 100KHz) ====================

void EightWayTrack_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

	GPIO_InitTypeDef g;
	g.GPIO_Mode  = GPIO_Mode_AF_OD;
	g.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
	g.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &g);

	I2C_DeInit(I2C1);
	I2C_InitTypeDef i;
	i.I2C_Mode                = I2C_Mode_I2C;
	i.I2C_DutyCycle           = I2C_DutyCycle_2;
	i.I2C_OwnAddress1         = 0x00;
	i.I2C_Ack                 = I2C_Ack_Enable;
	i.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	i.I2C_ClockSpeed          = 100000;  // 100KHz 标准模式
	I2C_Init(I2C1, &i);
	I2C_Cmd(I2C1, ENABLE);
}

static uint16_t i2c1_timeout;  // 软件看门狗: 防止I2C总线卡死

// 等待I2C事件, 带超时保护
static uint8_t I2C1_WaitEvent(uint32_t event)
{
	i2c1_timeout = 10000;
	while (!I2C_CheckEvent(I2C1, event))
	{
		if (--i2c1_timeout == 0) return 0;  // 超时失败
	}
	return 1;
}

// 等待I2C标志位
static uint8_t I2C1_WaitFlag(uint32_t flag, FlagStatus state)
{
	i2c1_timeout = 10000;
	while (I2C_GetFlagStatus(I2C1, flag) != state)
	{
		if (--i2c1_timeout == 0) return 0;
	}
	return 1;
}

// 读寄存器0x30 → 返回8路数字值
EightWayData EightWayTrack_Read(void)
{
	EightWayData data;
	uint8_t raw = 0;
	uint8_t ok = 1;

	// ----- 写设备地址(写模式) -----
	I2C_GenerateSTART(I2C1, ENABLE);
	if (!I2C1_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) goto fail;
	I2C_Send7bitAddress(I2C1, EWT_I2C_ADDR << 1, I2C_Direction_Transmitter);
	if (!I2C1_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) goto fail;

	// ----- 写寄存器地址 0x30 -----
	I2C_SendData(I2C1, EWT_REG_DIGITAL);
	if (!I2C1_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) goto fail;

	// ----- 重复起始 + 读设备地址(读模式) -----
	I2C_GenerateSTART(I2C1, ENABLE);
	if (!I2C1_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) goto fail;
	I2C_Send7bitAddress(I2C1, EWT_I2C_ADDR << 1, I2C_Direction_Receiver);
	if (!I2C1_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) goto fail;

	// ----- 读1字节, 先关ACK再STOP, 再读RX -----
	// 注意: 必须在最后一个字节的ADDR之后先关ACK和发STOP, 再读DR
	I2C_AcknowledgeConfig(I2C1, DISABLE);  // 最后字节不ACK
	I2C_GenerateSTOP(I2C1, ENABLE);
	if (!I2C1_WaitFlag(I2C_FLAG_RXNE, SET)) goto fail;
	raw = I2C_ReceiveData(I2C1);

	goto done;

fail:
	// I2C总线锁死时完整重初始化, 不能用SoftwareReset(会清空配置寄存器)
	ok = 0;
	I2C_GenerateSTOP(I2C1, ENABLE);
	I2C_Cmd(I2C1, DISABLE);
	// 发9个SCL时钟脉冲释放被锁的从机
	int k;
	GPIO_InitTypeDef gs;
	gs.GPIO_Mode  = GPIO_Mode_Out_OD;
	gs.GPIO_Pin   = GPIO_Pin_6;
	gs.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gs);
	for (k = 0; k < 9; k++) {
		volatile int d;
		GPIO_ResetBits(GPIOB, GPIO_Pin_6);
		for (d = 0; d < 10; d++);
		GPIO_SetBits(GPIOB, GPIO_Pin_6);
		for (d = 0; d < 10; d++);
	}
	// 重新恢复到I2C复用模式
	GPIO_InitTypeDef g;
	g.GPIO_Mode  = GPIO_Mode_AF_OD;
	g.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
	g.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &g);
	I2C_DeInit(I2C1);
	I2C_InitTypeDef i;
	i.I2C_Mode                = I2C_Mode_I2C;
	i.I2C_DutyCycle           = I2C_DutyCycle_2;
	i.I2C_OwnAddress1         = 0x00;
	i.I2C_Ack                 = I2C_Ack_Enable;
	i.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	i.I2C_ClockSpeed          = 100000;
	I2C_Init(I2C1, &i);
	I2C_Cmd(I2C1, ENABLE);

done:
	// bit7=L1(最左) ... bit0=L8(最右), 0=黑线
	data.x1 = (raw >> 7) & 0x01;
	data.x2 = (raw >> 6) & 0x01;
	data.x3 = (raw >> 5) & 0x01;
	data.x4 = (raw >> 4) & 0x01;
	data.x5 = (raw >> 3) & 0x01;
	data.x6 = (raw >> 2) & 0x01;
	data.x7 = (raw >> 1) & 0x01;
	data.x8 = (raw >> 0) & 0x01;
	data.ok = ok;

	return data;
}
