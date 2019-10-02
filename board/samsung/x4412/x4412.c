// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Wang Xinlu <wangkart@aliyun.com>
 */

#include <common.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/cpu.h>
#include <asm/arch/mmc.h>
#include <asm/arch/periph.h>
#include <asm/arch/pinmux.h>
#include <usb.h>

#ifndef CONFIG_SPL_BUILD
u32 get_board_rev(void)
{
	return 0;
}

int exynos_init(void)
{
	return 0;
}

int board_usb_init(int index, enum usb_init_type init)
{
	return 0;
}

void usb_hub_reset_devices(struct usb_hub_device *hub, int port)
{
	struct udevice *dev = hub->pusb_dev->dev;

	if (usb_hub_is_root_hub(dev) == false)
		return;

	if (port == 2) {
		struct gpio_desc desc;
		int ret;
		ret = dm_gpio_lookup_name("gpc00", &desc);
		if (ret < 0) {
			debug("Failed to find out gpc00\n");
			return;
		}
		ret = dm_gpio_request(&desc, "usb3503_reset");
		if (ret < 0 && ret != -EBUSY) {
			debug("%s: Failed to request gpc00\n", __func__);
			return;
		}
		ret = dm_gpio_set_dir_flags(&desc,
					    GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
		if (ret < 0) {
			debug("failed to set gpio direction\n");
			return;
		}
		udelay(100);
		dm_gpio_set_value(&desc, 0);
		udelay(5000);
	}
}

#ifdef CONFIG_BOARD_EARLY_INIT_F
int exynos_early_init_f(void)
{
	return 0;
}
#endif

#else /* !CONFIG_SPL_BUILD */
void exynos_led_set_on_early(unsigned int led_num)
{
#define GPX1CON     (0x11000C20)
#define GPX1DAT     (0x11000C24)
#define GPX2CON     (0x11000C40)
#define GPX2DAT     (0x11000C44)
#define DEBUG_LED1  (24)
#define DEBUG_LED2  (28)

    uint8_t val;

    val = led_num & 0x01;
    if (val) {
        clrsetbits_le32(GPX1CON, 0xf << DEBUG_LED1, 0x01 << DEBUG_LED1);
        clrbits_8(GPX1DAT, 1 << (DEBUG_LED1/4));
    }

    val = led_num & 0x02;
    if (val) {
        clrsetbits_le32(GPX1CON, 0xf << DEBUG_LED2, 0x01 << DEBUG_LED2);
        clrbits_8(GPX1DAT, 1 << (DEBUG_LED2/4));
    }

    val = led_num & 0x04;
    if (val) {
        clrsetbits_le32(GPX2CON, 0xf << DEBUG_LED1, 0x01 << DEBUG_LED1);
        clrbits_8(GPX2DAT, 1 << (DEBUG_LED1/4));
    }

    val = led_num & 0x08;
    if(val) {
        clrsetbits_le32(GPX2CON, 0xf << DEBUG_LED2, 0x1 << DEBUG_LED2);
        clrbits_8(GPX2DAT, 1 << (DEBUG_LED2/4));
    }
}
#endif
