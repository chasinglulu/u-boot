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

#include "dwc_eth_qos.h"

__weak u32 lua_get_eqos_csr_clk(void)
{
	return 100 * 1000000;
}

__weak int lua_eqos_txclk_set_rate(unsigned long rate)
{
	return 0;
}

static ulong eqos_get_tick_clk_rate_lua(struct udevice *dev)
{
	return lua_get_eqos_csr_clk();
}

static int eqos_probe_resources_lua(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	phy_interface_t interface;

	debug("%s(dev=%p):\n", __func__, dev);

	interface = eqos->config->interface(dev);

	if (interface == PHY_INTERFACE_MODE_NA) {
		pr_err("Invalid PHY interface\n");
		return -EINVAL;
	}

	debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_set_tx_clk_speed_lua(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	ulong rate;
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	switch (eqos->phy->speed) {
	case SPEED_1000:
		rate = 125 * 1000 * 1000;
		break;
	case SPEED_100:
		rate = 25 * 1000 * 1000;
		break;
	case SPEED_10:
		rate = 2.5 * 1000 * 1000;
		break;
	default:
		pr_err("invalid speed %d", eqos->phy->speed);
		return -EINVAL;
	}

	ret = lua_eqos_txclk_set_rate(rate);
	if (ret < 0) {
		pr_err("lambert (tx_clk, %lu) failed: %d", rate, ret);
		return ret;
	}

	return 0;
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

static struct eqos_ops eqos_lua_ops = {
	.eqos_inval_desc = eqos_inval_desc_generic,
	.eqos_flush_desc = eqos_flush_desc_generic,
	.eqos_inval_buffer = eqos_inval_buffer_generic,
	.eqos_flush_buffer = eqos_flush_buffer_generic,
	.eqos_probe_resources = eqos_probe_resources_lua,
	.eqos_remove_resources = eqos_null_ops,
	.eqos_stop_resets = eqos_null_ops,
	.eqos_start_resets = eqos_null_ops,
	.eqos_stop_clks = eqos_null_ops,
	.eqos_start_clks = eqos_null_ops,
	.eqos_calibrate_pads = eqos_null_ops,
	.eqos_disable_calibration = eqos_null_ops,
	.eqos_set_tx_clk_speed = eqos_set_tx_clk_speed_lua,
	.eqos_get_enetaddr = eqos_get_enetaddr_lua,
	.eqos_get_tick_clk_rate = eqos_get_tick_clk_rate_lua,
};

struct eqos_config __maybe_unused eqos_lua_config = {
	.reg_access_always_ok = false,
	.mdio_wait = 10,
	.swr_wait = 50,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB,
	.config_mac_mdio = EQOS_MAC_MDIO_ADDRESS_CR_250_300,
	.axi_bus_width = EQOS_AXI_WIDTH_64,
	.interface = dev_read_phy_mode,
	.ops = &eqos_lua_ops
};