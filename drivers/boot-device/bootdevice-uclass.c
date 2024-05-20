// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <exports.h>
#include <boot-device/bootdevice.h>

int dm_boot_device_get(struct udevice *dev, const char **name)
{
	struct boot_device_ops *ops = boot_device_get_ops(dev);
	u32 bootdevice;
	int ret, i;

	assert(ops);

	if (!ops->get)
		return -ENOSYS;

	ret = ops->get(dev, &bootdevice);
	if (ret < 0) {
		dev_err(dev, "Failed to retrieve the boot device value\n");
		return ret;
	}

	const struct boot_device_uclass_platdata *plat_data =
		dev_get_uclass_plat(dev);

	for (i = 0; i < plat_data->count; i++) {
		if (plat_data->devs[i].dev_id == bootdevice) {
			if (name)
				*name = plat_data->devs[i].dev_name;

			return bootdevice;
		}
	}

	return -ENODEV;
}

int dm_boot_device_update(struct udevice *dev)
{
	struct boot_device_ops *ops = boot_device_get_ops(dev);
	u32 bootdevice;
	int ret, i;

	assert(ops);

	if (!ops->get)
		return -ENOSYS;

	ret = ops->get(dev, &bootdevice);
	if (ret < 0) {
		dev_err(dev, "Failed to retrieve the boot device value\n");
		return ret;
	}

	const struct boot_device_uclass_platdata *plat_data =
		dev_get_uclass_plat(dev);

	for (i = 0; i < plat_data->count; i++) {
		if (plat_data->devs[i].dev_id == bootdevice) {
			ret = env_set(plat_data->env_variable,
				      plat_data->devs[i].dev_name);
			if (ret) {
				dev_err(dev, "Failed to set env: %s\n",
					plat_data->env_variable);
				return ret;
			}
		}
	}

	if (ops->set) {
		/* Clear the value */
		bootdevice = 0;
		ret = ops->set(dev, bootdevice);
		if (ret) {
			dev_err(dev, "Failed to clear the boot device\n");
			return ret;
		}
	}

	return 0;
}

int dm_boot_device_pre_probe(struct udevice *dev)
{
	struct boot_device_uclass_platdata *plat_data;

	plat_data = dev_get_uclass_plat(dev);
	if (!plat_data)
		return -EINVAL;

#if CONFIG_IS_ENABLED(OF_CONTROL)
	const char *device_prefix = "dev-";
	const int device_prefix_len = strlen(device_prefix);
	struct ofprop property;
	const u32 *propvalue;
	const char *propname;

	plat_data->env_variable = dev_read_string(dev, "u-boot,env-variable");
	if (!plat_data->env_variable)
		plat_data->env_variable = "boot-device";

	plat_data->count = 0;

	dev_for_each_property(property, dev) {
		propvalue = dev_read_prop_by_prop(&property, &propname, NULL);
		if (!propvalue) {
			dev_err(dev, "Could not get the value for property %s\n",
				propname);
			return -EINVAL;
		}

		if (!strncmp(propname, device_prefix, device_prefix_len))
			plat_data->count++;
	}

	plat_data->devs = devm_kcalloc(dev, plat_data->count,
					sizeof(struct boot_device), 0);

	struct boot_device *next = plat_data->devs;

	dev_for_each_property(property, dev) {
		propvalue = dev_read_prop_by_prop(&property, &propname, NULL);
		if (!propvalue) {
			dev_err(dev, "Could not get the value for property %s\n",
				propname);
			return -EINVAL;
		}

		if (!strncmp(propname, device_prefix, device_prefix_len)) {
			next->dev_name = &propname[device_prefix_len];
			next->dev_id = fdt32_to_cpu(*propvalue);

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
	.name	= "boot-device",
	.id	= UCLASS_BOOT_DEVICE,
	.pre_probe	= dm_boot_device_pre_probe,
	.per_device_plat_auto =
		sizeof(struct boot_device_uclass_platdata),
};
