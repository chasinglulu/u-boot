// SPDX-License-Identifier: GPL-2.0+
/*
 * Common board-specific code
 *
 * Copyright (c) 2023 Horizon Robotics Co., Ltd
 */

#include <common.h>
#include <init.h>
#include <fdt_support.h>
#include <asm/sections.h>
#include <linux/sizes.h>

extern uint32_t ram_base_msb;
DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_OF_BOARD
void *board_fdt_blob_setup(int *err)
{
	/* QEMU loads a generated DTB for us at the start of RAM. */
	void *fdt_addr = NULL;

	if (ram_base_msb >= 0x25)
		fdt_addr = (void *)CONFIG_SYS_NON_INTER_DDR_BASE;
	else
		fdt_addr = (void *)CONFIG_SYS_INTERLEAVE_DDR_BASE;

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
 * Assuming the size of the OCM is 32MiB
 */
ulong board_get_usable_ram_top(ulong total_size)
{

	if (ram_base_msb >= 0x20) {
		if (ram_base_msb >= 0x25)
			return CONFIG_SYS_NON_INTER_DDR_BASE + gd->ram_size;
		else
			return CONFIG_SYS_INTERLEAVE_DDR_BASE + gd->ram_size;
	} else {
		return CONFIG_SYS_OCM_BASE + SZ_32M;
	}

	/* Never reach here */
	return 0;
}
