// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <fdtdec.h>
#include <init.h>

int board_init(void)
{
	return 0;
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

int dram_init(void)
{
	if (fdtdec_setup_mem_size_base() != 0)
		return -EINVAL;

	return 0;
}

#ifdef CONFIG_DESIGNWARE_SPI
int dw_spi_get_clk(struct udevice *bus, ulong *rate)
{
	return 0;
}
#endif