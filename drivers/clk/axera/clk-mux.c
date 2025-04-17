
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

#define MUX_DRV_NAME "clk_mux"

struct bit_map {
	uint32_t offset;
	uint32_t width;
};

struct clk_mux_priv {
	fdt_addr_t base;
	uint32_t offset;
	uint32_t mask;
	uint32_t clk_nums;
	struct bit_map *array;
	uint32_t *clk_parent_nums;
};

#if defined(CONFIG_CLK_AXERA_V1)
static int clk_mux_val_to_index(struct clk *clk, u32 *table,
                unsigned int flags, unsigned int val)
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

static unsigned int
__clk_mux_index_to_val(u32 *table, unsigned int flags, u8 index)
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

static int clk_mux_of_xlate(struct clk *clk,
                struct ofnode_phandle_args *args)
{
	struct clk *clkp = dev_get_uclass_priv(clk->dev);

	clk->id = dev_seq(clkp->dev);

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
#elif defined(CONFIG_CLK_AXERA_V2)
static int clk_mux_of_xlate_v2(struct clk *clk,
                struct ofnode_phandle_args *args)
{
	if (args->args_count != 1)
		return -EINVAL;

	clk->id = args->args[0];
	dev_dbg(clk->dev, "clk id: 0x%lx\n", clk->id);

	return 0;
}

static int clk_mux_set_parent_v2(struct clk *clk, struct clk *parent)
{
	struct udevice *dev = clk->dev;
	struct clk_mux_priv *priv = dev_get_priv(dev);
	struct bit_map *map;
	struct clk pclk;
	uint32_t reg;
	const char *parent_name;
	int i, parent_start = 0, parent_end;
	int ret, index = -1;

	assert(clk->id < priv->clk_nums);
	for (i = 0; i < clk->id; i++)
		parent_start += priv->clk_parent_nums[i];
	parent_end = parent_start + priv->clk_parent_nums[clk->id];
	dev_dbg(dev, "start: %d, end: %d\n", parent_start, parent_end);

	parent_name = parent->data ? (const char *)parent->data : parent->dev->name;
	dev_dbg(dev, "parent name: %s\n", parent_name);
	for (i = parent_start; i < parent_end; i++) {
		ret = clk_get_by_index(dev, i, &pclk);
		if (ret) {
			dev_err(dev, "failed to get '%d' clock [%d]\n", i, ret);
			return -EINVAL;
		}
		if (!strcmp(parent_name, pclk.dev->name)) {
			index = i;
			break;
		}
	}

	if (index < 0)
		return -ENXIO;

	for (i = 0; i < priv->clk_nums; i++) {
		if (index > priv->clk_parent_nums[i])
			index -= priv->clk_parent_nums[i];
		else
			break;
	}
	dev_dbg(dev, "clk id: %lu index %u\n", clk->id, index);

	map = priv->array + i;
	reg = readl(priv->base + priv->offset);
	reg &= ~((BIT(map->width) - 1) << map->offset);
	reg |= (index & (BIT(map->width) - 1)) << map->offset;
	writel(reg, priv->base + priv->offset);

	return 0;
}

static ulong clk_mux_set_rate_v2(struct clk *clk, ulong rate)
{
	struct udevice *dev = clk->dev;
	struct clk_mux_priv *priv = dev_get_priv(dev);
	struct bit_map *map;
	struct clk parent;
	uint32_t reg, index = 0;
	ulong freq;
	int count;
	int ret, i;

	if (!dev)
		return -ENXIO;

	count = dev_read_string_count(dev, "clock-output-names");
	assert(clk->id < count);

	for (i = 0; i < clk->id; i++)
		index += priv->clk_parent_nums[i];

	map = priv->array + clk->id;
	reg = readl(priv->base + priv->offset);
	index += (reg >> map->offset) & (BIT(map->width) - 1);
	dev_dbg(dev, "index %u\n", index);

	ret = clk_get_by_index(dev, index, &parent);
	if (ret) {
		dev_err(dev, "failed to get '%d' clock [%d]\n", index, ret);
		return 0;
	}

	freq = clk_set_rate(&parent, rate);
	if (rate == (ulong)-ENOSYS ||
	    rate == (ulong)-ENXIO)
		return 0;

	dev_dbg(dev, "new freq: %lu\n", freq);
	return rate;
}

static ulong clk_mux_get_rate_v2(struct clk *clk)
{
	struct udevice *dev = clk->dev;
	struct clk_mux_priv *priv = dev_get_priv(dev);
	struct bit_map *map;
	struct clk parent;
	uint32_t reg, index = 0;
	ulong rate;
	int count;
	int ret, i;

	if (!dev)
		return -ENXIO;

	count = dev_read_string_count(dev, "clock-output-names");
	assert(clk->id < count);

	for (i = 0; i < clk->id; i++)
		index += priv->clk_parent_nums[i];

	map = priv->array + clk->id;
	reg = readl(priv->base + priv->offset);
	index += (reg >> map->offset) & (BIT(map->width) - 1);
	dev_dbg(dev, "index %u\n", index);

	ret = clk_get_by_index(dev, index, &parent);
	if (ret) {
		dev_err(dev, "failed to get '%d' clock [%d]\n", index, ret);
		return 0;
	}

	rate = clk_get_rate(&parent);
	if (rate == (ulong)-ENOSYS ||
	    rate == (ulong)-ENXIO)
		return 0;

	dev_dbg(dev, "rate: %lu\n", rate);
	return rate;
}
#endif

const struct clk_ops axera_clk_mux_ops = {
#if defined(CONFIG_CLK_AXERA_V1)
	.set_parent = clk_mux_set_parent,
	.get_rate = clk_generic_get_rate,
	.set_rate = clk_mux_set_rate,
	.of_xlate = clk_mux_of_xlate,
#elif defined(CONFIG_CLK_AXERA_V2)
	.set_parent = clk_mux_set_parent_v2,
	.get_rate = clk_mux_get_rate_v2,
	.set_rate = clk_mux_set_rate_v2,
	.of_xlate = clk_mux_of_xlate_v2,
#endif
};

static const struct udevice_id clk_mux_of_ids[] = {
	{ .compatible = "axera,lua-clock-mux" },
	{ /* sentinel */ },
};

static int clk_mux_probe(struct udevice *dev)
{
	struct clk_mux_priv *priv;
	ofnode regmap;
	uint32_t phandle;
	int ret;

	if (device_is_compatible(dev, MUX_DRV_NAME) ||
	     !device_is_compatible(dev, clk_mux_of_ids[0].compatible))
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
	dev_read_u32_array(dev, "bit-map", (uint32_t *)priv->array, priv->clk_nums * 2);

	priv->clk_parent_nums = kzalloc(priv->clk_nums * sizeof(uint32_t), GFP_KERNEL);
	if (IS_ERR_OR_NULL(priv->array)) {
		dev_err(dev, "Out of memory\n");
		ret = -ENOMEM;
		goto failed;
	}
	dev_read_u32_array(dev, "clock-parent-nums", priv->clk_parent_nums, priv->clk_nums);

	dev_dbg(dev, "Probe sucessfully\n");
	return 0;

failed:
	free(priv->array);
	return ret;
}

U_BOOT_DRIVER(axera_clk_mux) = {
	.name = MUX_DRV_NAME,
	.id = UCLASS_CLK,
	.of_match = clk_mux_of_ids,
	.probe = clk_mux_probe,
	.ops = &axera_clk_mux_ops,
	.flags = DM_FLAG_PRE_RELOC,
#ifdef CONFIG_CLK_AXERA_V2
	.priv_auto = sizeof(struct clk_mux_priv),
#endif
};

#if defined(CONFIG_CLK_AXERA_V1)
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
	ret = clk_register(clk, MUX_DRV_NAME, name,
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

static int mux_clk_bind(struct udevice *dev)
{
	struct ofnode_phandle_args args;
	int shift, width, count;
	ofnode subnode, regmap;
	const char *name, **parents;
	uint32_t phandle, offset;
	struct clk *clk;
	fdt_addr_t base;
	ulong flags = 0;
	int i, ret;

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

	offset = dev_read_u32_default(dev, "offset", -1U);
	if (offset == ~0U) {
		dev_err(dev, "Failed to get 'offset' property\n");
		return -ENODATA;
	}

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
			ret = ofnode_parse_phandle_with_args(subnode, "clocks",
			            "#clock-cells", 0,
			            i, &args);

			parents[i] = ofnode_get_name(args.node);
		}

		if (dev_read_bool(dev, "axera,set-rate-parent"))
			flags |= CLK_SET_RATE_PARENT;

		clk = __clk_register_mux(NULL, name, parents,
		            count, flags, (void *)base + offset,
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
	{ /* sentinel */ },
};

U_BOOT_DRIVER(mux_clk) = {
	.name = "mux_clock",
	.id = UCLASS_NOP,
	.of_match = mux_clk_of_match,
	.bind = mux_clk_bind,
};
#endif
