
// SPDX-License-Identifier: GPL-2.0+
/*
 * Axera Laguna SoC mux clock driver
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <clk-uclass.h>
#include <asm/io.h>
#include <linux/clk-provider.h>
#include <syscon.h>
#include <regmap.h>

static int clk_mux_val_to_index(struct clk *clk, u32 *table, unsigned int flags,
			 unsigned int val)
{
	struct clk_mux *mux = to_clk_mux(clk);
	int num_parents = mux->num_parents;

	if (table) {
		int i;

		for (i = 0; i < num_parents; i++)
			if (table[i] == val)
				return i;
		return -EINVAL;
	}

	if (val && (flags & CLK_MUX_INDEX_BIT))
		val = ffs(val) - 1;

	if (val && (flags & CLK_MUX_INDEX_ONE))
		val--;

	if (val >= num_parents)
		return -EINVAL;

	return val;
}

static unsigned int __clk_mux_index_to_val(u32 *table, unsigned int flags, u8 index)
{
	unsigned int val = index;

	if (table) {
		val = table[index];
	} else {
		if (flags & CLK_MUX_INDEX_BIT)
			val = 1 << index;

		if (flags & CLK_MUX_INDEX_ONE)
			val++;
	}

	return val;
}

static u8 __clk_mux_get_parent(struct clk *clk)
{
	struct clk_mux *mux = to_clk_mux(clk);
	u32 val;

	val = readl(mux->reg);
	val >>= mux->shift;
	val &= mux->mask;

	return clk_mux_val_to_index(clk, mux->table, mux->flags, val);
}

static int clk_fetch_parent_index(struct clk *clk,
				  struct clk *parent)
{
	struct clk_mux *mux = to_clk_mux(clk);

	int i;

	if (!parent)
		return -EINVAL;

	for (i = 0; i < mux->num_parents; i++) {
		if (!strcmp(parent->dev->name, mux->parent_names[i]))
			return i;
	}

	return -EINVAL;
}

static int clk_mux_set_parent(struct clk *clk, struct clk *parent)
{
	struct clk *clkp = dev_get_uclass_priv(clk->dev);
	struct clk_mux *mux = to_clk_mux(clkp);
	int index;
	u32 val;
	u32 reg;

	index = clk_fetch_parent_index(clk, parent);
	if (index < 0) {
		log_err("Could not fetch index\n");
		return index;
	}

	val = __clk_mux_index_to_val(mux->table, mux->flags, index);

	if (mux->flags & CLK_MUX_HIWORD_MASK) {
		reg = mux->mask << (mux->shift + 16);
	} else {
		reg = readl(mux->reg);
		reg &= ~(mux->mask << mux->shift);
	}
	val = val << mux->shift;
	reg |= val;

	writel(reg, mux->reg);
	return 0;
}

static ulong clk_mux_set_rate(struct clk *clk, ulong rate)
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

static ulong clk_mux_round_rate(struct clk *clk, ulong rate)
{
	struct clk *parent;

	if ((clk->flags & CLK_SET_RATE_PARENT) == 0)
		return -ENOSYS;

	parent = clk_get_parent(clk);
	if (IS_ERR(parent))
		return PTR_ERR(parent);

	rate = clk_round_rate(parent, rate);
	dev_dbg(clk->dev, "rate=%ld\n", rate);
	return rate;
}

static int clk_mux_of_xlate(struct clk *clk,
			struct ofnode_phandle_args *args)
{
	struct clk *clkp = dev_get_uclass_priv(clk->dev);

	clk->id = dev_seq(clkp->dev);

	return 0;
}

const struct clk_ops ax_clk_mux_ops = {
	.get_rate = clk_generic_get_rate,
	.set_parent = clk_mux_set_parent,
	.set_rate = clk_mux_set_rate,
	.round_rate = clk_mux_round_rate,
	.of_xlate = clk_mux_of_xlate,
};

static struct clk *__clk_hw_register_mux_table(struct device *dev, const char *name,
		const char * const *parent_names, u8 num_parents,
		unsigned long flags,
		void __iomem *reg, u8 shift, u32 mask,
		u8 clk_mux_flags, u32 *table)
{
	struct clk_mux *mux;
	struct clk *clk;
	u8 width = 0;
	int ret;

	if (clk_mux_flags & CLK_MUX_HIWORD_MASK) {
		width = fls(mask) - ffs(mask) + 1;
		if (width + shift > 16) {
			dev_err(dev, "mux value exceeds LOWORD field\n");
			return ERR_PTR(-EINVAL);
		}
	}

	/* allocate the mux */
	mux = kzalloc(sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	/* U-boot specific assignments */
	mux->parent_names = parent_names;
	mux->num_parents = num_parents;

	/* struct clk_mux assignments */
	mux->reg = reg;
	mux->shift = shift;
	mux->mask = mask;
	mux->flags = clk_mux_flags;
	mux->table = table;

	clk = &mux->clk;
	clk->flags = flags;

	/*
	 * Read the current mux setup - so we assign correct parent.
	 *
	 * Changing parent would require changing internals of udevice struct
	 * for the corresponding clock (to do that define .set_parent() method).
	 */
	ret = clk_register(clk, "ax_clk_mux", name,
			   parent_names[__clk_mux_get_parent(clk)]);
	if (ret) {
		kfree(mux);
		return ERR_PTR(ret);
	}

	return clk;
}

static struct clk *clk_register_mux_table(struct device *dev, const char *name,
		const char * const *parent_names, u8 num_parents,
		unsigned long flags,
		void __iomem *reg, u8 shift, u32 mask,
		u8 clk_mux_flags, u32 *table)
{
	struct clk *clk;

	clk = __clk_hw_register_mux_table(dev, name, parent_names, num_parents,
				       flags, reg, shift, mask, clk_mux_flags,
				       table);
	if (IS_ERR(clk))
		return ERR_CAST(clk);
	return clk;
}

static struct clk *__clk_register_mux(struct device *dev, const char *name,
		const char * const *parent_names, u8 num_parents,
		unsigned long flags,
		void __iomem *reg, u8 shift, u8 width,
		u8 clk_mux_flags)
{
	u32 mask = BIT(width) - 1;

	return clk_register_mux_table(dev, name, parent_names, num_parents,
				      flags, reg, shift, mask, clk_mux_flags,
				      NULL);
}

U_BOOT_DRIVER(ax_clk_mux) = {
	.name = "ax_clk_mux",
	.id = UCLASS_CLK,
	.ops = &ax_clk_mux_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

static int mux_clk_bind(struct udevice *dev)
{
	ofnode subnode;
	const char *name;
	struct clk *clk;
	void __iomem *reg;
	int shift, width, count;
	const char ** parents;
	struct regmap *regmap;
	ulong flags = 0;
	int offset;
	int i;

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

		count = ofnode_count_phandle_with_args(subnode, "clocks",
							"#clock-cells", 0);
		parents = kzalloc(count * sizeof(parents), GFP_KERNEL);

		for (i = 0; i < count; i++) {
			struct ofnode_phandle_args args;
			int ret;

			ret = ofnode_parse_phandle_with_args(subnode, "clocks",
								"#clock-cells", 0,
								i, &args);

			parents[i] = ofnode_get_name(args.node);
		}

		if (dev_read_bool(dev, "axera,set-rate-parent"))
			flags |= CLK_SET_RATE_PARENT;

		clk = __clk_register_mux(NULL, name, parents,
				count, flags, reg + offset,
				shift, width, 0);
		if (IS_ERR(clk))
			dev_warn(dev, "Failed to register '%s' clk\n", name);

		clk->id = dev_seq(clk->dev);
		dev_set_ofnode(clk->dev, subnode);
		kfree(parents);
	};

	return 0;
}

static const struct udevice_id mux_clk_of_match[] = {
	{ .compatible = "axera,lua-mux-clocks" },
	{ },
};

U_BOOT_DRIVER(mux_clk) = {
	.name = "mux_clock",
	.id = UCLASS_NOP,
	.of_match = mux_clk_of_match,
	.bind = mux_clk_bind,
};
