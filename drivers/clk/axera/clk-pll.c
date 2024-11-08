// SPDX-License-Identifier: GPL-2.0+
/*
 * Axera Laguna SoC PLL clock driver
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#define LOG_CATEGORY UCLASS_CLK

#include <common.h>
#include <clk.h>
#include <clk-uclass.h>
#include <log.h>
#include <malloc.h>
#include <asm/io.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <dm/uclass.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/err.h>

#define PLL_DRV_NAME "clk_pll"

struct pll_clk {
	uint32_t stat;
	uint32_t cfg0;
	uint32_t rsvd0[2];
	uint32_t cfg1;
	uint32_t rsvd1[2];
	uint32_t ssc0;
	uint32_t rsvd2[2];
	uint32_t ssc1;
	uint32_t rsvd3[2];
};

#define PLL_REG         0
#define PLL_ON          1
#define PLL_RDY         2
#define PLL_FRC_EN      3
#define PLL_FRC_EN_SW   4
#define PLL_REOPEN      5
#define PLL_REG_CNT     6

struct pll_clk_priv {
	/*
	 * 0: pll_reg address
	 * 1: pll_on address
	 * 2: pll_rdy address
	 * 3: pll_frc_en address
	 * 4: pll_frc_en_sw address
	 * 5: pll_reopen address
	 */
	void __iomem *addr[PLL_REG_CNT];
	uint8_t rdy_idx;
	uint8_t frc_en_idx;
	uint8_t frc_en_sw_idx;
	uint8_t reopen_idx;
};

ulong pll_clk_set_rate(struct clk *clk, ulong rate)
{
	return 0;
}

ulong pll_clk_get_rate(struct clk *clk)
{
	return clk_generic_get_rate(clk);
	// return 0;
}

static int clk_pll_of_xlate(struct clk *clk,
			struct ofnode_phandle_args *args)
{
	struct clk *clkp = dev_get_uclass_priv(clk->dev);

	clk->id = dev_seq(clkp->dev);

	return 0;
}

const struct clk_ops pll_clk_ops = {
	.get_rate = pll_clk_get_rate,
	.set_rate = pll_clk_set_rate,
	.of_xlate = clk_pll_of_xlate,
};

static int pll_clk_of_to_plat(struct udevice *dev)
{
	struct pll_clk_priv *priv = dev_get_priv(dev);
	uint32_t phandle;
	ofnode node;
	phys_addr_t addr;
	uint32_t offset;
	uint32_t idx;
	int err, i;
	const char *prop[] = {
		"pll-reg",
		"pll-on",
		"pll-rdy",
		"pll-frc-en",
		"pll-frc-en-sw",
		"pll-reopen",
	};

	for (i = 0; i < PLL_REG_CNT; i++) {
		err = ofnode_read_u32(dev_ofnode(dev), prop[i], &phandle);
		if (err)
			return err;

		node = ofnode_get_by_phandle(phandle);
		addr = ofnode_get_addr(node);

		err = ofnode_read_u32_index(dev_ofnode(dev), prop[i], 1, &offset);
		if (err) {
			dev_err(dev, "Failed to get '%s' value\n", prop[i]);
			return -ENODATA;
		}
		priv->addr[i] = (void *)addr + offset;

		if (i < PLL_RDY)
			continue;

		err = ofnode_read_u32_index(dev_ofnode(dev), prop[i], 2, &idx);
		if (err) {
			dev_err(dev, "Failed to get '%s' value\n", prop[i]);
			return -ENODATA;
		}
		switch (i) {
		case PLL_RDY:
			priv->rdy_idx = idx;
			break;
		case PLL_FRC_EN:
			priv->frc_en_idx = idx;
			break;
		case PLL_FRC_EN_SW:
			priv->frc_en_sw_idx = idx;
			break;
		case PLL_REOPEN:
			priv->reopen_idx = idx;
			break;
		}
	}

	return 0;
}

U_BOOT_DRIVER(axera_clk_pll) = {
	.name	= PLL_DRV_NAME,
	.id	= UCLASS_CLK,
	.ops	= &pll_clk_ops,
	.of_to_plat = pll_clk_of_to_plat,
	.priv_auto = sizeof(struct pll_clk_priv),
	.flags = DM_FLAG_PRE_RELOC,
};

static int pll_clk_bind(struct udevice *dev)
{
	return 0;
}

static const struct udevice_id pll_clk_of_match[] = {
	{ .compatible = "axera,lua-pll-clocks" },
	{ /* sentinel */ },
};

U_BOOT_DRIVER(pll_clk) = {
	.name = "pll_clock",
	.id = UCLASS_NOP,
	.of_match = pll_clk_of_match,
	.bind = pll_clk_bind,
};