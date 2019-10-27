// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2009 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <dm/device-internal.h>

DECLARE_GLOBAL_DATA_PTR;

#define S5P_GPIO_GET_PIN(x)	(x % GPIO_PER_BANK)

#define CON_MASK(val)			(0xf << ((val) << 2))
#define CON_SFR(gpio, cfg)		((cfg) << ((gpio) << 2))
#define CON_SFR_UNSHIFT(val, gpio)	((val) >> ((gpio) << 2))

#define DAT_MASK(gpio)			(0x1 << (gpio))
#define DAT_SET(gpio)			(0x1 << (gpio))

#define PULL_MASK(gpio)		(0x3 << ((gpio) << 1))
#define PULL_MODE(gpio, pull)		((pull) << ((gpio) << 1))

#define DRV_MASK(gpio)			(0x3 << ((gpio) << 1))
#define DRV_SET(gpio, mode)		((mode) << ((gpio) << 1))
#define RATE_MASK(gpio)		(0x1 << (gpio + 16))
#define RATE_SET(gpio)			(0x1 << (gpio + 16))

/* Platform data for each bank */
struct exynos_gpio_platdata {
	struct s5p_gpio_bank *bank;
	const char *bank_name;	/* Name of port, e.g. 'gpa0" */
};

/* Information about each bank at run-time */
struct exynos_bank_info {
	struct s5p_gpio_bank *bank;
};

static struct s5p_gpio_bank *s5p_gpio_get_bank(unsigned int gpio)
{
	const struct gpio_info *data;
	unsigned int upto;
	int i, count;

	data = get_gpio_data();
	count = get_bank_num();
	upto = 0;

	for (i = 0; i < count; i++) {
		debug("i=%d, upto=%d\n", i, upto);
		if (gpio < data->max_gpio) {
			struct s5p_gpio_bank *bank;
			bank = (struct s5p_gpio_bank *)data->reg_addr;
			bank += (gpio - upto) / GPIO_PER_BANK;
			debug("gpio=%d, bank=%p\n", gpio, bank);
			return bank;
		}

		upto = data->max_gpio;
		data++;
	}

	return NULL;
}

static void s5p_gpio_cfg_pin(struct s5p_gpio_bank *bank, int gpio, int cfg)
{
	unsigned int value;

	value = readl(&bank->con);
	value &= ~CON_MASK(gpio);
	value |= CON_SFR(gpio, cfg);
	writel(value, &bank->con);
}

#ifdef CONFIG_SPL_BUILD
static void s5p_gpio_set_value(struct s5p_gpio_bank *bank, int gpio, int en)
{
	unsigned int value;

	value = readl(&bank->dat);
	value &= ~DAT_MASK(gpio);
	if (en)
		value |= DAT_SET(gpio);
	writel(value, &bank->dat);
}

/* Common GPIO API - SPL does not support driver model yet */
int gpio_set_value(unsigned gpio, int value)
{
	s5p_gpio_set_value(s5p_gpio_get_bank(gpio),
			   s5p_gpio_get_pin(gpio), value);

	return 0;
}
#endif /* CONFIG_SPL_BUILD */

static void s5p_gpio_set_pull(struct s5p_gpio_bank *bank, int gpio, int mode)
{
	unsigned int value;

	value = readl(&bank->pull);
	value &= ~PULL_MASK(gpio);

	switch (mode) {
	case S5P_GPIO_PULL_DOWN:
	case S5P_GPIO_PULL_UP:
		value |= PULL_MODE(gpio, mode);
		break;
	default:
		break;
	}

	writel(value, &bank->pull);
}

static void s5p_gpio_set_drv(struct s5p_gpio_bank *bank, int gpio, int mode)
{
	unsigned int value;

	value = readl(&bank->drv);
	value &= ~DRV_MASK(gpio);

	switch (mode) {
	case S5P_GPIO_DRV_1X:
	case S5P_GPIO_DRV_2X:
	case S5P_GPIO_DRV_3X:
	case S5P_GPIO_DRV_4X:
		value |= DRV_SET(gpio, mode);
		break;
	default:
		return;
	}

	writel(value, &bank->drv);
}

static void s5p_gpio_set_rate(struct s5p_gpio_bank *bank, int gpio, int mode)
{
	unsigned int value;

	value = readl(&bank->drv);
	value &= ~RATE_MASK(gpio);

	switch (mode) {
	case S5P_GPIO_DRV_FAST:
	case S5P_GPIO_DRV_SLOW:
		value |= RATE_SET(gpio);
		break;
	default:
		return;
	}

	writel(value, &bank->drv);
}

int s5p_gpio_get_pin(unsigned gpio)
{
	return S5P_GPIO_GET_PIN(gpio);
}

/*
 * There is no common GPIO API for pull, drv, pin, rate (yet). These
 * functions are kept here to preserve function ordering for review.
 */
void gpio_set_pull(int gpio, int mode)
{
	s5p_gpio_set_pull(s5p_gpio_get_bank(gpio),
			  s5p_gpio_get_pin(gpio), mode);
}

void gpio_set_drv(int gpio, int mode)
{
	s5p_gpio_set_drv(s5p_gpio_get_bank(gpio),
			 s5p_gpio_get_pin(gpio), mode);
}

void gpio_cfg_pin(int gpio, int cfg)
{
	s5p_gpio_cfg_pin(s5p_gpio_get_bank(gpio),
			 s5p_gpio_get_pin(gpio), cfg);
}

void gpio_set_rate(int gpio, int mode)
{
	s5p_gpio_set_rate(s5p_gpio_get_bank(gpio),
			  s5p_gpio_get_pin(gpio), mode);
}
