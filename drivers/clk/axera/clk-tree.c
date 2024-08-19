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

static ulong clk_factor_recalc_rate(struct clk *clk)
{
	struct clk_fixed_factor *fix = to_clk_fixed_factor(clk);
	unsigned long parent_rate = clk_get_parent_rate(clk);
	unsigned long long int rate;

	rate = (unsigned long long int)parent_rate * fix->mult;
	do_div(rate, fix->div);
	return (ulong)rate;
}

static ulong clk_factor_set_rate(struct clk *clk, ulong rate)
{
	struct clk *parent;

	if ((clk->flags & CLK_SET_RATE_PARENT) == 0)
		return -ENOSYS;

	parent = clk_get_parent(clk);
	if (IS_ERR(parent))
		return PTR_ERR(parent);

	rate = clk_set_rate(parent, rate);
	dev_dbg(clk->dev, "rate=%ld\n", rate);
	return rate;
}

static int clk_factor_of_xlate(struct clk *clk,
			struct ofnode_phandle_args *args)
{
	struct clk *clkp = dev_get_uclass_priv(clk->dev);

	clk->id = dev_seq(clkp->dev);

	return 0;
}

const struct clk_ops ax_clk_fixed_factor_ops = {
	.get_rate = clk_factor_recalc_rate,
	.set_rate = clk_factor_set_rate,
	.of_xlate = clk_factor_of_xlate,
};

static struct clk *__clk_hw_register_fixed_factor(struct device *dev,
		const char *name, const char *parent_name, unsigned long flags,
		unsigned int mult, unsigned int div)
{
	struct clk_fixed_factor *fix;
	struct clk *clk;
	int ret;

	fix = kzalloc(sizeof(*fix), GFP_KERNEL);
	if (!fix)
		return ERR_PTR(-ENOMEM);

	/* struct clk_fixed_factor assignments */
	fix->mult = mult;
	fix->div = div;
	clk = &fix->clk;
	clk->flags = flags;

	ret = clk_register(clk, "ax_clk_fixed_factor", name,
			   parent_name);
	if (ret) {
		kfree(fix);
		return ERR_PTR(ret);
	}

	return clk;
}

static struct clk *__clk_register_fixed_factor(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		unsigned int mult, unsigned int div)
{
	struct clk *clk;

	clk = __clk_hw_register_fixed_factor(dev, name, parent_name, flags, mult,
					  div);
	if (IS_ERR(clk))
		return ERR_CAST(clk);
	return clk;
}

U_BOOT_DRIVER(ax_clk_fixed_factor) = {
	.name = "ax_clk_fixed_factor",
	.id = UCLASS_CLK,
	.ops = &ax_clk_fixed_factor_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

static int clk_tree_bind(struct udevice *dev)
{
	struct ofnode_phandle_args args;
	ofnode subnode;
	struct clk *clk;
	const char *name, *sname;
	uint32_t div;
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

		div = ofnode_read_u32_default(subnode, "clock-div", -1);

		sname = ofnode_read_string(subnode, "compatible");
		if (!strcmp(sname, "fixed-factor-clock"))
			clk = __clk_register_fixed_factor(NULL, name, ofnode_get_name(args.node),
						0, 1, div);
		else if (!strcmp(sname, "pll-clk"))
			clk = clk_register_pll(name, ofnode_get_name(args.node), NULL);

		if (IS_ERR_OR_NULL(clk)) {
			dev_warn(dev, "Failed to register '%s' clk\n", name);
			continue;
		}

		clk->id = dev_seq(clk->dev);
		dev_set_ofnode(clk->dev, subnode);
	};

	return 0;
}

static const struct udevice_id clk_tree_of_match[] = {
	{ .compatible = "axera,lua-clock-tree" },
	{ },
};

U_BOOT_DRIVER(clk_tree) = {
	.name = "clock_tree",
	.id = UCLASS_NOP,
	.of_match = clk_tree_of_match,
	.bind = clk_tree_bind,
	.flags = DM_FLAG_PRE_RELOC,
};