// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2022 Horizon Robotics Co., Ltd
 */

#include <common.h>
#include <dm.h>
#include <init.h>
#include <asm/system.h>

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

#ifdef CONFIG_OF_LIBFDT
#ifdef CONFIG_OF_SYSTEM_SETUP
int ft_system_setup(void *blob, struct bd_info *bd)
{
	return 0;
}
#endif

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, struct bd_info *bd)
{
	return 0;
}
#endif
#endif