// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2022 Horizon Robotics Co., Ltd
 */

#include <common.h>
#include <dm.h>
#include <init.h>
#include <asm/system.h>
#include <linux/sizes.h>

extern uint32_t ram_base_msb;
DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_SPL_BUILD
int board_early_init_f(void)
{
	return 0;
}
#endif

int board_init(void)
{
	printf("EL Level:\tEL%d\n", current_el());

	return 0;
}

ulong board_get_usable_ram_top(ulong total_size)
{
	if (ram_base_msb >= 32)
		return CONFIG_SYS_DDR_BASE + gd->ram_size;
	else
		return CONFIG_SYS_OCM_BASE + SZ_32M;
}
