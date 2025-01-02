// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <exports.h>
#include <bootdev/bootdevice.h>

int boot_device_get(struct udevice *dev, const char **name)
{
	const struct boot_device_uc_plat *plat = NULL;
	struct boot_device_ops *ops = NULL;
	struct boot_device *bootdevice = NULL;
	uint32_t dev_id;
	int ret, i, j;

	if (unlikely(!dev))
		return -EINVAL;

	ops = boot_device_get_ops(dev);
	if (!ops || !ops->get)
		return -ENOSYS;

	ret = ops->get(dev, &dev_id);
	if (ret < 0) {
		dev_err(dev, "Failed to retrieve the boot device ID\n");
		return ret;
	}

	plat = dev_get_uclass_plat(dev);
	if (unlikely(!plat))
		return -EINVAL;

	for (i = 0; i < plat->count; i++) {
		bootdevice = plat->devs + i;
		for (j = 0; j < bootdevice->count; j++) {
			if (bootdevice->dev_ids[j] == dev_id) {
				if (name)
					*name = bootdevice->dev_name;

				return dev_id;
			}
		}
	}

	return -ENODEV;
}

int boot_device_update(struct udevice *dev)
{
	const struct boot_device_uc_plat *plat = NULL;
	struct boot_device *bootdevice = NULL;
	struct boot_device_ops *ops = NULL;
	u32 dev_id;
	int ret, i, j;

	if (unlikely(!dev))
		return -EINVAL;

	ops = boot_device_get_ops(dev);
	if (!ops || !ops->get)
		return -ENOSYS;

	ret = ops->get(dev, &dev_id);
	if (ret < 0) {
		dev_err(dev, "Failed to retrieve the boot device value\n");
		return ret;
	}

	plat = dev_get_uclass_plat(dev);
	for (i = 0; i < plat->count; i++) {
		bootdevice = plat->devs + i;
		for (j = 0; j < bootdevice->count; j++) {
			if (bootdevice->dev_ids[j] == dev_id) {
				ret = env_set(plat->env_variable,
						bootdevice->dev_name);
				if (ret) {
					dev_err(dev, "Failed to set env: %s\n",
								plat->env_variable);
					return ret;
				}
			}
		}
	}

	if (ops->set) {
		/* Clear the value */
		bootdevice = 0;
		ret = ops->set(dev, dev_id);
		if (ret) {
			dev_err(dev, "Failed to clear the boot device\n");
			return ret;
		}
	}

	return 0;
}

static int boot_device_pre_probe(struct udevice *dev)
{
	struct boot_device_uc_plat *plat = NULL;

	plat = dev_get_uclass_plat(dev);
	if (!plat)
		return -EINVAL;

#if CONFIG_IS_ENABLED(OF_CONTROL)
	struct boot_device *next = NULL;
	const char *device_prefix = "dev-";
	const int device_prefix_len = strlen(device_prefix);
	struct ofprop property;
	const u32 *propvalue;
	const char *propname;
	int count, lenp;

	plat->env_variable = dev_read_string(dev, "u-boot,env-variable");
	if (!plat->env_variable)
		plat->env_variable = "boot-device";

	plat->count = 0;

	dev_for_each_property(property, dev) {
		propvalue = dev_read_prop_by_prop(&property, &propname, NULL);
		if (!propvalue) {
			dev_err(dev, "Could not get the value for property %s\n",
				propname);
			return -EINVAL;
		}

		if (!strncmp(propname, device_prefix, device_prefix_len))
			plat->count++;
	}

	plat->devs = devm_kcalloc(dev, plat->count, sizeof(struct boot_device), 0);
	if (!plat->devs)
		return -ENOMEM;

	next = plat->devs;
	dev_for_each_property(property, dev) {
		propvalue = dev_read_prop_by_prop(&property, &propname, &lenp);
		if (!propvalue) {
			dev_err(dev, "Could not get the value for property %s\n",
				propname);
			return -EINVAL;
		}

		if (!strncmp(propname, device_prefix, device_prefix_len)) {
			count = lenp / sizeof(uint32_t);
			next->dev_name = &propname[device_prefix_len];
			next->count = count;
			next->dev_ids = devm_kcalloc(dev, count, sizeof(uint32_t), 0);

			for (int i = 0; i < count; i++)
				next->dev_ids[i] = fdt32_to_cpu(*propvalue++);

			next++;
		}
	}
#else
	if (!plat_data->env_variable)
		plat_data->env_variable = "boot-device";

#endif

	return 0;
}

UCLASS_DRIVER(boot_device) = {
	.name = "bootdevice",
	.id = UCLASS_BOOT_DEVICE,
	.pre_probe = boot_device_pre_probe,
	.per_device_plat_auto = sizeof(struct boot_device_uc_plat),
};
