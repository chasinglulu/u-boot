
// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef FDL_USB_DRV_H_
#define FDL_USB_DRV_H_

#include <linux/types.h>
struct udevice;

struct fdl_usb_drv_ops {
	int (*usb_open)(void);
	int (*usb_read)(uint8_t* buf, uint32_t len, uint32_t timeout_ms);
	int (*usb_write)(uint8_t *buf, uint32_t len, uint32_t timeout_ms);
};

#define fdl_usb_drv_get_ops(dev)    ((struct fdl_usb_drv_ops *)(dev)->driver->ops)

int fdl_usb_drv_get(struct udevice **devp);
int fdl_usb_drv_get_maxpacket(const struct udevice *dev);
int fdl_usb_drv_open(const struct udevice *dev);
int fdl_usb_drv_read(const struct udevice *dev, char *buffer, uint32_t len);
int fdl_usb_drv_write(const struct udevice *dev, const char *buffer, uint32_t len);

#endif
