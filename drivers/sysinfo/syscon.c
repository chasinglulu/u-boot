
// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <sysinfo.h>
#include <syscon.h>
#include <regmap.h>
#include <dm/device_compat.h>

struct sysinfo_syscon_priv {
	struct regmap *regmap;
	uint32_t reg_offset;
	uint32_t shift;
	uint32_t mask;
	uint32_t value;
};

enum {
	DOWNLOAD_IF,
};

static int sysinfo_syscon_detect(struct udevice *dev)
{
	struct sysinfo_syscon_priv *priv;
	int ret;

	if (unlikely(!dev))
		return -EINVAL;
	priv = dev_get_priv(dev);

	if (unlikely(!priv))
		return -EINVAL;

	ret = regmap_read(priv->regmap, priv->reg_offset, &priv->value);
	if (ret)
		return ret;

	return 0;
}

static int sysinfo_syscon_get_int(struct udevice *dev, int id, int *val)
{
	struct sysinfo_syscon_priv *priv;

	if (unlikely(!dev))
		return -EINVAL;
	priv = dev_get_priv(dev);

	if (unlikely(!priv))
		return -EINVAL;

	switch (id) {
	case DOWNLOAD_IF:
		*val = (priv->value >> priv->shift) & priv->mask;
		break;
	default:
		debug("%s: Integer value %d unknown\n", dev->name, id);
		return -EINVAL;
	}

	return 0;
}

static const struct sysinfo_ops sysinfo_syscon_ops = {
	.detect = sysinfo_syscon_detect,
	.get_int = sysinfo_syscon_get_int,
};

static int sysinfo_syscon_probe(struct udevice *dev)
{
	struct sysinfo_syscon_priv *priv = NULL;
	struct regmap *regmap = NULL;
	uint32_t offset, shift, mask;
	int ret;

	regmap = syscon_regmap_lookup_by_phandle(dev, "regmap");
	if (IS_ERR(regmap))
		return -ENODEV;

	ret = dev_read_u32_index(dev, "regmap", 1, &offset);
	if (ret)
		return ret;

	ret = dev_read_u32(dev, "syscon-shift", &shift);
	if (ret)
		return ret;

	ret = dev_read_u32(dev, "syscon-mask", &mask);
	if (ret)
		return ret;

	priv = dev_get_priv(dev);
	priv->regmap = regmap;
	priv->reg_offset = offset;
	priv->shift = shift;
	priv->mask = mask;

	debug("sysinfo syscon probe successfully\n");
	return 0;
}

static const struct udevice_id sysinfo_syscon_ids[] = {
	{ .compatible = "syscon-sysinfo" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(sysinfo_syscon) = {
	.name           = "sysinfo_syscon",
	.id             = UCLASS_SYSINFO,
	.of_match       = sysinfo_syscon_ids,
	.ops            = &sysinfo_syscon_ops,
	.priv_auto      = sizeof(struct sysinfo_syscon_priv),
	.probe          = sysinfo_syscon_probe,
};