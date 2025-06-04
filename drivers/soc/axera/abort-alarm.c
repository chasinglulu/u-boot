// SPDX-License-Identifier: GPL-2.0+
/*
 * Chip abort alarm driver for Axera LAGUNA SoCs
 *
 * Copyright (C) 2024 Xinlu Wang <wangxinlu@axera-tech.com>
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

struct abort_alarm {
	struct udevice *dev;
	struct regmap *regmap;
	uint32_t offset;
};

static int abort_alarm_read(struct udevice *dev, int offset, void *buf,
                             int size)
{
	struct abort_alarm *plat = dev_get_plat(dev);
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

	debug("%s: val = 0x%x, ret: %d\n", __func__, val, ffs(val));
	return ffs(val);
}

static int abort_alarm_of_to_plat(struct udevice *dev)
{
	struct abort_alarm *plat = dev_get_plat(dev);
	struct regmap *regmap;
	int ret;

	regmap = syscon_regmap_lookup_by_phandle(dev, "regmap");
	if (IS_ERR(regmap)) {
		debug("%s: Failed to find regmap: %ld\n", __func__, PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	ret = dev_read_u32(dev, "offset", &plat->offset);
	if (ret) {
		debug("%s: Failed to read 'offset' property: %d\n", __func__, ret);
		return ret;
	}

	plat->dev = dev;
	plat->regmap = regmap;

	debug("%s: %s done\n", __func__, dev->name);
	return 0;
}

static const struct misc_ops abort_alarm_ops = {
	.read = abort_alarm_read,
};

static const struct udevice_id abort_alarm_ids[] = {
	{ .compatible = "axera,laguna-abort-alarm" },
	{}
};

U_BOOT_DRIVER(abort_alarm) = {
	.name = "abort_alarm",
	.of_match = abort_alarm_ids,
	.id = UCLASS_MISC,
	.of_to_plat = abort_alarm_of_to_plat,
	.plat_auto = sizeof(struct abort_alarm),
	.ops = &abort_alarm_ops,
};
