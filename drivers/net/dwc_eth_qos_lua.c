// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2023 (C) Charley <wangkart@aliyun.com>
 */

#include <common.h>
#include <clk.h>
#include <cpu_func.h>
#include <dm.h>
#include <errno.h>
#include <eth_phy.h>
#include <log.h>
#include <malloc.h>
#include <memalign.h>
#include <miiphy.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <reset.h>
#include <wait_bit.h>
#include <asm/cache.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <syscon.h>
#include <regmap.h>

#include "dwc_eth_qos.h"

#define EPLL_5M       (5000000UL)
#define EPLL_50M      (50000000UL)
#define EPLL_250M     (250000000UL)

__weak int lua_eqos_txclk_set_rate(unsigned long rate)
{
	uint64_t addr;
	uint32_t val;
	uint32_t mask = BIT(8) | BIT(9);

	addr = 0x0E001000;
	val = readl(addr);
	val &= ~mask;

	switch (rate) {
	case EPLL_5M:
		break;
	case EPLL_50M:
		val |= BIT(8);
		break;
	case EPLL_250M:
		val |= BIT(9);
		break;
	}
	writel(val, addr);

	return 0;
}

static ulong eqos_get_tick_clk_rate_lua(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	ulong rate;

	rate = clk_get_rate(&eqos->clk_slave_bus);
	if (!rate)
		rate = dev_read_u32_default(dev, "clock-frequency", 1000000);

	return rate;
}

static int eqos_interface_eth_init(struct udevice *dev,
				    phy_interface_t interface_type)
{
	struct regmap *regmap;
	uint32_t val;
	uint32_t offset;
	int ret;

	regmap = syscon_regmap_lookup_by_phandle(dev, "axera,syscon-eth0");
	if (IS_ERR(regmap))
		return -ENODEV;

	ret = dev_read_u32_index(dev, "axera,syscon-eth0", 1, &offset);
	if (ret)
		return ret;

	regmap_read(regmap, offset, &val);
	val &= ~BIT(4);

	switch ((int)interface_type) {
	case PHY_INTERFACE_MODE_RGMII:
		val |= BIT(4);
		break;
	case PHY_INTERFACE_MODE_RMII:
	case PHY_INTERFACE_MODE_GMII:
		break;
	}
	regmap_write(regmap, offset, val);

	return 0;
}

static int eqos_probe_resources_lua(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	phy_interface_t interface;
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = reset_get_by_name(dev, "arst", &eqos->reset_ctl);
	if (ret) {
		pr_err("reset_get_by_name(rst) failed: %d", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "aclk_eqos", &eqos->clk_slave_bus);
	if (ret) {
		pr_err("clk_get_by_name(aclk_eqos) failed: %d", ret);
		goto err_free_reset_eqos;
	}

	ret = clk_get_by_name(dev, "ptp_ref", &eqos->clk_ptp_ref);
	if (ret) {
		pr_err("clk_get_by_name(ptp_ref) failed: %d", ret);
		goto err_free_clk_eqos;
	}

	ret = clk_get_by_name(dev, "ephy_ref", &eqos->clk_ck);
	if (ret) {
		pr_err("clk_get_by_name(ephy_ref) failed: %d", ret);
		goto err_free_clk_ptp_ref;
	}

	ret = clk_get_by_name(dev, "rmii_phy", &eqos->clk_rx);
	if (ret) {
		pr_err("clk_get_by_name(rmii_phy) failed: %d", ret);
		goto err_free_clk_ck;
	}

	ret = clk_get_by_name(dev, "rgmii_tx", &eqos->clk_tx);
	if (ret) {
		pr_err("clk_get_by_name(rmii_phy) failed: %d", ret);
		goto err_free_clk_rx;
	}

	interface = eqos->config->interface(dev);

	ret = -EINVAL;
	if (interface == PHY_INTERFACE_MODE_NA) {
		pr_err("Invalid PHY interface\n");
		goto err_free_clk_tx;
	}

	if (eqos_interface_eth_init(dev, interface))
		goto err_free_clk_tx;

	debug("%s: OK\n", __func__);
	return 0;

err_free_clk_tx:
	clk_free(&eqos->clk_tx);
err_free_clk_rx:
	clk_free(&eqos->clk_rx);
err_free_clk_ck:
	clk_free(&eqos->clk_ck);
err_free_clk_ptp_ref:
	clk_free(&eqos->clk_ptp_ref);
err_free_clk_eqos:
	clk_free(&eqos->clk_slave_bus);
err_free_reset_eqos:
	reset_free(&eqos->reset_ctl);
	return ret;
}

static int eqos_set_rgmii_tx_clk_speed(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	ulong rate;
	int ret;

	switch (eqos->phy->speed) {
	case SPEED_1000:
		rate = EPLL_250M;
		break;
	case SPEED_100:
		rate = EPLL_50M;
		break;
	case SPEED_10:
		rate = EPLL_5M;
		break;
	default:
		pr_err("invalid speed %d", eqos->phy->speed);
		return -EINVAL;
	}

	ret = lua_eqos_txclk_set_rate(rate);
	if (ret < 0) {
		pr_err("laguna (tx_clk, %lu) failed: %d", rate, ret);
		return ret;
	}

	return 0;
}

static int eqos_set_rmii_rx_clk_speed(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	uint64_t addr;
	uint32_t val;

	addr = 0x0E001000 + 0x168;
	val = readl(addr);
	val &= ~0xF;

	switch (eqos->phy->speed) {
	case SPEED_10:
		val |= 0xA;
		break;
	case SPEED_100:
		val |= 0x1;
		break;
	}

	/* Enable divn_update */
	val |= BIT(4);
	writel(val, addr);

	return 0;
}

static int eqos_set_tx_clk_speed_lua(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	phy_interface_t interface;
	int ret = 0;

	debug("%s(dev=%p):\n", __func__, dev);

	interface = eqos->config->interface(dev);
	switch ((int)interface) {
	case PHY_INTERFACE_MODE_RGMII:
		debug("Configuring RGMII TX clk...\n");
		ret = eqos_set_rgmii_tx_clk_speed(dev);
		break;
	case PHY_INTERFACE_MODE_RMII:
		debug("Configuring RMII RX clk...\n");
		ret = eqos_set_rmii_rx_clk_speed(dev);
	case PHY_INTERFACE_MODE_GMII:
		debug("No need to configure GMII clk\n");

		/* In HAPS platform, configure MAC <-> PHY with 1000M
		 * But PHY Link Speed with 100M */
		clrbits_le32(&eqos->mac_regs->configuration,
			    EQOS_MAC_CONFIGURATION_PS | EQOS_MAC_CONFIGURATION_FES);
		break;
	}

	return ret;
}

static int eqos_get_enetaddr_lua(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct eqos_priv *eqos = dev_get_priv(dev);
	uint32_t val;

	val = readl(&eqos->mac_regs->address0_high);
	pdata->enetaddr[4] = val & 0xFF;
	pdata->enetaddr[5] = (val >> 8) & 0xFF;

	val = readl(&eqos->mac_regs->address0_low);
	pdata->enetaddr[0] = val & 0xFF;
	pdata->enetaddr[1] = (val >> 8) & 0xFF;
	pdata->enetaddr[2] = (val >> 16) & 0xFF;
	pdata->enetaddr[3] = (val >> 24) & 0xFF;

	return 0;
}

static int eqos_stop_resets_lua(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	reset_assert(&eqos->reset_ctl);

	return 0;
}

static int eqos_start_resets_lua(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;

	ret = reset_assert(&eqos->reset_ctl);
	if (ret < 0) {
		pr_err("reset_assert() failed: %d", ret);
		return ret;
	}

	udelay(2);

	ret = reset_deassert(&eqos->reset_ctl);
	if (ret < 0) {
		pr_err("reset_deassert() failed: %d", ret);
		return ret;
	}

	if (eqos->phy) {
		phy_config(eqos->phy);
	}

	return ret;
}

static int eqos_stop_clks_lua(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	clk_disable(&eqos->clk_tx);
	clk_disable(&eqos->clk_rx);
	clk_disable(&eqos->clk_ck);
	clk_disable(&eqos->clk_slave_bus);

	debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_start_clks_lua(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = clk_enable(&eqos->clk_slave_bus);
	if (ret < 0) {
		pr_err("clk_enable(clk_slave_bus) failed: %d", ret);
		goto err;
	}

	ret = clk_enable(&eqos->clk_ck);
	if (ret < 0) {
		pr_err("clk_enable(clk_ck) failed: %d", ret);
		goto err_disable_clk_slave_bus;
	}

	ret = clk_enable(&eqos->clk_rx);
	if (ret < 0) {
		pr_err("clk_enable(clk_rx) failed: %d", ret);
		goto err_disable_clk_ck;
	}

	ret = clk_enable(&eqos->clk_tx);
	if (ret < 0) {
		pr_err("clk_enable(clk_tx) failed: %d", ret);
		goto err_disable_clk_rx;
	}

	debug("%s: OK\n", __func__);
	return 0;

err_disable_clk_rx:
	clk_disable(&eqos->clk_rx);
err_disable_clk_ck:
	clk_disable(&eqos->clk_ck);
err_disable_clk_slave_bus:
	clk_disable(&eqos->clk_slave_bus);
err:
	debug("%s: FAILED: %d\n", __func__, ret);
	return ret;
}

static struct eqos_ops eqos_lua_ops = {
	.eqos_inval_desc = eqos_inval_desc_generic,
	.eqos_flush_desc = eqos_flush_desc_generic,
	.eqos_inval_buffer = eqos_inval_buffer_generic,
	.eqos_flush_buffer = eqos_flush_buffer_generic,
	.eqos_probe_resources = eqos_probe_resources_lua,
	.eqos_remove_resources = eqos_null_ops,
	.eqos_stop_resets = eqos_stop_resets_lua,
	.eqos_start_resets = eqos_start_resets_lua,
	.eqos_stop_clks = eqos_stop_clks_lua,
	.eqos_start_clks = eqos_start_clks_lua,
	.eqos_calibrate_pads = eqos_null_ops,
	.eqos_disable_calibration = eqos_null_ops,
	.eqos_set_tx_clk_speed = eqos_set_tx_clk_speed_lua,
	.eqos_get_enetaddr = eqos_get_enetaddr_lua,
	.eqos_get_tick_clk_rate = eqos_get_tick_clk_rate_lua,
};

struct eqos_config __maybe_unused eqos_lua_config = {
	.reg_access_always_ok = false,
	.mdio_wait = 100,
	.swr_wait = 50,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_AV,
	.config_mac_mdio = EQOS_MAC_MDIO_ADDRESS_CR_60_100,
	.axi_bus_width = EQOS_AXI_WIDTH_64,
	.interface = dev_read_phy_mode,
	.ops = &eqos_lua_ops
};
