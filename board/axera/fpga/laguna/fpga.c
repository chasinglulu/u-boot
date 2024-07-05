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
#include <asm/io.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

#define PHEIPH_SYS_CK_RST_BASE  0x0E001000
void uart_clk_enable(void)
{
	writel(0x0, PHEIPH_SYS_CK_RST_BASE + 0x40);
	writel(0xFFFFFFFF, PHEIPH_SYS_CK_RST_BASE + 0x120);
	udelay(10);
	writel(0x7FE00, PHEIPH_SYS_CK_RST_BASE + 0x40);
	writel(0xFFE007FF, PHEIPH_SYS_CK_RST_BASE + 0x120);
}

#ifdef CONFIG_BOARD_EARLY_INIT_F
int board_early_init_f(void)
{
	uart_clk_enable();
	return 0;
}
#endif

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{
	uart_clk_enable();
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
	return 0;
}
#endif