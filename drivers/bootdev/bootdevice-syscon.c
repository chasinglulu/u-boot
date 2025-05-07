// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm/devres.h>
#include <dm.h>
#include <errno.h>
#include <regmap.h>
#include <syscon.h>

#include <bootdev/bootdevice.h>

struct boot_device_syscon {
	struct regmap *regmap;
	struct regmap *wp_regmap;
	uint32_t wp_offset;
	uint32_t wp_enable;
	uint32_t reg_offset;
	uint32_t shift;
	uint32_t mask;
};

static int boot_device_syscon_get(struct udevice *dev, u32 *buf)
{
	struct boot_device_syscon *plat = NULL;
	uint32_t val;

	if (unlikely(!buf))
		return -EINVAL;

	plat = dev_get_plat(dev);
	if (unlikely(!plat))
		return -EINVAL;

	regmap_read(plat->regmap, plat->reg_offset, &val);

	*buf = (val >> plat->shift) & plat->mask;

	return 0;
}

static int boot_device_syscon_set(struct udevice *dev, u32 bootdevice)
{
	struct boot_device_syscon *plat = NULL;

	plat = dev_get_plat(dev);
	if (unlikely(!plat))
		return -EINVAL;

	if (plat->wp_regmap)
		regmap_update_bits(plat->wp_regmap, plat->wp_offset,
				   plat->wp_enable,
				   plat->wp_enable);

	regmap_update_bits(plat->regmap, plat->reg_offset,
			   plat->mask << plat->shift,
			   bootdevice << plat->shift);

	return 0;
}

static const struct boot_device_ops boot_device_syscon_ops = {
	.get = boot_device_syscon_get,
	.set = boot_device_syscon_set,
};

static int boot_device_syscon_probe(struct udevice *dev)
{
	struct boot_device_syscon *plat = NULL;
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

	plat = dev_get_plat(dev);
	plat->regmap = regmap;
	plat->reg_offset = offset;
	plat->shift = shift;
	plat->mask = mask;

	plat->wp_regmap = syscon_regmap_lookup_by_phandle(dev, "wp-enable");
	if (IS_ERR(plat->wp_regmap)) {
		debug("Not found 'wp-enable' node\n");
		plat->wp_regmap = NULL;
	}

	if (plat->wp_regmap) {
		ret = dev_read_u32_index(dev, "wp-enable", 1, &plat->wp_offset);
		if (ret) {
			pr_err("Not found 'wp-enable' offset\n");
			return ret;
		}

		ret = dev_read_u32_index(dev, "wp-enable", 2, &plat->wp_enable);
		if (ret) {
			pr_err("Not found 'wp-enable' value\n");
			return ret;
		}
	}

	debug("bootdevice syscon probe successfully\n");
	return 0;
}

static const struct udevice_id boot_device_syscon_ids[] = {
	{ .compatible = "boot-device-syscon" },
	{ }
};

U_BOOT_DRIVER(boot_device_syscon) = {
	.name = "bootdevice-syscon",
	.id = UCLASS_BOOT_DEVICE,
	.probe = boot_device_syscon_probe,
	.of_match = boot_device_syscon_ids,
	.plat_auto = sizeof(struct boot_device_syscon),
	.ops = &boot_device_syscon_ops,
};