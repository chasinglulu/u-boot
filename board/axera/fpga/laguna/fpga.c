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

#if defined(CONFIG_LUA_PERIPH_CRU)
#define PERIPH_SWRST0_RSTVAL            0x7FFFFFFF
#define PHEIPH_CLK_GATE_ENABLE_RSTVAL   0xE0000000
void main_uart_clk_enable(void)
{
	unsigned int reg;

	writel(PHEIPH_CLK_GATE_ENABLE_RSTVAL,
		   (CONFIG_LUA_PERIPH_CRU_BASE +
		     CONFIG_LUA_PERIPH_CLK_GATE_REG0));
	udelay(1);

	/* Enable UART1 Clock Gate */
	reg = readl(CONFIG_LUA_PERIPH_CRU_BASE +
			    CONFIG_LUA_PERIPH_CLK_GATE_REG0);
	reg |= BIT(9);    /* UART1 PCLK */
	reg |= BIT(14);   /* UART1 CLK */

	reg |= BIT(10);   /* UART2 PCLK */
	reg |= BIT(15);   /* UART2 CLK */

	reg |= BIT(11);   /* UART3 PCLK */
	reg |= BIT(16);   /* UART3 CLK */

	reg |= BIT(12);   /* UART4 PCLK */
	reg |= BIT(17);   /* UART4 CLK */

	reg |= BIT(13);   /* UART5 PCLK */
	reg |= BIT(18);   /* UART5 CLK */
	writel(reg, (CONFIG_LUA_PERIPH_CRU_BASE +
		     CONFIG_LUA_PERIPH_CLK_GATE_REG0));
}

void deassert_uart1_reset(void)
{
	unsigned int reg;

	writel(PERIPH_SWRST0_RSTVAL,
		    (CONFIG_LUA_PERIPH_CRU_BASE +
		      CONFIG_LUA_PERIPH_SWRST_REG0));
	udelay(5);
	/* Deassert UART1 */
	reg = readl(CONFIG_LUA_PERIPH_CRU_BASE +
			    CONFIG_LUA_PERIPH_SWRST_REG0);
	reg &= ~BIT(11);    /* UART1 SW PRST */
	reg &= ~BIT(16);    /* UART1 SW RST */
	writel(reg, (CONFIG_LUA_PERIPH_CRU_BASE +
		     CONFIG_LUA_PERIPH_SWRST_REG0));
}
#else
void main_uart_clk_enable(void) {};
void deassert_uart1_reset(void) {};
#endif

#ifdef CONFIG_BOARD_EARLY_INIT_F
int board_early_init_f(void)
{
	main_uart_clk_enable();
	deassert_uart1_reset();
	return 0;
}
#endif

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{
	main_uart_clk_enable();
	deassert_uart1_reset();
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