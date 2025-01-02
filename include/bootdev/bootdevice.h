// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef BOOT_DEVICE_H_
#define BOOT_DEVICE_H_

#include <asm/types.h>
#include <dm/device.h>

struct boot_device {
	const char *dev_name;
	uint32_t *dev_ids;
	uint8_t count;
};

struct boot_device_uc_plat {
	const char *env_variable;
	struct boot_device *devs;
	uint8_t count;
};

struct boot_device_ops {
	/**
	 * get() - get the current boot device value
	 *
	 * Returns the current value from the boot device backing store.
	 *
	 * @dev:	Device to read from
	 * @bootdevice:	Address to save the current boot device value
	 */
	int (*get)(struct udevice *dev, u32 *bootdevice);

	/**
	 * set() - set a boot device value
	 *
	 * Sets the value in the boot device backing store.
	 *
	 * @dev:	Device to read from
	 * @bootdevice:	New boot device value to store
	 */
	int (*set)(struct udevice *dev, u32 bootdevice);
};

/* Access the operations for a boot device device */
#define boot_device_get_ops(dev) ((struct boot_device_ops *)(dev)->driver->ops)

/**
 * boot_device_update() - Update the boot device env variable.
 *
 * @dev:	Device to read from
 * Return: 0 if OK, -ve on error
 */
int boot_device_update(struct udevice *dev);

/**
 * dm_boot_device_get() - Get the boot device ID.
 *
 * @dev:	Device to read from
 * @name:	boot device name to ouptut
 * Return: dev_id if OK, -ve on error
 */
int boot_device_get(struct udevice *dev, const char **name);

#endif /* BOOT_DEVICE_H_ */
