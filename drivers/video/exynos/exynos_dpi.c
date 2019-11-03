// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Wang Xinlu <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <display.h>
#include <fdtdec.h>

int exynos_dpi_enable(struct udevice *dev, int panel_bpp,
                const struct display_timing *timing)
{
        return 0;
}

static const struct dm_display_ops exynos_dpi_ops = {
        .enable = exynos_dpi_enable,
};

static const struct udevice_id exynos_dpi_ids[] = {
        { .compatible = "samsung,exynos4412-dpi" },
        { }
};

U_BOOT_DRIVER(exynos_dpi) = {
        .name   = "exynos_dpi",
        .id     = UCLASS_DISPLAY,
        .of_match = exynos_dpi_ids,
        .ops = &exynos_dpi_ops,
};
