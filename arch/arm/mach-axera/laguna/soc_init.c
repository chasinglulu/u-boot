/* SPDX-License-Identifier: GPL-2.0+
 *
 * Common SoC-specific initialization code
 *
 * Copyright (C) 2025 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/arch/bootparams.h>
#include <asm/arch/cpu.h>

#if defined(CONFIG_LUA_COMMON_CRU)
static void main_pll_ctrl_init(void)
{
	unsigned int reg;

	/* Enabel pclk_pll_ctrl2_eb */
	reg = readl(CONFIG_LUA_COMMON_CRU_BASE +
	             CONFIG_LUA_COMMON_CLK_GATE_REG0);
	reg |= BIT(23);
	writel(reg, (CONFIG_LUA_COMMON_CRU_BASE +
	              CONFIG_LUA_COMMON_CLK_GATE_REG0));

	/* Deassert pll_ctrl2_sw_rst */
	reg = readl(CONFIG_LUA_COMMON_CRU_BASE +
	             CONFIG_LUA_COMMON_SWRST_REG0);
	reg &= ~BIT(23);
	writel(reg, (CONFIG_LUA_COMMON_CRU_BASE +
	              CONFIG_LUA_COMMON_SWRST_REG0));
}
#else
static inline void main_pll_ctrl_init(void) { }
#endif

#if defined(CONFIG_LUA_PERIPH_CRU)
#define PERIPH_SWRST0_RSTVAL               0x7FFFFBC3
#define PHEIPH_CLK_GATE_ENABLE_RSTVAL      0x6000013C
static void main_uart1_clk_enable(void)
{
	unsigned int reg;

	reg = readl(CONFIG_LUA_PERIPH_CRU_BASE +
	             CONFIG_LUA_PERIPH_CLK_MUX_ENABLE_REG0);
	reg |= BIT(2);    /* clk_periph_ser_eb */
	writel(reg, (CONFIG_LUA_PERIPH_CRU_BASE +
	              CONFIG_LUA_PERIPH_CLK_MUX_ENABLE_REG0));

	writel(PHEIPH_CLK_GATE_ENABLE_RSTVAL,
	        (CONFIG_LUA_PERIPH_CRU_BASE +
	          CONFIG_LUA_PERIPH_CLK_GATE_REG0));
	udelay(1);

	/* Enable UART1 Clock Gate */
	reg = readl(CONFIG_LUA_PERIPH_CRU_BASE +
	             CONFIG_LUA_PERIPH_CLK_GATE_REG0);
	reg |= BIT(9);    /* UART1 PCLK */
	reg |= BIT(14);   /* UART1 CLK */

	writel(reg, (CONFIG_LUA_PERIPH_CRU_BASE +
	              CONFIG_LUA_PERIPH_CLK_GATE_REG0));
}

static void main_uart1_rst_deassert(void)
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
static inline void main_uart1_clk_enable(void) { }
static inline void main_uart1_rst_deassert(void) { }
#endif

#if defined(CONFIG_LUA_SAFETY_INIT)
#if defined(CONFIG_LUA_SAFETY_TOP_CRU)
static void safety_pll_ctrl_init(void)
{
	unsigned int reg;

	/* Enabel safety_pll_ctrl_eb */
	reg = readl(CONFIG_LUA_SAFETY_TOP_CRU_BASE +
	             CONFIG_LUA_SAFETY_TOP_CLK_GATE_REG0);
	reg |= BIT(4);
	writel(reg, (CONFIG_LUA_SAFETY_TOP_CRU_BASE +
	              CONFIG_LUA_SAFETY_TOP_CLK_GATE_REG0));

	/* Deassert safety_pll_ctrl_sw_rst */
	reg = readl(CONFIG_LUA_SAFETY_TOP_CRU_BASE +
	             CONFIG_LUA_SAFETY_TOP_SWRST_REG0);
	reg &= ~BIT(2);

	writel(reg, (CONFIG_LUA_SAFETY_TOP_CRU_BASE +
	              CONFIG_LUA_SAFETY_TOP_SWRST_REG0));
}
#else
static inline void safety_pll_ctrl_init(void) { }
#endif
#endif

int soc_init_f(void)
{
	main_uart1_clk_enable();
	main_uart1_rst_deassert();
	main_pll_ctrl_init();

#if defined(CONFIG_LUA_SAFETY_INIT)
	safety_pll_ctrl_init();
#endif

	return 0;
}