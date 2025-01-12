/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2023 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <init.h>
#include "jffs2/load_kernel.h"
#include "fdt_support.h"
#include "mtd_node.h"
#include "env.h"

DECLARE_GLOBAL_DATA_PTR;

#if CONFIG_IS_ENABLED(BOARD_EARLY_INIT_R)
int board_early_init_f(void)
{
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
		{ "jedec,spi-nor", MTD_DEV_TYPE_NOR, },
		{ "spi-nand", MTD_DEV_TYPE_NAND, },
		{ "spi-nand", MTD_DEV_TYPE_SPINAND, },
	};

	/* Update partition nodes using info from mtdparts env var */
	puts("   Updating MTD partitions...\n");
	fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));
#endif

	return 0;
}
#endif