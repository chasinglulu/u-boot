// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2022 Beijing Horizon Robotics Co., Ltd
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>
#include <reset-uclass.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <asm/arch/peri_sys.h>
#include <asm/arch/hardware.h>

/*
 * Each reg has 32 bits reset signal for devices
 */
 #define HOBOT_RESET_NUM_IN_REG		32

struct hobot_reset_priv {
	void __iomem *base;
	/* hobot reset reg locate at peri sysreg unit */
	u32 reset_reg_offset;
	/* hobot reset reg number */
	u32 reset_reg_num;
};

static int hobot_reset_request(struct reset_ctl *reset_ctl)
{
	struct hobot_reset_priv *priv = dev_get_priv(reset_ctl->dev);

	debug("%s(reset_ctl=%p) (dev=%p, id=%lu) (reg_num=%d)\n", __func__,
	      reset_ctl, reset_ctl->dev, reset_ctl->id, priv->reset_reg_num);

	if (reset_ctl->id / HOBOT_RESET_NUM_IN_REG >= priv->reset_reg_num)
		return -EINVAL;

	return 0;
}

static int hobot_reset_free(struct reset_ctl *reset_ctl)
{
	debug("%s(reset_ctl=%p) (dev=%p, id=%lu) (reg_addr=%ld)\n", __func__,
			reset_ctl, reset_ctl->dev, reset_ctl->id, reset_ctl->data);

	return 0;
}

/**
 * @NO{S02E02C11}
 * @ASIL{B}
 * @brief Assert a reset signal
 *
 * @param[in] reset_ctl: The reset signal to assert
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
 * @design
 */
static int hobot_reset_assert(struct reset_ctl *reset_ctl)
{
	struct hobot_reset_priv *priv = dev_get_priv(reset_ctl->dev);
	int bank =  reset_ctl->id / HOBOT_RESET_NUM_IN_REG;
	int offset =  reset_ctl->id % HOBOT_RESET_NUM_IN_REG;

	debug("%s(reset_ctl=%p) (dev=%p, id=%lu) (reg_addr=%p)\n", __func__,
	      reset_ctl, reset_ctl->dev, reset_ctl->id,
	      priv->base + (bank * 4));

	hb_setreg(priv->base + (bank * 4), BIT(offset));

	return 0;
}

/**
 * @NO{S02E02C11}
 * @ASIL{B}
 * @brief Deassert a reset signal
 *
 * @param[in/out] reset_ctl: The reset signal to deassert
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
 * @design
 */
static int hobot_reset_deassert(struct reset_ctl *reset_ctl)
{
	struct hobot_reset_priv *priv = dev_get_priv(reset_ctl->dev);
	int bank =  reset_ctl->id / HOBOT_RESET_NUM_IN_REG;
	int offset =  reset_ctl->id % HOBOT_RESET_NUM_IN_REG;

	debug("%s(reset_ctl=%p) (dev=%p, id=%lu) (reg_addr=%p)\n", __func__,
	      reset_ctl, reset_ctl->dev, reset_ctl->id,
	      priv->base + (bank * 4));

	hb_clrreg(priv->base + (bank * 4), BIT(offset));

	return 0;
}

struct reset_ops hobot_reset_ops = {
	.request = hobot_reset_request,
	.rfree = hobot_reset_free,
	.rst_assert = hobot_reset_assert,
	.rst_deassert = hobot_reset_deassert,
};

/**
 * @NO{S02E02C11}
 * @ASIL{B}
 * @brief Initialize reset device
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
 * @design
 */
static int hobot_reset_probe(struct udevice *dev)
{
	struct hobot_reset_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;
	fdt_size_t size;

	addr = dev_read_addr_size(dev, "reg", &size);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->reset_reg_offset = offsetof(struct sigi_peri_sys, softrst_con[0]);
	priv->reset_reg_num = 4;

	if ((priv->reset_reg_offset == 0) && (priv->reset_reg_num == 0))
		return -EINVAL;

	addr += priv->reset_reg_offset;
	priv->base = ioremap(addr, size);

	debug("%s(base=%p) (reg_offset=0x%x, reg_num=%d)\n", __func__,
	      priv->base, priv->reset_reg_offset, priv->reset_reg_num);

	return 0;
}

int hobot_reset_bind(struct udevice *pdev, u32 reg_offset, u32 reg_number)
{
	struct udevice *rst_dev;
	struct hobot_reset_priv *priv;
	int ret;

	 ret = device_bind_driver_to_node(pdev, "hobot_reset", "reset",
					  dev_ofnode(pdev), &rst_dev);
	if (ret) {
		debug("Warning: No hobot reset driver: ret=%d\n", ret);
		return ret;
	}
	priv = malloc(sizeof(struct hobot_reset_priv));
	dev_set_priv(rst_dev, priv);

	return 0;
}

static const struct udevice_id hobot_reset_ids[] = {
	{ .compatible = "hobot,sigi-reset" },
	{ }
};

U_BOOT_DRIVER(rockchip_reset) = {
	.name = "hobot_reset",
	.id = UCLASS_RESET,
	.probe = hobot_reset_probe,
	.of_match = hobot_reset_ids,
	.ops = &hobot_reset_ops,
	.priv_auto	= sizeof(struct hobot_reset_priv),
};
