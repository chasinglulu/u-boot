// SPDX-License-Identifier: GPL-2.0+
/*
 * Axera Laguna SoC divider clock driver
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */
#define LOG_DEBUG
#include <common.h>
#include <clk.h>
#include <clk-uclass.h>
#include <div64.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <asm/io.h>
#include <linux/log2.h>
#include <linux/bug.h>
#include <linux/clk-provider.h>
#include <linux/kernel.h>
#include <syscon.h>
#include <regmap.h>

#define CLK_DIVIVER_UPDATE_BIT   BIT(31)

static unsigned int __clk_divider_get_table_div(const struct clk_div_table *table,
				       unsigned int val)
{
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->val == val)
			return clkt->div;
	return 0;
}

static unsigned int _get_div(const struct clk_div_table *table,
			     unsigned int val, unsigned long flags, u8 width)
{
	if (flags & CLK_DIVIDER_ONE_BASED)
		return val;
	if (flags & CLK_DIVIDER_POWER_OF_TWO)
		return 1 << val;
	if (flags & CLK_DIVIDER_MAX_AT_ZERO)
		return val ? val : clk_div_mask(width) + 1;
	if (table)
		return __clk_divider_get_table_div(table, val);
	return val + 1;
}

static unsigned long __divider_recalc_rate(struct clk *hw, unsigned long parent_rate,
				  unsigned int val,
				  const struct clk_div_table *table,
				  unsigned long flags, unsigned long width)
{
	unsigned int div;

	div = _get_div(table, val, flags, width);
	if (!div) {
		WARN(!(flags & CLK_DIVIDER_ALLOW_ZERO),
		     "%s: Zero divisor and CLK_DIVIDER_ALLOW_ZERO not set\n",
		     clk_hw_get_name(hw));
		return parent_rate;
	}

	return DIV_ROUND_UP_ULL((u64)parent_rate, div);
}

static ulong clk_divider_recalc_rate(struct clk *clk)
{
	struct clk_divider *divider = to_clk_divider(clk);
	unsigned long parent_rate = clk_get_parent_rate(clk);
	unsigned int val;

	val = readl(divider->reg);
	val >>= divider->shift;
	val &= clk_div_mask(divider->width);

	return __divider_recalc_rate(clk, parent_rate, val, divider->table,
				   divider->flags, divider->width);
}

static bool _clk_divider_is_valid_table_div(const struct clk_div_table *table,
				    unsigned int div)
{
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->div == div)
			return true;
	return false;
}

static bool _clk_divider_is_valid_div(const struct clk_div_table *table,
			      unsigned int div, unsigned long flags)
{
	if (flags & CLK_DIVIDER_POWER_OF_TWO)
		return is_power_of_2(div);
	if (table)
		return _clk_divider_is_valid_table_div(table, div);
	return true;
}

static unsigned int _clk_divider_get_table_val(const struct clk_div_table *table,
				       unsigned int div)
{
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->div == div)
			return clkt->val;
	return 0;
}

static unsigned int _get_val(const struct clk_div_table *table,
			     unsigned int div, unsigned long flags, u8 width)
{
	if (flags & CLK_DIVIDER_ONE_BASED)
		return div;
	if (flags & CLK_DIVIDER_POWER_OF_TWO)
		return __ffs(div);
	if (flags & CLK_DIVIDER_MAX_AT_ZERO)
		return (div == clk_div_mask(width) + 1) ? 0 : div;
	if (table)
		return _clk_divider_get_table_val(table, div);
	return div - 1;
}

static int __divider_get_val(unsigned long rate, unsigned long parent_rate,
		    const struct clk_div_table *table, u8 width,
		    unsigned long flags)
{
	unsigned int div, value;

	div = DIV_ROUND_UP_ULL((u64)parent_rate, rate);

	if (!_clk_divider_is_valid_div(table, div, flags))
		return -EINVAL;

	value = _get_val(table, div, flags, width);

	return min_t(unsigned int, value, clk_div_mask(width));
}

static ulong clk_divider_set_rate(struct clk *clk, unsigned long rate)
{
	struct clk_divider *divider = to_clk_divider(clk);
	unsigned long parent_rate = clk_get_parent_rate(clk);
	int value;
	u32 val;

	value = __divider_get_val(rate, parent_rate, divider->table,
				divider->width, divider->flags);
	if (value < 0)
		return value;

	if (divider->flags & CLK_DIVIDER_HIWORD_MASK) {
		val = clk_div_mask(divider->width) << (divider->shift + 16);
	} else {
		val = readl(divider->reg);
		val &= ~(clk_div_mask(divider->width) << divider->shift);
	}
	val |= (u32)value << divider->shift;
	writel(val, divider->reg);

	/* Update Flags */
	if (clk->flags & CLK_DIVIVER_UPDATE_BIT) {
		val = readl(divider->reg);
		val &= ~BIT(divider->flags);
		val |= BIT(divider->flags);
		writel(val, divider->reg);
	}

	return clk_get_rate(clk);
}

const struct clk_ops ax_clk_divider_ops = {
	.get_rate = clk_divider_recalc_rate,
	.set_rate = clk_divider_set_rate,
};

static struct clk *_register_divider(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		void __iomem *reg, u8 shift, u8 width,
		u8 clk_divider_flags, const struct clk_div_table *table)
{
	struct clk_divider *div;
	struct clk *clk;
	int ret;

	if (clk_divider_flags & CLK_DIVIDER_HIWORD_MASK) {
		if (width + shift > 16) {
			dev_warn(dev, "divider value exceeds LOWORD field\n");
			return ERR_PTR(-EINVAL);
		}
	}

	/* allocate the divider */
	div = kzalloc(sizeof(*div), GFP_KERNEL);
	if (!div)
		return ERR_PTR(-ENOMEM);

	/* struct clk_divider assignments */
	div->reg = reg;
	div->shift = shift;
	div->width = width;
	div->flags = clk_divider_flags;
	div->table = table;

	/* register the clock */
	clk = &div->clk;
	clk->flags = flags;

	ret = clk_register(clk, "ax_clk_divider", name, parent_name);
	if (ret) {
		kfree(div);
		return ERR_PTR(ret);
	}

	return clk;
}

static struct clk *__clk_register_divider(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		void __iomem *reg, u8 shift, u8 width,
		u8 clk_divider_flags)
{
	struct clk *clk;

	clk =  _register_divider(dev, name, parent_name, flags, reg, shift,
				 width, clk_divider_flags, NULL);
	if (IS_ERR(clk))
		return ERR_CAST(clk);
	return clk;
}

U_BOOT_DRIVER(ax_clk_divider) = {
	.name	= "ax_clk_divider",
	.id	= UCLASS_CLK,
	.ops	= &ax_clk_divider_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

static int divider_clk_bind(struct udevice *dev)
{
	struct ofnode_phandle_args args;
	ofnode subnode;
	struct clk *clk;
	const char *name;
	void __iomem *reg;
	struct regmap *regmap;
	uint32_t offset;
	int shift, width, update;
	int ret;

	regmap = syscon_regmap_lookup_by_phandle(dev, "regmap");
	if (IS_ERR(regmap)) {
		dev_err(dev, "Failed to get regmap\n");
		return -ENODEV;
	}
	offset = dev_read_u32_default(dev, "offset", 0);
	reg = (void *)regmap->ranges->start;

	dev_for_each_subnode(subnode, dev) {
		name = ofnode_get_name(subnode);
		shift = ofnode_read_u32_default(subnode, "bit-shift", -1);
		if (shift < 0) {
			dev_warn(dev, "Failed to get 'bit-shift' in %s node\n", name);
			continue;
		}

		width = ofnode_read_u32_default(subnode, "bit-width", -1);
		if (width < 0) {
			dev_warn(dev, "Failed to get 'bit-width' in %s node\n", name);
			continue;
		}

		update = ofnode_read_u32_default(subnode, "bit-update", -1);
		if (width < 0) {
			dev_warn(dev, "Failed to get 'bit-update' in %s node\n", name);
			continue;
		}

		ret = ofnode_parse_phandle_with_args(subnode, "clocks",
							"#clock-cells", 0,
							0, &args);

		clk = __clk_register_divider(NULL, name, ofnode_get_name(args.node),
					CLK_DIVIVER_UPDATE_BIT, reg + offset,
					shift, width, 0);
		if (IS_ERR(clk)) {
			dev_warn(dev, "Failed to register '%s' clk\n", name);
			continue;
		}

		to_clk_divider(clk)->flags = update;
		dev_set_ofnode(clk->dev, subnode);
	};

	return 0;
}

static const struct udevice_id divider_clk_of_match[] = {
	{.compatible = "axera,lua-divider-clock"},
	{}
};

U_BOOT_DRIVER(divider_clk) = {
	.name = "divider_clock",
	.id = UCLASS_NOP,
	.of_match = divider_clk_of_match,
	.bind = divider_clk_bind,
};
