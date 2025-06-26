// SPDX-License-Identifier: GPL-2.0+
/*
 * syscon based AB control driver
 *
 * Copyright (C) 2025 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/of_access.h>
#include <errno.h>
#include <regmap.h>
#include <linux/err.h>
#include <syscon.h>
#include <misc.h>
#include <dm/devres.h>

struct abc_syscon_platdata {
	struct udevice *dev;
	struct regmap *regmap;
	uint32_t offset;
};

static int abc_syscon_read(struct udevice *dev, int offset, void *buf,
                             int size)
{
	struct abc_syscon_platdata *plat = dev_get_plat(dev);
	uint32_t val;
	int ret;

	if (!plat->regmap)
		return -ENODEV;

	ret = regmap_read(plat->regmap, plat->offset, &val);
	if (ret)
		return ret;

	if (buf && size >= sizeof(val)) {
		memcpy(buf, &val, sizeof(val));
		return 0;
	}

	debug("%s: val = 0x%x\n", __func__, val);
	return 0;
}

static int
abc_syscon_write(struct udevice *dev, int offset, const void *buf,
                            int size)
{
	struct abc_syscon_platdata *plat = dev_get_plat(dev);
	uint32_t val;
	int ret;

	if (!plat->regmap)
		return -ENODEV;

	if (!buf || size != sizeof(val)) {
		debug("%s: Invalid buffer or size\n", __func__);
		return -EINVAL;
	}

	memcpy(&val, buf, sizeof(val));
	ret = regmap_write(plat->regmap, plat->offset, val);
	if (ret) {
		debug("Unable to write regmap (ret = %d)\n",ret);
		return ret;
	}

	return 0;
}

static int abc_syscon_of_to_plat(struct udevice *dev)
{
	struct abc_syscon_platdata *plat = dev_get_plat(dev);
	struct regmap *regmap;
	int ret;

	regmap = syscon_regmap_lookup_by_phandle(dev, "regmap");
	if (IS_ERR(regmap)) {
		debug("%s: Failed to find regmap: %ld\n",
		                    __func__, PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	ret = dev_read_u32(dev, "offset", &plat->offset);
	if (ret) {
		debug("%s: Failed to read 'offset' property: %d\n",
		                    __func__, ret);
		return ret;
	}

	plat->dev = dev;
	plat->regmap = regmap;

	debug("%s: %s done\n", __func__, dev->name);
	return 0;
}

static const struct misc_ops abc_syscon_ops = {
	.read = abc_syscon_read,
	.write = abc_syscon_write,
};

static const struct udevice_id abc_syscon_ids[] = {
	{ .compatible = "axera,laguna-abc-syscon" },
	{}
};

U_BOOT_DRIVER(abc_syscon) = {
	.name = "abc_syscon",
	.of_match = abc_syscon_ids,
	.id = UCLASS_MISC,
	.of_to_plat = abc_syscon_of_to_plat,
	.plat_auto = sizeof(struct abc_syscon_platdata),
	.ops = &abc_syscon_ops,
};
