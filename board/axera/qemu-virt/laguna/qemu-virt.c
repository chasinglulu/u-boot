/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2023 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <init.h>
#include <asm/system.h>
#include "jffs2/load_kernel.h"
#include "fdt_support.h"
#include "mtd_node.h"
#include "boot-device/bootdevice.h"
#include "env.h"

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

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	struct udevice *dev;
	uclass_get_device_by_name(UCLASS_BOOT_DEVICE, "boot-device", &dev);

	if (dev)
		dm_boot_device_update(dev);

	printf("Boot Device: %s\n", env_get("bootdevice"));

	return 0;
}
#endif

#ifdef CONFIG_DESIGNWARE_SPI
int dw_spi_get_clk(struct udevice *bus, ulong *rate)
{
	return 0;
}
#endif

#if defined (CONFIG_OF_LIBFDT) && defined (CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, struct bd_info *bd)
{
#if defined(CONFIG_FDT_FIXUP_PARTITIONS)
	static struct node_info nodes[] = {
		{ "cfi-flash", MTD_DEV_TYPE_NOR, },
	};

	/* Update partition nodes using info from mtdparts env var */
	puts("   Updating MTD partitions...\n");
	fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));
#endif

	return 0;
}
#endif