// SPDX-License-Identifier: GPL-2.0+
/*
 * Axera Laguna SoC PLL contoller driver
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <asm/io.h>
#include <dm.h>
#include <div64.h>
#include <errno.h>
#include <clk-uclass.h>
#include <linux/clk-provider.h>
#include <linux/rational.h>
#include <dm/device_compat.h>
#include <asm/arch/clk.h>

struct pll_ctrl_priv {
	struct clk clk;
	void __iomem *reg;
	uint32_t flags;
};

#define to_clk_pll(_clk) container_of(_clk, struct pll_ctrl, clk)

struct clk *clk_register_pll(const char *name, const char *parent_name,
				void __iomem *reg)
{
	struct pll_ctrl_priv *pllc;
	int ret;

	pllc = kzalloc(sizeof(*pllc), GFP_KERNEL);
	if (!pllc)
		return ERR_PTR(-ENOMEM);

	pllc->reg = reg;

	ret = clk_register(&pllc->clk, "ax_clk_pll", name, parent_name);
	if (ret) {
		printf("%s: failed to register: %d\n", __func__, ret);
		kfree(pllc);
		return ERR_PTR(ret);
	}

	return &pllc->clk;
}

static int pll_ctrl_bind(struct udevice *dev)
{
	struct ofnode_phandle_args args;
	ofnode subnode;
	struct clk *clk;
	const char *name;
	int ret;

	dev_for_each_subnode(subnode, dev) {
		name = ofnode_get_name(subnode);

		ret = ofnode_parse_phandle_with_args(subnode, "clocks",
					"#clock-cells", 0,
					0, &args);
		if (ret) {
			dev_warn(dev, "Failed to get clocks in %s\n", name);
			continue;
		}

		clk = clk_register_pll(name, ofnode_get_name(args.node),
								NULL);
		if (IS_ERR(clk)) {
			dev_warn(dev, "Failed to register '%s' clk\n", name);
			continue;
		}

		clk->id = dev_seq(clk->dev);
		dev_set_ofnode(clk->dev, subnode);
	};

	return 0;
}

static const struct udevice_id pll_ctrl_of_match[] = {
	{ .compatible = "axera,lua-pll-ctrl" },
	{ },
};

U_BOOT_DRIVER(pll_ctrl) = {
	.name = "pll-ctrl",
	.id = UCLASS_CLK,
	.of_match = pll_ctrl_of_match,
	.bind = pll_ctrl_bind,
	.flags = DM_FLAG_PRE_RELOC,
};
