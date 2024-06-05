/* SPDX-License-Identifier: GPL-2.0+
 *
 * Common board-specific code
 *
 * Copyright (c) 2024 Charleye <wangkart@aliyun.com>
 */

#include <asm/arch/bootparams.h>
#include <common.h>
#include <init.h>
#include <fdt_support.h>
#include <asm/sections.h>
#include <linux/sizes.h>
#include <exports.h>
#include <env.h>
#include <nand.h>
#include <dm/uclass.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_OF_BOARD
void *board_fdt_blob_setup(int *err)
{
	void *fdt_addr = NULL;
#ifndef CONFIG_SPL_BUILD
	boot_params_t *bp = boot_params_get_base();
	if (bp->fdt_addr && !fdt_check_header(bp->fdt_addr)) {
		*err = 0;
		return bp->fdt_addr;
	}

	/* QEMU loads a generated DTB for us at the start of DRAM. */
	fdt_addr = (void *)CONFIG_LUA_DRAM_BASE;
	if (!fdt_check_header(fdt_addr)) {
		*err = 0;
		return fdt_addr;
	}
#endif

	/* FDT is at end of image */
	fdt_addr = (ulong *)&_end;
	if (!fdt_check_header(fdt_addr)) {
		*err = 0;
		return fdt_addr;
	}

	*err = -FDT_ERR_NOTFOUND;
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


#if CONFIG_IS_ENABLED(CMD_NAND)
void board_nand_init(void)
{
	struct mtd_info *mtd;
	struct udevice *dev;
	char mtd_name[20];
	int i;

	/* probe all mtd device */
	uclass_foreach_dev_probe(UCLASS_MTD, dev)
		;

	for (i = 0; i < CONFIG_SYS_MAX_NAND_DEVICE; i++) {
		snprintf(mtd_name, sizeof(mtd_name), "spi-nand%d", i);
		mtd = get_mtd_device_nm(mtd_name);
		if (IS_ERR_OR_NULL(mtd)) {
			printf("MTD device %s not found, ret %ld\n", mtd_name,
				PTR_ERR(mtd));
			return;
		}

		nand_register(0, mtd);
	}
}
#endif