// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023, Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/ofnode.h>
#include <env.h>
#include <fdtdec.h>
#include <image.h>
#include <log.h>
#include <spl.h>
#include <init.h>

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	return 0;
}

#if CONFIG_IS_ENABLED(LMT_RISCV_NO_DRAM)
int mach_cpu_init(void)
{
	gd->flags |= GD_FLG_SKIP_RELOC;
	return 0;
}
#endif

#ifdef CONFIG_SPL
u32 spl_boot_device(void)
{
	/* RISC-V QEMU only supports RAM as SPL boot device */
	return BOOT_DEVICE_RAM;
}
#endif

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	/* boot using first FIT config */
	return 0;
}
#endif
