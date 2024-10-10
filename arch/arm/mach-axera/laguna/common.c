/* SPDX-License-Identifier: GPL-2.0+
 *
 * Common board-specific code
 *
 * Copyright (c) 2024 Charleye <wangkart@aliyun.com>
 */

#include <asm/arch/bootparams.h>
#include <dm/uclass.h>
#include <common.h>
#include <fdt_support.h>
#include <asm/sections.h>
#include <linux/sizes.h>
#include <exports.h>
#include <env.h>
#include <init.h>
#if IS_ENABLED(CONFIG_MTD) || IS_ENABLED(CONFIG_DM_MTD)
#include <mtd.h>
#endif
#if IS_ENABLED(CONFIG_CMD_NAND) && IS_ENABLED(CONFIG_MTD_RAW_NAND)
#include <nand.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_OF_BOARD
void *board_fdt_blob_setup(int *err)
{
	void *fdt_addr;

	/* FDT is at end of image */
	fdt_addr = (ulong *)&_end;
	if (!fdt_check_header(fdt_addr)) {
		*err = 0;
		return fdt_addr;
	}

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

	*err = -FDT_ERR_NOTFOUND;
	return NULL;
}
#endif

int fdtdec_board_setup(const void *fdt_blob)
{
#if defined(CONFIG_LUA_SELECT_UART_FOR_CONSOLE)
	int node = -1;
	const char *str, *p;
	char name[20];
	int namelen;
	int idx, ret;

	str = fdtdec_get_chosen_prop(fdt_blob, "stdout-path");
	if (str) {
		p = strchr(str, ':');
		namelen = p ? p - str : strlen(str);
		memcpy(name, str, namelen);
		node = fdt_path_offset_namelen(fdt_blob, str, namelen);
		if (node < 0) {
			printf("Invalid '%s' node\n", name);
			return -ENODATA;
		}

		idx = name[namelen - 1] - '0';
		if (idx == CONFIG_LUA_UART_INDEX)
			return 0;

		snprintf(name, namelen + 1, "serial%d", CONFIG_LUA_UART_INDEX);
		if (p)
			memcpy(name + namelen, str + namelen, 20 - namelen);

		/* check for new UART name */
		node = fdt_path_offset_namelen(fdt_blob, name, namelen);
		if (node < 0) {
			printf("Failed to get new '%s' node\n", name);
			return -ENODATA;
		}

		node = fdt_path_offset(fdt_blob, "/chosen");
		ret = fdt_setprop_string((void*)fdt_blob, node, "stdout-path", name);
		if (ret)
			return ret;
	}
#endif

	return 0;
}

void reset_cpu(void)
{
	return;
}

#ifndef CONFIG_SPL_BUILD
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

int board_init(void)
{
#if IS_ENABLED(CONFIG_DM_SPI_FLASH) && IS_ENABLED(CONFIG_SPI_FLASH_MTD)
	struct udevice *dev;

	/* probe all SPI NOR Flash device */
	uclass_foreach_dev_probe(UCLASS_SPI_FLASH, dev)
		;
#endif

	return 0;
}

#if IS_ENABLED(CONFIG_CMD_NAND) && IS_ENABLED(CONFIG_MTD_RAW_NAND)
void board_nand_init(void)
{
	struct udevice *dev;
	struct mtd_info *mtd;
	char mtd_name[20];
	int i;

	/* probe all SPI NAND Flash devices */
	uclass_foreach_dev_probe(UCLASS_MTD, dev)
		debug("SPI NAND Flash device = %s\n", dev->name);

	for (i = 0; i < CONFIG_SYS_MAX_NAND_DEVICE; i++) {
		snprintf(mtd_name, sizeof(mtd_name), "spi-nand%d", i);
		mtd = get_mtd_device_nm(mtd_name);
		if (IS_ERR_OR_NULL(mtd)) {
			debug("MTD device %s not found, ret %ld\n", mtd_name,
				PTR_ERR(mtd));
			return;
		}

		nand_register(0, mtd);
	}
}
#endif
#endif /* !CONFIG_SPI_BUILD */
