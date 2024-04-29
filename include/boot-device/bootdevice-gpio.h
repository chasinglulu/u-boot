/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef BOOT_DEVICE_GPIO_H_
#define BOOT_DEVICE_GPIO_H_

#include <asm/gpio.h>

/*
 * In case of initializing the driver statically (using U_BOOT_DEVICE macro),
 * we can use this struct to declare the pins used.
 */

#if !CONFIG_IS_ENABLED(OF_CONTROL)
struct boot_device_gpio_config {
	int gpio_dev_offset;
	int gpio_offset;
	int flags;
};
#endif

struct boot_device_gpio_platdata {
	struct gpio_desc *gpio_desc;
#if !CONFIG_IS_ENABLED(OF_CONTROL)
	struct boot_device_gpio_config *gpios_config;
#endif
	int gpio_count;
};

#endif /* BOOT_DEVICE_GPIO_H_ */
