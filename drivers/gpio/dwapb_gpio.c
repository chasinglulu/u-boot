// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2015 Marek Vasut <marex@denx.de>
 *
 * DesignWare APB GPIO driver
 */

/**
 * @file dwapb_gpio.c
 * @NO{S02E02C08}
 * @ASIL{B}
 *
 */

#include <common.h>
#include <log.h>
#include <malloc.h>
#include <asm/arch/gpio.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <dm/lists.h>
#include <dm/root.h>
#include <errno.h>
#include <reset.h>
#include <linux/bitops.h>

#define GPIO_SWPORT_DR(p)	(0x00 + (p) * 0xc)	/**< gpio portx data register */
#define GPIO_SWPORT_DDR(p)	(0x04 + (p) * 0xc)	/**< gpio portx direction register */
#define GPIO_INTEN		0x30	/**< interrupt enable register */
#define GPIO_INTMASK		0x34	/**< interrupt mask register */
#define GPIO_INTTYPE_LEVEL	0x38	/**< interrupt level register */
#define GPIO_INT_POLARITY	0x3c	/**< interrupt polarity register */
#define GPIO_INTSTATUS		0x40	/**< interrupt status register */
#define GPIO_PORTA_DEBOUNCE	0x48	/**< debounce enable register */
#define GPIO_PORTA_EOI		0x4c	/**< port A clear interrupt register */
#define GPIO_EXT_PORT(p)	(0x50 + (p) * 4)	/**< external gpio port register */

/**
 * @NO{S02E02C08}
 * @struct gpio_dwapb_priv
 * @brief Private data for gpio device
 */
struct gpio_dwapb_priv {
	struct reset_ctl_bulk	resets;	/**< reset signal from dts */
};

/**
 * @NO{S02E02C08}
 * @struct gpio_dwapb_plat
 * @brief information about gpio device
 */
struct gpio_dwapb_plat {
	const char	*name;	/**< bank name */
	int		bank;	/**< bank number */
	int		pins;	/**< gpio pin number per bank */
	void __iomem	*base; /**< gpio controller base address */
};

/**
 * @NO{S02E02C08}
 * @ASIL{B}
 * @brief Setup gpio pin input direction
 *
 * @param[in] dev: gpio device pointer
 * @param[in] pin: gpio pin
 *
 * @retval =0: success
 *
 * @data_read None
 * @data_read None
 * @data_updated None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @design
 */
static int dwapb_gpio_direction_input(struct udevice *dev, unsigned pin)
{
	struct gpio_dwapb_plat *plat = dev_get_plat(dev);

	clrbits_le32(plat->base + GPIO_SWPORT_DDR(plat->bank), 1 << pin);
	return 0;
}

/**
 * @NO{S02E02C08}
 * @ASIL{B}
 * @brief Setup gpio pin output direction and output valid value
 *
 * @param[in] dev: gpio device pointer
 * @param[in] pin: gpio pin
 * @param[in] val: value to output
 *
 * @retval =0: success
 * @retval <0: failure
 *
 * @data_read None
 * @data_read None
 * @data_updated None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @design
 */
static int dwapb_gpio_direction_output(struct udevice *dev, unsigned pin,
				     int val)
{
	struct gpio_dwapb_plat *plat = dev_get_plat(dev);

	setbits_le32(plat->base + GPIO_SWPORT_DDR(plat->bank), 1 << pin);

	if (val)
		setbits_le32(plat->base + GPIO_SWPORT_DR(plat->bank), 1 << pin);
	else
		clrbits_le32(plat->base + GPIO_SWPORT_DR(plat->bank), 1 << pin);

	return 0;
}

/**
 * @NO{S02E02C08}
 * @ASIL{B}
 * @brief Output value into gpio pin
 *
 * @param[in] dev: gpio device pointer
 * @param[in] pin: gpio pin
 * @param[in] val: value to set
 *
 * @retval =0: success
 * @retval <0: failure
 *
 * @data_read None
 * @data_read None
 * @data_updated None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @design
 */
static int dwapb_gpio_set_value(struct udevice *dev, unsigned pin, int val)
{
	struct gpio_dwapb_plat *plat = dev_get_plat(dev);

	if (val)
		setbits_le32(plat->base + GPIO_SWPORT_DR(plat->bank), 1 << pin);
	else
		clrbits_le32(plat->base + GPIO_SWPORT_DR(plat->bank), 1 << pin);

	return 0;
}

/**
 * @NO{S02E02C08}
 * @ASIL{B}
 * @brief get the current function for a GPIO pin
 *
 * @param[in] dev: Device to check
 * @param[in] offset: Offset of device GPIO to check
 *
 * @retval function: input or output
 *
 * @data_read None
 * @data_read None
 * @data_updated None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @design
 */
static int dwapb_gpio_get_function(struct udevice *dev, unsigned offset)
{
	struct gpio_dwapb_plat *plat = dev_get_plat(dev);
	u32 gpio;

	gpio = readl(plat->base + GPIO_SWPORT_DDR(plat->bank));

	if (gpio & BIT(offset))
		return GPIOF_OUTPUT;
	else
		return GPIOF_INPUT;
}

/**
 * @NO{S02E02C08}
 * @ASIL{B}
 * @brief Get value from specific gpio pin
 *
 * @param[in] dev: device ponier
 * @param[in] pin: gpio pin to read
 *
 * @retval value : 0 or 1
 *
 * @data_read None
 * @data_read None
 * @data_updated None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @design
 */
static int dwapb_gpio_get_value(struct udevice *dev, unsigned pin)
{
	struct gpio_dwapb_plat *plat = dev_get_plat(dev);
	u32 value;

	if (dwapb_gpio_get_function(dev, pin) == GPIOF_OUTPUT)
		value = readl(plat->base + GPIO_SWPORT_DR(plat->bank));
	else
		value = readl(plat->base + GPIO_EXT_PORT(plat->bank));
	return !!(value & BIT(pin));
}

static const struct dm_gpio_ops gpio_dwapb_ops = {
	.direction_input	= dwapb_gpio_direction_input,
	.direction_output	= dwapb_gpio_direction_output,
	.get_value		= dwapb_gpio_get_value,
	.set_value		= dwapb_gpio_set_value,
	.get_function		= dwapb_gpio_get_function,
};

/**
 * @NO{S02E02C08U}
 * @ASIL{B}
 * @brief Software reset gpio device
 *
 * @param[in] dev: device pointer
 *
 * @retval =0: success
 * @retval <0: failure
 *
 * @data_read None
 * @data_read None
 * @data_updated None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @design
 */
static int gpio_dwapb_reset(struct udevice *dev)
{
	int ret;
	struct gpio_dwapb_priv *priv = dev_get_priv(dev);

	ret = reset_get_bulk(dev, &priv->resets);
	if (ret) {
		/* Return 0 if error due to !CONFIG_DM_RESET and reset
		 * DT property is not present.
		 */
		if (ret == -ENOENT || ret == -ENOTSUPP)
			return 0;

		dev_warn(dev, "Can't get reset: %d\n", ret);
		return ret;
	}

	ret = reset_deassert_bulk(&priv->resets);
	if (ret) {
		reset_release_bulk(&priv->resets);
		dev_err(dev, "Failed to reset: %d\n", ret);
		return ret;
	}

	return 0;
}

/**
 * @NO{S02E02C08}
 * @ASIL{B}
 * @brief Initialize gpio device
 *
 * @param[in] dev: device pointer
 *
 * @retval =0: success
 * @retval <0: failure
 *
 * @data_read None
 * @data_read None
 * @data_updated None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @design
 */
static int gpio_dwapb_probe(struct udevice *dev)
{
	struct gpio_dev_priv *priv = dev_get_uclass_priv(dev);
	struct gpio_dwapb_plat *plat = dev_get_plat(dev);

	if (!plat) {
		/* Reset on parent device only */
		return gpio_dwapb_reset(dev);
	}

	priv->gpio_count = plat->pins;
	priv->bank_name = plat->name;

	return 0;
}

/**
 * @NO{S02E02C08}
 * @ASIL{B}
 * @brief Bind gpio driver with gpio device
 *
 * @param[in/out] dev:
 *
 * @retval =0: success
 * @retval <0: failure
 *
 * @data_read None
 * @data_read None
 * @data_updated None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @design
 */
static int gpio_dwapb_bind(struct udevice *dev)
{
	struct gpio_dwapb_plat *plat = dev_get_plat(dev);
	struct udevice *subdev;
	fdt_addr_t base;
	int ret, bank = 0;
	ofnode node;

	/* If this is a child device, there is nothing to do here */
	if (plat)
		return 0;

	base = dev_read_addr(dev);
	if (base == FDT_ADDR_T_NONE) {
		debug("Can't get the GPIO register base address\n");
		return -ENXIO;
	}

	for (node = dev_read_first_subnode(dev); ofnode_valid(node);
	     node = dev_read_next_subnode(node)) {
		if (!ofnode_read_bool(node, "gpio-controller"))
			continue;

		plat = devm_kcalloc(dev, 1, sizeof(*plat), GFP_KERNEL);
		if (!plat)
			return -ENOMEM;

		plat->base = (void *)base;
		plat->bank = bank;
		plat->pins = ofnode_read_u32_default(node, "snps,nr-gpios", 0);

		if (ofnode_read_string_index(node, "bank-name", 0,
					     &plat->name)) {
			/*
			 * Fall back to node name. This means accessing pins
			 * via bank name won't work.
			 */
			char name[32];

			snprintf(name, sizeof(name), "%s_",
				 ofnode_get_name(node));
			plat->name = strdup(name);
			if (!plat->name) {
				kfree(plat);
				return -ENOMEM;
			}
		}

		ret = device_bind(dev, dev->driver, plat->name, plat, node,
				  &subdev);
		if (ret)
			return ret;

		bank++;
	}

	return 0;
}

/**
 * @NO{S02E02C08}
 * @ASIL{B}
 * @brief Reset GPIO device via software reset
 *
 * @param[in] dev: device pointer
 *
 * @retval =0: success
 * @retval <0: failure
 *
 * @data_read None
 * @data_read None
 * @data_updated None
 * @data_updated None
 * @compatibility None
 *
 * @callgraph
 * @design
 */
static int gpio_dwapb_remove(struct udevice *dev)
{
	struct gpio_dwapb_plat *plat = dev_get_plat(dev);
	struct gpio_dwapb_priv *priv = dev_get_priv(dev);

	if (!plat && priv)
		return reset_release_bulk(&priv->resets);

	return 0;
}

static const struct udevice_id gpio_dwapb_ids[] = {
	{ .compatible = "snps,dw-apb-gpio" },
	{ }
};

U_BOOT_DRIVER(gpio_dwapb) = {
	.name		= "gpio-dwapb",
	.id		= UCLASS_GPIO,
	.of_match	= gpio_dwapb_ids,
	.ops		= &gpio_dwapb_ops,
	.bind		= gpio_dwapb_bind,
	.probe		= gpio_dwapb_probe,
	.remove		= gpio_dwapb_remove,
	.priv_auto	  = sizeof(struct gpio_dwapb_priv),
};
