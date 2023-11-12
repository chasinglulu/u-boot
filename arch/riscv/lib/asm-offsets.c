// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 *
 * From arch/x86/lib/asm-offsets.c
 *
 * This program is used to generate definitions needed by
 * assembly language modules.
 */

#include <common.h>
#include <asm/global_data.h>
#include <linux/kbuild.h>

int main(void)
{
	DEFINE(GD_BOOT_HART, offsetof(gd_t, arch.boot_hart));
	DEFINE(GD_FIRMWARE_FDT_ADDR, offsetof(gd_t, arch.firmware_fdt_addr));
#ifndef CONFIG_XIP
	DEFINE(GD_AVAILABLE_HARTS, offsetof(gd_t, arch.available_harts));
#endif
	DEFINE(GD_FLAGS, offsetof(gd_t, flags));
	DEFINE(GD_NEW_GD, offsetof(gd_t, new_gd));
	DEFINE(GD_NEW_FDT, offsetof(gd_t, new_fdt));
	DEFINE(GD_FDT_BLOB, offsetof(gd_t, fdt_blob));
	DEFINE(GD_FDT_SIZE, offsetof(gd_t, fdt_size));

	return 0;
}
