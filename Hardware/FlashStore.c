/*
 * FLASH参数存储模块 - 实现
 * 
 * STM32F103C8T6 内部Flash = 64KB, 页大小 = 1KB
 * 使用最后一页（第63页, 地址 0x0800FC00）存储配置。
 * 此地址远在代码区之后（正常固件 < 32KB），不会冲突。
 */

#include "stm32f10x.h"
#include "FlashStore.h"

#define FLASH_SAVE_ADDR  0x0800FC00  // 最后一页起始地址

FlashConfig g_flash_cfg;  // 全局配置实例

/*
 * 从Flash读取配置
 * 如果Flash区域未被写过(全0xFF)，则使用默认值
 */
void FlashStore_Init(void)
{
	FlashConfig *p = (FlashConfig *)FLASH_SAVE_ADDR;

	// 检查是否写过：写过的kp不会是0xFFFFFFFF
	if (*(uint32_t *)&p->kp == 0xFFFFFFFF)
	{
		FlashStore_LoadDefaults();
		return;
	}

	g_flash_cfg.kp                 = p->kp;
	g_flash_cfg.ki                 = p->ki;
	g_flash_cfg.kd                 = p->kd;
	g_flash_cfg.base_speed         = p->base_speed;
	g_flash_cfg.sharp_rotate_speed = p->sharp_rotate_speed;
	g_flash_cfg.turn_lock          = p->turn_lock;

	if (g_flash_cfg.kp <= 0.0f || g_flash_cfg.kp > 300.0f)   goto load_defaults;
	if (g_flash_cfg.ki < 0.0f || g_flash_cfg.ki > 100.0f)     goto load_defaults;
	if (g_flash_cfg.kd < 0.0f || g_flash_cfg.kd > 300.0f)    goto load_defaults;
	if (g_flash_cfg.base_speed < 20 || g_flash_cfg.base_speed > 99) goto load_defaults;
	return;

load_defaults:
	FlashStore_LoadDefaults();
}

void FlashStore_Save(void)
{
	FLASH_Unlock();
	FLASH_ErasePage(FLASH_SAVE_ADDR);
	FLASH_ProgramWord(FLASH_SAVE_ADDR + 0,  *(uint32_t *)&g_flash_cfg.kp);
	FLASH_ProgramWord(FLASH_SAVE_ADDR + 4,  *(uint32_t *)&g_flash_cfg.ki);
	FLASH_ProgramWord(FLASH_SAVE_ADDR + 8,  *(uint32_t *)&g_flash_cfg.kd);
	FLASH_ProgramWord(FLASH_SAVE_ADDR + 12, *(uint32_t *)&g_flash_cfg.base_speed);
	FLASH_Lock();
}

/*
 * 恢复出厂默认值 (PR2 默认挡)
 */
void FlashStore_LoadDefaults(void)
{
	g_flash_cfg.kp                 = 25.0f;
	g_flash_cfg.ki                 = 0.15f;
	g_flash_cfg.kd                 = 30.0f;
	g_flash_cfg.base_speed         = 55;
	g_flash_cfg.sharp_rotate_speed = 55;
	g_flash_cfg.turn_lock          = 6;
}
