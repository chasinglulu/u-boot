// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2022 Horizon Robotics Co., Ltd
 */

#include <common.h>
#include <dm.h>
#include <init.h>

#ifndef CONFIG_SPL_BUILD
int board_early_init_f(void)
{
	return 0;
}
#endif

int board_init(void)
{
	return 0;
}
