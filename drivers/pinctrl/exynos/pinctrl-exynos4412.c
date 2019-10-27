// SPDX-License-Identifier: GPL-2.0+
/*
 * Exynos4412 pinctl driver.
 * Copyright (C) 2019 Wang Xinlu <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <dm/lists.h>
#include <dm/root.h>
#include <fdtdec.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/gpio.h>
#include <dm/device-internal.h>
#include "pinctrl-exynos.h"

DECLARE_GLOBAL_DATA_PTR;

/* platform data for each GPIO bank */
struct exynos_gpio_platdata {
        struct s5p_gpio_bank *bank;
        const char *bank_name;
};

static const struct samsung_pin_bank_data exynos4412_pin_bank0[] = {
        EXYNOS_PIN_BANK(8, 0x000, "gpa0"),
        EXYNOS_PIN_BANK(6, 0x020, "gpa1"),
        EXYNOS_PIN_BANK(8, 0x040, "gpb"),
        EXYNOS_PIN_BANK(5, 0x060, "gpc0"),
        EXYNOS_PIN_BANK(5, 0x080, "gpc1"),
        EXYNOS_PIN_BANK(4, 0x0a0, "gpd0"),
        EXYNOS_PIN_BANK(4, 0x0c0, "gpd1"),
        EXYNOS_PIN_BANK(8, 0x180, "gpf0"),
        EXYNOS_PIN_BANK(8, 0x1a0, "gpf1"),
        EXYNOS_PIN_BANK(8, 0x1c0, "gpf2"),
        EXYNOS_PIN_BANK(6, 0x1e0, "gpf3"),
        EXYNOS_PIN_BANK(8, 0x240, "gpj0"),
        EXYNOS_PIN_BANK(5, 0x260, "gpj1"),
};

static const struct samsung_pin_bank_data exynos4412_pin_bank1[] = {
        EXYNOS_PIN_BANK(7, 0x040, "gpk0"),
        EXYNOS_PIN_BANK(7, 0x060, "gpk1"),
        EXYNOS_PIN_BANK(7, 0x080, "gpk2"),
        EXYNOS_PIN_BANK(7, 0x0a0, "gpk3"),
        EXYNOS_PIN_BANK(7, 0x0c0, "gpl0"),
        EXYNOS_PIN_BANK(2, 0x0e0, "gpl1"),
        EXYNOS_PIN_BANK(8, 0x100, "gpl2"),
        EXYNOS_PIN_BANK(6, 0x120, "gpy0"),
        EXYNOS_PIN_BANK(4, 0x140, "gpy1"),
        EXYNOS_PIN_BANK(6, 0x160, "gpy2"),
        EXYNOS_PIN_BANK(8, 0x180, "gpy3"),
        EXYNOS_PIN_BANK(8, 0x1a0, "gpy4"),
        EXYNOS_PIN_BANK(8, 0x1c0, "gpy5"),
        EXYNOS_PIN_BANK(8, 0x1e0, "gpy6"),
        EXYNOS_PIN_BANK(8, 0x260, "gpm0"),
        EXYNOS_PIN_BANK(7, 0x280, "gpm1"),
        EXYNOS_PIN_BANK(5, 0x2a0, "gpm2"),
        EXYNOS_PIN_BANK(8, 0x2c0, "gpm3"),
        EXYNOS_PIN_BANK(8, 0x2e0, "gpm4"),
        EXYNOS_PIN_BANK(8, 0xc00, "gpx0"),
        EXYNOS_PIN_BANK(8, 0xc20, "gpx1"),
        EXYNOS_PIN_BANK(8, 0xc40, "gpx2"),
        EXYNOS_PIN_BANK(8, 0xc60, "gpx3"),
};

static const struct samsung_pin_bank_data exynos4412_pin_bank2[] = {
        EXYNOS_PIN_BANK(7, 0x000, "gpz"),
};

static const struct samsung_pin_bank_data exynos4412_pin_bank3[] = {
        EXYNOS_PIN_BANK(8, 0x000, "gpv0"),
        EXYNOS_PIN_BANK(8, 0x020, "gpv1"),
        EXYNOS_PIN_BANK(8, 0x060, "gpv2"),
        EXYNOS_PIN_BANK(8, 0x080, "gpv3"),
        EXYNOS_PIN_BANK(2, 0x0c0, "gpv4"),
};

static struct pinctrl_ops exynos4412_pinctrl_ops = {
        .set_state = exynos_pinctrl_set_state,
};

const struct samsung_pin_ctrl exynos4412_pin_ctrl[] = {
        {
                .pin_banks = exynos4412_pin_bank0,
                .nr_banks = ARRAY_SIZE(exynos4412_pin_bank0),
        }, {
                .pin_banks = exynos4412_pin_bank1,
                .nr_banks = ARRAY_SIZE(exynos4412_pin_bank1),
        }, {
                .pin_banks = exynos4412_pin_bank2,
                .nr_banks = ARRAY_SIZE(exynos4412_pin_bank2),
        }, {
                .pin_banks = exynos4412_pin_bank3,
                .nr_banks = ARRAY_SIZE(exynos4412_pin_bank3),
        },
};

static const struct udevice_id exynos4412_pinctrl_ids[] = {
        { .compatible = "samsung,exynos4x12-pinctrl",
                .data = (ulong)exynos4412_pin_ctrl },
        { }
};

static unsigned long get_bank_offset(struct udevice *dev, const char *bank_name)
{
        const struct samsung_pin_ctrl *pin_ctrl = NULL;
        const struct samsung_pin_bank_data *bank_data = NULL;
        u32 nr_banks, idx = 0;

        if (dev->req_seq < 0 ||
                (dev->req_seq >= ARRAY_SIZE(exynos4412_pin_ctrl)))
                return -ENOENT;

        pin_ctrl = &exynos4412_pin_ctrl[dev->req_seq];
        bank_data = pin_ctrl->pin_banks;
        nr_banks = pin_ctrl->nr_banks;

        for (idx = 0; idx < nr_banks; idx++)
                if (!strcmp(bank_name, bank_data[idx].name))
                        break;

        return bank_data[idx].offset;
}

static int pinctrl_exynos4412_bind(struct udevice *parent)
{
        struct s5p_gpio_bank *base;
        const void *blob = gd->fdt_blob;
        struct driver *gpio_drv = NULL;
        int node;

        gpio_drv = lists_driver_lookup_name("gpio_exynos");
        if (!gpio_drv)
                return -ENOENT;

        base = (struct s5p_gpio_bank *)devfdt_get_addr(parent);
        for (node = fdt_first_subnode(blob, dev_of_offset(parent));
                node > 0;
                node = fdt_next_subnode(blob, node)) {
                struct exynos_gpio_platdata *plat;
                struct udevice *child;
                unsigned long offset;
                int ret;

                if (!fdtdec_get_bool(blob, node, "gpio-controller"))
                        continue;

                plat = calloc(1, sizeof(*plat));
                if (!plat)
                        return -ENOMEM;

                plat->bank_name = fdt_get_name(blob, node, NULL);
                ret = device_bind(parent, gpio_drv,
                                plat->bank_name, plat, -1, &child);
                if (ret)
                        return ret;

                dev_set_of_offset(child, node);

                debug("requested sequence: %d\n",parent->req_seq);
                offset = get_bank_offset(parent, plat->bank_name);
                plat->bank = (struct s5p_gpio_bank *)((ulong)base + offset);

                debug("bank at %p\n", plat->bank);
                debug("dev at %p: %s\n", plat->bank, plat->bank_name);
        }

        return 0;
}

U_BOOT_DRIVER(pinctrl_exynos4412) = {
        .name   = "pinctrl_exynos4412",
        .id     = UCLASS_PINCTRL,
        .of_match       = exynos4412_pinctrl_ids,
        .priv_auto_alloc_size   = sizeof(struct exynos_pinctrl_priv),
        .ops    = &exynos4412_pinctrl_ops,
        .bind   = pinctrl_exynos4412_bind,
        .probe  = exynos_pinctrl_probe,
};
