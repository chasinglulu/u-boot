// SPDX-License-Identifier: GPL-2.0+
/*
 * Axera Laguna SoC gate clock support
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

static void clk_gate_endisable(struct clk *clk, int enable)
{
	struct clk *clkp = dev_get_uclass_priv(clk->dev);
	struct clk_gate *gate = to_clk_gate(clkp);
	int set = gate->flags & CLK_GATE_SET_TO_DISABLE ? 1 : 0;
	u32 reg;

	set ^= enable;

	if (gate->flags & CLK_GATE_HIWORD_MASK) {
		reg = BIT(gate->bit_idx + 16);
		if (set)
			reg |= BIT(gate->bit_idx);
	} else {
		reg = readl(gate->reg);

		if (set)
			reg |= BIT(gate->bit_idx);
		else
			reg &= ~BIT(gate->bit_idx);
	}

	writel(reg, gate->reg);
}

static int clk_gate_enable(struct clk *clk)
{
	clk_gate_endisable(clk, 1);

	return 0;
}

static int clk_gate_disable(struct clk *clk)
{
	clk_gate_endisable(clk, 0);

	return 0;
}

static ulong clk_gate_set_rate(struct clk *clk, ulong rate)
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

static int clk_gate_of_xlate(struct clk *clk,
			struct ofnode_phandle_args *args)
{
	struct clk *clkp = dev_get_uclass_priv(clk->dev);

	clk->id = dev_seq(clkp->dev);

	return 0;
}

const struct clk_ops ax_clk_gate_ops = {
	.enable = clk_gate_enable,
	.disable = clk_gate_disable,
	.get_rate = clk_generic_get_rate,
	.set_rate = clk_gate_set_rate,
	.of_xlate = clk_gate_of_xlate,
};

static struct clk *__clk_register_gate(struct device *dev, const char *name,
			      const char *parent_name, unsigned long flags,
			      void __iomem *reg, u8 bit_idx,
			      u8 clk_gate_flags, spinlock_t *lock)
{
	struct clk_gate *gate;
	struct clk *clk;
	int ret;

	if (clk_gate_flags & CLK_GATE_HIWORD_MASK) {
		if (bit_idx > 15) {
			dev_err(dev, "gate bit exceeds LOWORD field\n");
			return ERR_PTR(-EINVAL);
		}
	}

	/* allocate the gate */
	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate)
		return ERR_PTR(-ENOMEM);

	gate->reg = reg;
	gate->bit_idx = bit_idx;
	gate->flags = clk_gate_flags;

	clk = &gate->clk;
	clk->flags = flags;

	ret = clk_register(clk, "ax_clk_gate", name, parent_name);
	if (ret) {
		kfree(gate);
		return ERR_PTR(ret);
	}

	return clk;
}

U_BOOT_DRIVER(ax_clk_gate) = {
	.name = "ax_clk_gate",
	.id = UCLASS_CLK,
	.ops = &ax_clk_gate_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

static int gate_clk_bind(struct udevice *dev)
{
	struct ofnode_phandle_args args;
	ofnode subnode;
	struct clk *clk;
	const char *name;
	void __iomem *reg;
	struct regmap *regmap;
	uint32_t offset;
	int bit_idx;
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
		bit_idx = ofnode_read_u32_default(subnode, "bit-shift", -1);
		if (bit_idx < 0) {
			dev_warn(dev, "Failed to get 'bit-shift' in %s node\n", name);
			continue;
		}

		ret = ofnode_parse_phandle_with_args(subnode, "clocks",
							"#clock-cells", 0,
							0, &args);

		clk = __clk_register_gate(NULL, name, ofnode_get_name(args.node),
				0, reg + offset, bit_idx, 0, NULL);
		if (IS_ERR(clk)) {
			dev_warn(dev, "Failed to register '%s' clk\n", name);
			continue;
		}

		clk->id = dev_seq(clk->dev);
		dev_set_ofnode(clk->dev, subnode);
	};

	return 0;
}

static const struct udevice_id gate_clk_of_match[] = {
	{ .compatible = "axera,lua-gate-clocks" },
	{ },
};

U_BOOT_DRIVER(gate_clk) = {
	.name = "gate_clock",
	.id = UCLASS_NOP,
	.of_match = gate_clk_of_match,
	.bind = gate_clk_bind,
};
