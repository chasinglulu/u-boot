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
#include <dm/read.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <div64.h>

#define PLL_DRV_NAME "clk_pll"

enum {
	PLL_REG,
	PLL_ON_CFG,
	PLL_RDY,
	PLL_FRC_EN,
	PLL_FRC_EN_SW,
	PLL_REOPEN,
	PLL_REG_CNT,
};

#define PLL_STAT_DIGCK_MISS     BIT(6)
#define PLL_STAT_FBKCK_MISS     BIT(5)
#define PLL_STAT_REFCK_MISS     BIT(4)
#define PLL_STAT_FM_UNDER       BIT(3)
#define PLL_STAT_FM_OVER        BIT(2)
#define PLL_STAT_FM_CPLT        BIT(1)
#define PLL_STAT_LOCKED         BIT(0)

#define PLL_CFG0_FBK_FRA_MASK     GENMASK(25, 2)
#define PLL_CFG0_FBK_FRA_SHIFT    2
#define PLL_CFG0_FBK_CHG          BIT(1)
#define PLL_CFG0_BYP_MODE         BIT(0)

#define PLL_CFG1_CKMON_EN         BIT(25)
#define PLL_CFG1_POST_DIV_MASK    GENMASK(24, 23)
#define PLL_CFG1_POST_DIV_SHIFT   23
#define PLL_CFG1_WATCHDOG_EN      BIT(22)
#define PLL_CFG1_TEST_SEL_MASK    GENMASK(21, 19)
#define PLL_CFG1_TEST_SEL_SHIFT   19
#define PLL_CFG1_PRE_DIV_MASK     GENMASK(18, 17)
#define PLL_CFG1_PRE_DIV_SHIFT    17
#define PLL_CFG1_MON_CTRL_MASK    GENMASK(16, 15)
#define PLL_CFG1_MON_CTRL_SHIFT   15
#define PLL_CFG1_LDO_STB_X2_EN    BIT(14)
#define PLL_CFG1_LDO_STB_MASK     GENMASK(13, 12)
#define PLL_CFG1_LDO_STB_SHIFT    12
#define PLL_CFG1_FM_TOR_MASK      GENMASK(11, 10)
#define PLL_CFG1_FM_TOR_SHIFT     10
#define PLL_CFG1_FM_EN            BIT(9)
#define PLL_CFG1_FBK_INT_MASK     GENMASK(8, 0)
#define PLL_CFG1_FBK_INT_SHIFT    0

struct pll_reg {
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

static const char *pll_prop[] = {
	"pll-reg-offset",
	"pll-on-cfg",
	"pll-rdy-bit",
	"pll-frc-en-bit",
	"pll-frc-en-sw-bit",
	"pll-reopen-bit",
};

static const char *pll_reg_prop[] = {
	"pll-ready",
	"pll-frc-en",
	"pll-frc-en-sw",
	"pll-reopen",
};

/*
 * Calculation formula for frequency:
 *
 * fpfd = fref / 2 ^ (PRE_DIV[1:0])
 * fpll = fpfd * (FBK_INT[8:0] + FBK_FRA[23:0] / 2 ^ 24)
 * fvco = fpll / 2 ^ (POST_DIV[1:0])
 *
 */
static __maybe_unused
ulong pll_clac_rate(struct pll_reg *reg, ulong parent_rate)
{
	uint8_t pre_div, post_div;
	uint32_t cfg;
	uint16_t fbk_int;
	uint32_t fbk_fra;
	ulong rate;

	cfg = readl(&reg->cfg1);

	pre_div = (cfg & PLL_CFG1_PRE_DIV_MASK) >> PLL_CFG1_PRE_DIV_SHIFT;
	post_div = (cfg & PLL_CFG1_POST_DIV_MASK) >> PLL_CFG1_POST_DIV_SHIFT;
	fbk_int = (cfg & PLL_CFG1_FBK_INT_MASK) >> PLL_CFG1_FBK_INT_SHIFT;

	cfg = readl(&reg->cfg0);
	fbk_fra = (cfg & PLL_CFG0_FBK_FRA_MASK) >> PLL_CFG0_FBK_FRA_SHIFT;

	rate = parent_rate >> pre_div;
	rate = rate * (fbk_int + (fbk_fra >> 24));
	rate >>= post_div;

	return rate;
}

static __maybe_unused
ulong pll_fixed_rate(ulong base, int id)
{
	ulong rate = 0;

	base &= 0xFFFFF000;

	if (base == 0x18001000) {
		/* main pllctrl */
		switch (id) {
		case 0: // fab_pll
			rate = 2400000000UL;
			break;
		case 1: // cpu_pll
			rate = 1296000000UL;
			break;
		case 2: // npu_pll
			rate = 996000000UL;
			break;
		case 3: // apll
			rate = 2700000000UL;
			break;
		case 4: // epll
			rate = 1500000000UL;
			break;
		}
	} else if (base == 0x006C2000) {
		/* safety pllctrl */
		switch (id) {
		case 0: // safe_epll
			rate = 1500000000UL;
			break;
		case 1: // safe_rpll
			rate = 996000000UL;
			break;
		case 2: // safe_fab_pll
			rate = 2400000000UL;
			break;
		}
	} else {
		pr_err("Not supported PLL Controller device (base = 0x%lx)\n", base);
	}

	return rate;
}

#if defined(CONFIG_CLK_AXERA_V1)
struct pll_clock_priv {
	/*
	 * 0: pll_ctrlreg address
	 * 1: pll_on_cfg address
	 * 2: pll_rdy address
	 * 3: pll_frc_en address
	 * 4: pll_frc_en_sw address
	 * 5: pll_reopen address
	 */
	void __iomem *addr[PLL_REG_CNT];
	uint8_t rdy_bit;
	uint8_t frc_en_bit;
	uint8_t frc_en_sw_bit;
	uint8_t reopen_bit;
};

struct pll_clock_plat {
	uint8_t pll_count;
	struct pll_clock_priv *pll_priv;
};

struct lua_pll_clk {
	struct clk clk;
	struct pll_clock_priv *priv;
};
#define to_pll_clk(_clk) container_of(_clk, struct lua_pll_clk, clk)

ulong pll_clk_set_rate(struct clk *clk, ulong rate)
{
#if !defined(CONFIG_LUA_PLL_SET_RATE) || defined(CONFIG_LUA_PLL_FIXED_FREQ)
	/* FIXME: TODO */
	pr_err("Not allow seting PLL rate.\n");
#else
	struct lua_pll_clk *pll = to_pll_clk(clk);
	struct pll_clock_priv *priv = pll->priv;
	struct pll_reg *reg = priv->addr[PLL_REG];
	ulong parent_rate = clk_get_parent_rate(clk);;
	ulong dividend, remainder;
	uint32_t cfg;

	/* clear reopen and on bit */
	clrbits_le32(priv->addr[PLL_REOPEN], BIT(priv->reopen_bit));
	clrbits_le32(priv->addr[PLL_ON_CFG], BIT(0));

	/*
	 * FBK_INT configure the integer part of frequency
	 * FBK_FRA configure the fraction part of frequency
	 *    - Currently not supported
	 */

	/* Clear POST_DIV, PRE_DIV and FBK_INT */
	cfg = readl(&reg->cfg1);
	cfg &= ~(PLL_CFG1_POST_DIV_MASK | PLL_CFG1_PRE_DIV_MASK | PLL_CFG1_FBK_INT_MASK);
	writel(cfg, &reg->cfg1);

	dividend = rate;
	remainder = do_div(dividend, parent_rate);

	/*
	 * four case:
	 * 1. PRE_DIV = 0, POST_DIV = 0
	 * 2. PRE_DIV = 1, POST_DIV = 0
	 * 3. PRE_DIV = 0, POST_DIV = 1
	 * 2. PRE_DIV = 1, POST_DIV = 1
	 *
	 */
	cfg = readl(&reg->cfg1);
	cfg |= dividend;
	writel(cfg, &reg->cfg1);

	/* set on bit*/
	setbits_le32(priv->addr[PLL_ON_CFG], BIT(0));

	/* wait for LOCKED */
	while (true) {
		if (readl(&reg->stat) & BIT(0))
			break;

		udelay(10);
	}

	while (true) {
		if (readl(priv->addr[PLL_RDY]) & BIT(priv->rdy_bit))
			break;

		udelay(10);
	}

	/* set reopen bit */
	setbits_le32(priv->addr[PLL_REOPEN], BIT(priv->reopen_bit));
#endif

	return 0;
}

ulong pll_clk_get_rate(struct clk *clk)
{
	struct lua_pll_clk *pll = to_pll_clk(clk);
	struct pll_clock_priv *priv = pll->priv;
	ulong rate = 0UL;

#if defined(CONFIG_LUA_PLL_FIXED_FREQ)
	ulong base = (ulong)priv->addr[PLL_REG];
	rate = pll_fixed_rate(base, priv->rdy_bit);
#else
	struct pll_reg *reg = priv->addr[PLL_REG];
	unsigned long parent_rate = clk_get_parent_rate(clk);

	rate = pll_clac_rate(reg, parent_rate);
#endif

	return rate;
}

static int pll_clk_of_xlate(struct clk *clk,
			struct ofnode_phandle_args *args)
{
	struct clk *clkp = dev_get_uclass_priv(clk->dev);

	clk->id = dev_seq(clkp->dev);

	return 0;
}
#elif defined (CONFIG_CLK_AXERA_V2)
struct pll_clk_priv {
	fdt_addr_t base;
	uint32_t pll_cnt;
	uint32_t *cfg_map[PLL_REG_CNT];
	uint32_t reg_offset[PLL_REG_CNT - PLL_RDY];
};

ulong pll_clk_set_rate_v2(struct clk *clk, ulong rate)
{
	/* FIXME: TODO */
	pr_err("Not allow seting PLL rate.\n");

	return 0;
}

ulong pll_clk_get_rate_v2(struct clk *clk)
{
	struct udevice *dev = clk->dev;
	struct pll_clk_priv *priv = dev_get_priv(dev);
	ulong rate;
	ulong id;

	if (unlikely(!dev) || unlikely(!priv))
		return -ENXIO;
	assert(clk->id < priv->pll_cnt);
	id = clk->id;

#if defined(CONFIG_LUA_PLL_FIXED_FREQ)
	rate = pll_fixed_rate(priv->base, id);
#else
	struct clk parent;
	struct pll_reg *reg;
	unsigned long parent_rate;
	int ret;

	ret = clk_get_by_index(dev, id, &parent);
	if (ret) {
		dev_err(dev, "failed to get '%ld' clock [%d]\n", id, ret);
		return 0;
	}

	parent_rate = clk_get_rate(&parent);
	if (parent_rate == (ulong)-ENOSYS ||
	    parent_rate == (ulong)-ENXIO)
		return 0;

	reg = (void *)priv->base + priv->cfg_map[PLL_REG][id];
	rate = pll_clac_rate(reg, parent_rate);
#endif

	return rate;
}

static int pll_clk_of_xlate_v2(struct clk *clk,
                struct ofnode_phandle_args *args)
{
	if (args->args_count != 1)
		return -EINVAL;

	clk->id = args->args[0];
	dev_dbg(clk->dev, "clk id: 0x%lx\n", clk->id);

	return 0;
}

static const struct udevice_id pll_clk_of_ids[] = {
	{ .compatible = "axera,lua-pll-clock" },
	{ /* sentinel */ },
};

static int pll_clk_probe(struct udevice *dev)
{
	struct pll_clk_priv *priv = dev_get_priv(dev);
	ulong size;
	int i, ret;

	if (device_is_compatible(dev, PLL_DRV_NAME) ||
	     !device_is_compatible(dev, pll_clk_of_ids[0].compatible))
		return 0;

	priv->base = dev_read_addr(dev);

	priv->pll_cnt = dev_read_string_count(dev, "clock-output-names");
	dev_dbg(dev, "PLL count: %d\n", priv->pll_cnt);
	assert(priv->pll_cnt > 0);

	size = priv->pll_cnt * sizeof(uint32_t) * PLL_REG_CNT;
	priv->cfg_map[PLL_REG] = kzalloc(size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(priv->cfg_map[0])) {
		dev_err(dev, "Out of memory\n");
		return -ENOMEM;
	}

	for (i = PLL_ON_CFG; i < PLL_REG_CNT; i++)
		priv->cfg_map[i] = priv->cfg_map[0] + i * priv->pll_cnt;

	for (i = 0; i < ARRAY_SIZE(pll_reg_prop); i++) {
		ret = dev_read_u32(dev, pll_reg_prop[i],
		                           &priv->reg_offset[i]);
		if (ret < 0) {
			dev_err(dev, "Failed to get '%s' value on '%s' device\n",
			                  pll_reg_prop[i], dev_read_name(dev));
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(pll_prop); i++) {
		ret = dev_read_u32_array(dev, pll_prop[i],
		                    priv->cfg_map[i], priv->pll_cnt);
		if (ret < 0) {
			dev_err(dev, "Failed to get '%s' array value on '%s' device\n",
			                  pll_prop[i], dev_read_name(dev));
			return ret;
		}
	}

	return 0;
}
#endif

const struct clk_ops pll_clk_ops = {
#if defined(CONFIG_CLK_AXERA_V1)
	.get_rate = pll_clk_get_rate,
	.set_rate = pll_clk_set_rate,
	.of_xlate = pll_clk_of_xlate,
#elif defined(CONFIG_CLK_AXERA_V2)
	.get_rate = pll_clk_get_rate_v2,
	.set_rate = pll_clk_set_rate_v2,
	.of_xlate = pll_clk_of_xlate_v2,
#endif
};

U_BOOT_DRIVER(axera_clk_pll) = {
	.name      = PLL_DRV_NAME,
	.id        = UCLASS_CLK,
	.ops       = &pll_clk_ops,
	.flags     = DM_FLAG_PRE_RELOC,
#ifdef CONFIG_CLK_AXERA_V2
	.of_match  = pll_clk_of_ids,
	.probe     = pll_clk_probe,
	.priv_auto = sizeof(struct pll_clk_priv),
#endif
};

#ifdef CONFIG_CLK_AXERA_V1
struct clk *clk_register_pll(const char *name, const char *parent_name,
                       void *priv_data)
{
	struct lua_pll_clk *pll_clk;
	int ret;

	pll_clk = kzalloc(sizeof(*pll_clk), GFP_KERNEL);
	if (!pll_clk)
		return ERR_PTR(-ENOMEM);

	pll_clk->priv = priv_data;

	ret = clk_register(&pll_clk->clk, PLL_DRV_NAME,
	                             name, parent_name);
	if (ret) {
		pr_err("%s: failed to register: %d\n", __func__, ret);
		kfree(pll_clk);
		return ERR_PTR(ret);
	}

	return &pll_clk->clk;
}

static int pll_clock_bind(struct udevice *dev)
{
	uint32_t reg_offset[ARRAY_SIZE(pll_reg_prop)];
	struct pll_clock_plat *plat = NULL;
	struct ofnode_phandle_args args;
	ofnode subnode, regmap;
	uint32_t phandle, val;
	struct clk *clk;
	fdt_addr_t base;
	const char *name;
	int i, count, ret;

	plat = dev_get_plat(dev);

	count = dev_get_child_count(dev);
	plat->pll_priv = calloc(count, sizeof(struct pll_clock_priv));
	if (!plat->pll_priv)
		return -ENOMEM;
	plat->pll_count = count;

	struct pll_clock_priv *priv = plat->pll_priv;
	dev_for_each_subnode(subnode, dev) {
		name = ofnode_get_name(subnode);

		ret = ofnode_read_u32(subnode, "regmap",
		                   &phandle);
		if (ret) {
			dev_err(dev, "Failed to find 'regmap' property\n");
			return ret;
		}

		regmap = ofnode_get_by_phandle(phandle);
		if (!ofnode_valid(regmap)) {
			dev_dbg(dev, "unable to find 'regmap' phandle\n");
			return -EINVAL;
		}
		base = ofnode_get_addr(regmap);

		for (i = 0; i < ARRAY_SIZE(pll_reg_prop); i++)
			ofnode_read_u32(regmap, pll_reg_prop[i],
		                    &reg_offset[i]);

		for (i = 0; i < ARRAY_SIZE(pll_prop); i++) {
			ret = ofnode_read_u32(subnode, pll_prop[i], &val);
			if (ret) {
				dev_err(dev, "Failed to get '%s' value on '%s' node\n",
				                           pll_prop[i], name);
				return ret;
			}

			if (i < PLL_RDY)
				priv->addr[i] = (void *)base + val;
			else
				priv->addr[i] = (void *)base + reg_offset[i - PLL_RDY];

			switch (i) {
			case PLL_RDY:
				priv->rdy_bit = val;
				break;
			case PLL_FRC_EN:
				priv->frc_en_bit = val;
				break;
			case PLL_FRC_EN_SW:
				priv->frc_en_sw_bit = val;
				break;
			case PLL_REOPEN:
				priv->reopen_bit = val;
				break;
			}
		}

		ret = ofnode_parse_phandle_with_args(subnode, "clocks",
							"#clock-cells", 0,
							0, &args);

		clk = clk_register_pll(name, ofnode_get_name(args.node),
		                     priv);
		if (IS_ERR(clk)) {
			dev_warn(dev, "Failed to register '%s' clk\n", name);
			continue;
		}

		clk->id = dev_seq(clk->dev);
		dev_set_ofnode(clk->dev, subnode);

		priv++;
	}

	return 0;
}

static const struct udevice_id pll_clock_of_match[] = {
	{ .compatible = "axera,lua-pll-clocks" },
	{ /* sentinel */ },
};

U_BOOT_DRIVER(pll_clocks) = {
	.name = "pll_clock",
	.id = UCLASS_NOP,
	.of_match = pll_clock_of_match,
	.plat_auto	= sizeof(struct pll_clock_plat),
	.bind = pll_clock_bind,
};
#endif