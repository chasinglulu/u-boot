// SPDX-License-Identifier: GPL-2.0+
/*
 * Axera Laguna SoC divider clock driver
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

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
#include <linux/delay.h>

#define DIV_DRV_NAME              "clk_div"
#define CLK_DIVIVER_UPDATE_BIT    BIT(31)

struct bit_map {
	uint32_t offset;
	uint32_t width;
	uint32_t update;
};

struct clk_div_priv {
	fdt_addr_t base;
	uint32_t offset;
	uint32_t mask;
	uint32_t clk_nums;
	struct bit_map *array;
};

#if defined(CONFIG_CLK_AXERA_V1)
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

static ulong clk_divider_get_rate(struct clk *clk)
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
#elif defined(CONFIG_CLK_AXERA_V2)
static int clk_divider_of_xlate_v2(struct clk *clk,
                struct ofnode_phandle_args *args)
{
	if (args->args_count != 1)
		return -EINVAL;

	clk->id = args->args[0];
	dev_dbg(clk->dev, "clk id: 0x%lx\n", clk->id);

	return 0;
}

static ulong clk_divider_set_rate_v2(struct clk *clk, unsigned long rate)
{
	struct udevice *dev = clk->dev;
	struct clk_div_priv *priv = dev_get_priv(dev);
	struct bit_map *map;
	struct clk parent;
	uint64_t parent_rate, freq;
	int ret, count;
	uint32_t reg;
	uint16_t divn;

	count = dev_read_string_count(dev, "clock-output-names");
	assert(clk->id < count);

	ret = clk_get_by_index(dev, clk->id, &parent);
	if (ret) {
		dev_err(dev, "failed to get '%ld' clock [%d]\n", clk->id, ret);
		return 0;
	}

	dev_dbg(dev, "parent_rate: %llu\n", parent_rate);
	parent_rate = clk_get_rate(&parent);
	if (parent_rate == (ulong)-ENOSYS ||
	      parent_rate == (ulong)-ENXIO)
		return 0;

	divn = DIV_ROUND_UP_ULL(parent_rate, rate);
	divn = divn > 1 ? (divn - 1) : 0;

	map = priv->array + clk->id;
	reg = readl(priv->base + priv->offset);
	reg &= ~((BIT(map->width) - 1) << map->offset);
	reg |= (divn & (BIT(map->width) - 1)) << map->offset;
	reg |= BIT(map->update);
	writel(reg, priv->base + priv->offset);
	udelay(10);
	reg = readl(priv->base + priv->offset);
	reg &= ~BIT(map->update);
	writel(reg, priv->base + priv->offset);

	freq = clk_get_rate(clk);
	if (freq == (ulong)-ENOSYS ||
	      freq == (ulong)-ENXIO)
		return 0;

	dev_dbg(dev, "rate: %llu\n", freq);
	return freq;
}

static ulong clk_divider_get_rate_v2(struct clk *clk)
{
	struct udevice *dev = clk->dev;
	struct clk_div_priv *priv = dev_get_priv(dev);
	struct bit_map *map;
	struct clk parent;
	uint64_t rate;
	int ret, count;
	uint32_t reg;
	uint16_t divn;

	count = dev_read_string_count(dev, "clock-output-names");
	assert(clk->id < count);

	ret = clk_get_by_index(dev, clk->id, &parent);
	if (ret) {
		dev_err(dev, "failed to get '%ld' clock [%d]\n", clk->id, ret);
		return 0;
	}

	rate = clk_get_rate(&parent);
	if (rate == (ulong)-ENOSYS ||
	      rate == (ulong)-ENXIO)
		return 0;

	map = priv->array + clk->id;
	reg = readl(priv->base + priv->offset);
	divn = ((reg >> map->offset) & (BIT(map->width) - 1)) + 1;
	do_div(rate, divn);

	dev_dbg(dev, "rate: %llu\n", rate);
	return rate;
}

static int clk_divider_set_parent_v2(struct clk *clk, struct clk *parent)
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

const struct clk_ops axera_clk_div_ops = {
#if defined(CONFIG_CLK_AXERA_V1)
	.get_rate = clk_divider_get_rate,
	.set_rate = clk_divider_set_rate,
#elif defined(CONFIG_CLK_AXERA_V2)
	.get_rate = clk_divider_get_rate_v2,
	.set_rate = clk_divider_set_rate_v2,
	.set_parent = clk_divider_set_parent_v2,
	.of_xlate = clk_divider_of_xlate_v2,
#endif
};

static const struct udevice_id clk_div_of_ids[] = {
	{ .compatible = "axera,lua-clock-div" },
	{ /* sentinel */ },
};

static int clk_divider_probe(struct udevice *dev)
{
	struct clk_div_priv *priv;
	ofnode regmap;
	uint32_t phandle;
	int ret;

	if (device_is_compatible(dev, DIV_DRV_NAME) ||
	     !device_is_compatible(dev, clk_div_of_ids[0].compatible))
		return 0;

	priv = dev_get_priv(dev);
	ret = ofnode_read_u32(dev_ofnode(dev), "regmap", &phandle);
	if (ret) {
		dev_err(dev, "Failed to find 'regmap' property\n");
		return ret;
	}
	regmap = ofnode_get_by_phandle(phandle);
	if (!ofnode_valid(regmap)) {
		dev_dbg(dev, "unable to find regmap phandle\n");
		return -EINVAL;
	}
	priv->base = ofnode_get_addr(regmap);

	priv->offset = dev_read_u32_default(dev, "offset", -1);
	if (priv->offset == ~0U) {
		dev_err(dev, "Failed to get 'offset' property\n");
		return -ENODATA;
	}

	priv->mask = dev_read_u32_default(dev, "mask", 0);
	if (!priv->mask) {
		dev_err(dev, "Failed to get 'mask' property\n");
		return -ENODATA;
	}

	priv->clk_nums = dev_read_string_count(dev, "clock-output-names");
	dev_dbg(dev, "clk total: %d\n", priv->clk_nums);
	assert(priv->clk_nums > 0);

	priv->array = kzalloc(priv->clk_nums * sizeof(struct bit_map), GFP_KERNEL);
	if (IS_ERR_OR_NULL(priv->array)) {
		dev_err(dev, "Out of memory\n");
		return -ENOMEM;
	}
	dev_read_u32_array(dev, "bit-map", (uint32_t *)priv->array, priv->clk_nums * 3);

	return 0;
}

U_BOOT_DRIVER(ax_clk_divider) = {
	.name = DIV_DRV_NAME,
	.id = UCLASS_CLK,
	.of_match = clk_div_of_ids,
	.ops = &axera_clk_div_ops,
	.probe = clk_divider_probe,
	.flags = DM_FLAG_PRE_RELOC,
#ifdef CONFIG_CLK_AXERA_V2
	.priv_auto = sizeof(struct clk_div_priv),
#endif
};

#if defined(CONFIG_CLK_AXERA_V1)
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

	ret = clk_register(clk, DIV_DRV_NAME, name, parent_name);
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

	clk = _register_divider(dev, name, parent_name, flags, reg, shift,
	                width, clk_divider_flags, NULL);
	if (IS_ERR(clk))
		return ERR_CAST(clk);
	return clk;
}

static int divider_clk_bind(struct udevice *dev)
{
	struct ofnode_phandle_args args;
	int shift, width, update;
	ofnode subnode, regmap;
	const char *name;
	uint32_t phandle, offset;
	struct clk *clk;
	fdt_addr_t base;
	int ret;

	ret = ofnode_read_u32(dev_ofnode(dev), "regmap", &phandle);
	if (ret) {
		dev_err(dev, "Failed to find 'regmap' property\n");
		return ret;
	}

	regmap = ofnode_get_by_phandle(phandle);
	if (!ofnode_valid(regmap)) {
		dev_dbg(dev, "unable to find regmap phandle\n");
		return -EINVAL;
	}
	base = ofnode_get_addr(regmap);

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
		               CLK_DIVIVER_UPDATE_BIT, (void *)base + offset,
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
	{ .compatible = "axera,lua-divider-clocks" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(divider_clk) = {
	.name = "divider_clock",
	.id = UCLASS_NOP,
	.of_match = divider_clk_of_match,
	.bind = divider_clk_bind,
};
#endif
