// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <asm/gpio.h>
#include <dm.h>
#include <dm/devres.h>
#include <errno.h>
#include <bootdev/bootdevice.h>

#if !CONFIG_IS_ENABLED(OF_CONTROL)
struct boot_device_gpio_config {
	int gpio_dev_offset;
	int gpio_offset;
	int flags;
};
#endif

struct boot_device_gpio {
	struct gpio_desc *gpios_desc;
#if !CONFIG_IS_ENABLED(OF_CONTROL)
	struct boot_device_gpio_config *gpios_config;
#endif
	int gpio_count;
};

static int boot_device_gpio_get(struct udevice *dev, u32 *buf)
{
	struct boot_device_gpio *plat;
	int ret;

	if (!buf)
		return -EINVAL;

	plat = dev_get_plat(dev);
	if (!plat)
		return -EINVAL;

	ret = dm_gpio_get_values_as_int(plat->gpios_desc,
	                 plat->gpio_count);
	if (ret < 0)
		return ret;

	*buf = ret;

	return 0;
}

static int boot_device_gpio_probe(struct udevice *dev)
{
	struct boot_device_gpio *plat;
	int ret;

	plat = dev_get_plat(dev);
	if (!plat)
		return -EINVAL;

#if CONFIG_IS_ENABLED(OF_CONTROL)
	ret = gpio_get_list_count(dev, "gpios");
	if (ret < 0)
		return ret;

	plat->gpio_count = ret;
#endif

	if (plat->gpio_count <= 0)
		return -EINVAL;

	plat->gpios_desc = devm_kcalloc(dev, plat->gpio_count,
					    sizeof(struct gpio_desc), 0);
	if (!plat->gpios_desc)
		return -ENOMEM;

#if CONFIG_IS_ENABLED(OF_CONTROL)
	ret = gpio_request_list_by_name(dev, "gpios",
	            plat->gpios_desc,
	            plat->gpio_count, GPIOD_IS_IN);
	if (ret < 0)
		return ret;
#else
	for (int i = 0; i < plat->gpio_count; i++) {
		struct boot_device_gpio_config *gpio =
			plat->gpios_config + i;
		struct gpio_desc *desc = plat->gpio_desc + i;

		ret = uclass_get_device_by_seq(UCLASS_GPIO,
		                       gpio->gpio_dev_offset,
		                       &desc->dev);
		if (ret < 0)
			return ret;

		desc->flags = gpio->flags;
		desc->offset = gpio->gpio_offset;

		ret = dm_gpio_request(desc, "");
		if (ret < 0)
			return ret;

		ret = dm_gpio_set_dir(desc);
		if (ret < 0)
			return ret;
	}
#endif

	debug("bootdevice gpio probe successfully\n");
	return 0;
}

static int boot_device_gpio_remove(struct udevice *dev)
{
	struct boot_device_gpio *plat;

	plat = dev_get_plat(dev);
	if (!plat)
		return -EINVAL;

	return gpio_free_list(dev, plat->gpios_desc,
	               plat->gpio_count);
}

#if CONFIG_IS_ENABLED(OF_CONTROL)
static const struct udevice_id boot_device_gpio_ids[] = {
	{ .compatible = "boot-device-gpio", 0 },
	{ }
};
#endif

static const struct boot_device_ops boot_device_gpio_ops = {
	.get = boot_device_gpio_get,
};

U_BOOT_DRIVER(boot_device_gpio) = {
	.name = "bootdevice-gpio",
	.id = UCLASS_BOOT_DEVICE,
	.probe = boot_device_gpio_probe,
	.remove = boot_device_gpio_remove,
#if CONFIG_IS_ENABLED(OF_CONTROL)
	.of_match = boot_device_gpio_ids,
#endif
	.plat_auto = sizeof(struct boot_device_gpio),
	.ops = &boot_device_gpio_ops,
};
