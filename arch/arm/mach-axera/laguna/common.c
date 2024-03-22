/* SPDX-License-Identifier: GPL-2.0+
 *
 * Common board-specific code
 *
 * Copyright (c) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <init.h>
#include <fdt_support.h>
#include <asm/sections.h>
#include <linux/sizes.h>
#include <exports.h>
#include <env.h>

extern uint64_t boot_start_addr;
DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_OF_BOARD
void *board_fdt_blob_setup(int *err)
{
	/* QEMU loads a generated DTB for us at the start of RAM. */
	void *fdt_addr = (void *)CONFIG_LUA_DRAM_BASE;

	if (!fdt_check_header(fdt_addr)) {
		*err = 0;
		return fdt_addr;
	}

	/* FDT is at end of image */
	fdt_addr = (ulong *)&_end;
	*err = 0;

	return fdt_addr;
}
#endif

/*
 * Get the top of usable RAM
 *
 */
ulong board_get_usable_ram_top(ulong total_size)
{
	/* TODO: detect size dynamically */
	return CONFIG_LUA_DRAM_BASE + SZ_4G - 1;
}

#ifdef CONFIG_LAST_STAGE_INIT
int last_stage_init(void)
{
	return 0;
}
#endif

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_SYSTEM_SETUP)
int ft_system_setup(void *blob, struct bd_info *bd)
{
	return 0;
}
#endif

int dram_init(void)
{
	if (fdtdec_setup_mem_size_base() != 0)
		return -EINVAL;

	return 0;
}

int dram_init_banksize(void)
{
	fdtdec_setup_memory_banksize();

	return 0;
}
