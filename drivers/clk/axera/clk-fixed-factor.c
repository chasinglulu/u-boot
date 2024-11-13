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

#define FF_DRV_NAME "clk_fixed_factor"

struct clk_ff_priv {
	unsigned int div;
	unsigned int mult;
};

#if defined(CONFIG_CLK_AXERA_V1)
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
#elif defined(CONFIG_CLK_AXERA_V2)
static ulong clk_ff_set_rate_v2(struct clk *clk, ulong rate)
{
	struct udevice *dev = clk->dev;
	struct clk_ff_priv *priv = dev_get_priv(dev);
	struct clk parent;
	ulong parent_rate;
	int ret;

	ret = clk_get_by_index(dev, clk->id, &parent);
	if (ret) {
		dev_err(dev, "failed to get '%ld' clock [%d]\n", clk->id, ret);
		return 0;
	}

	parent_rate = rate * priv->div;
	do_div(parent_rate, priv->mult);

	dev_dbg(dev, "parent rate: %lu\n", parent_rate);
	parent_rate = clk_set_rate(&parent, parent_rate);
	if (parent_rate == (ulong)-ENOSYS ||
	     parent_rate == (ulong)-ENXIO)
		return 0;

	dev_dbg(dev, "rate: %lu\n", rate);
	return rate;
}

static ulong clk_ff_get_rate_v2(struct clk *clk)
{
	struct udevice *dev = clk->dev;
	struct clk_ff_priv *priv = dev_get_priv(dev);
	struct clk parent;
	ulong rate, parent_rate;
	int ret;

	ret = clk_get_by_index(dev, clk->id, &parent);
	if (ret) {
		dev_err(dev, "failed to get '%ld' clock [%d]\n", clk->id, ret);
		return 0;
	}

	parent_rate = clk_get_rate(&parent);
	if (parent_rate == (ulong)-ENOSYS ||
	     parent_rate == (ulong)-ENXIO)
		return 0;

	rate = parent_rate * priv->mult;
	do_div(rate, priv->div);

	dev_dbg(dev, "rate: %lu\n", rate);
	return rate;
}

static int clk_ff_set_parent_v2(struct clk *clk, struct clk *parent)
{
	struct udevice *dev = clk->dev;
	ulong id = clk->id;
	struct clk self_parent;
	const char *parent_name;
	int ret;

	ret = clk_get_by_index(dev, id, &self_parent);
	if (ret) {
		dev_err(dev, "failed to get '%ld' clock [%d]\n", id, ret);
		return 0;
	}

	parent_name = parent->data ? (const char *)parent->data : parent->dev->name;
	dev_dbg(dev, "self: %s, parent: %s\n", self_parent.dev->name, parent_name);
	if (!strcmp(self_parent.dev->name, parent_name))
		return 0;

	ret = clk_set_parent(&self_parent, parent);
	if (ret < 0)
		return ret;

	return 0;
}
#endif

const struct clk_ops axera_clk_ff_ops = {
#if defined(CONFIG_CLK_AXERA_V1)
	.get_rate = clk_factor_recalc_rate,
	.set_rate = clk_factor_set_rate,
	.of_xlate = clk_factor_of_xlate,
#else
	.get_rate = clk_ff_get_rate_v2,
	.set_rate = clk_ff_set_rate_v2,
	.set_parent = clk_ff_set_parent_v2,
#endif
};

static const struct udevice_id clk_ff_of_ids[] = {
	{ .compatible = "axera,lua-clk-fixed-factor" },
	{ /* sentinel */ },
};

static int clk_ff_probe(struct udevice *dev)
{
	struct clk_ff_priv *priv;

	if (device_is_compatible(dev, FF_DRV_NAME) ||
	     !device_is_compatible(dev, clk_ff_of_ids[0].compatible))
		return 0;

	priv = dev_get_priv(dev);
	priv->div = dev_read_u32_default(dev, "clock-div", 1);
	priv->mult = dev_read_u32_default(dev, "clock-mult", 1);

	return 0;
}

U_BOOT_DRIVER(axera_clk_fixed_factor) = {
	.name = FF_DRV_NAME,
	.id = UCLASS_CLK,
	.of_match = clk_ff_of_ids,
	.ops = &axera_clk_ff_ops,
	.probe = clk_ff_probe,
	.flags = DM_FLAG_PRE_RELOC,
#ifdef CONFIG_CLK_AXERA_V2
	.priv_auto = sizeof(struct clk_ff_priv),
#endif
};

#if defined(CONFIG_CLK_AXERA_V1)
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

	ret = clk_register(clk, FF_DRV_NAME, name,
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

static int fixed_factor_clk_bind(struct udevice *dev)
{
	struct ofnode_phandle_args args;
	ofnode subnode;
	struct clk *clk;
	const char *name;
	uint32_t div, mult;
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

		div = ofnode_read_u32_default(subnode, "clock-div", 1);
		mult = ofnode_read_u32_default(subnode, "clock-mult", 1);

		clk = __clk_register_fixed_factor(NULL, name,
		                    ofnode_get_name(args.node),
		                    0, mult, div);

		if (IS_ERR_OR_NULL(clk)) {
			dev_warn(dev, "Failed to register '%s' clk\n", name);
			continue;
		}

		clk->id = dev_seq(clk->dev);
		dev_set_ofnode(clk->dev, subnode);
	};

	return 0;
}

static const struct udevice_id fixed_factor_clk_of_match[] = {
	{ .compatible = "axera,lua-fixed-factor-clocks" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(fixed_factor_clk) = {
	.name = "fixed_factor_clock",
	.id = UCLASS_NOP,
	.of_match = fixed_factor_clk_of_match,
	.bind = fixed_factor_clk_bind,
};
#endif