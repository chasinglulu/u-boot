// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Charley
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <malloc.h>
#include <sdhci.h>
#include <linux/err.h>
#include <dm/lists.h>

struct lmt_sdhci_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

static int lmt_sdhci_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct lmt_sdhci_plat *plat = dev_get_plat(dev);
	struct sdhci_host *host = dev_get_priv(dev);
	int ret;

	host->name = dev->name;
	host->ioaddr = dev_read_addr_ptr(dev);

	ret = mmc_of_parse(dev, &plat->cfg);
	if (ret)
		return ret;

	host->mmc = &plat->mmc;
	host->mmc->dev = dev;
	host->mmc->priv = host;
	upriv->mmc = host->mmc;

	ret = sdhci_setup_cfg(&plat->cfg, host, 0, 0);
	if (ret)
		goto err;

	ret = sdhci_probe(dev);
	if (ret)
		goto err;

	return 0;

err:
	return ret;
}

static int lmt_sdhci_bind(struct udevice *dev)
{
	struct lmt_sdhci_plat *plat = dev_get_plat(dev);

	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id lmt_sdhci_ids[] = {
	{ .compatible = "ax,lambert-sdhci" },
	{ }
};

U_BOOT_DRIVER(lmt_sdhci_drv) = {
	.name		= "lmt_sdhci",
	.id		= UCLASS_MMC,
	.of_match	= lmt_sdhci_ids,
	.ops		= &sdhci_ops,
	.bind		= lmt_sdhci_bind,
	.probe		= lmt_sdhci_probe,
	.priv_auto	= sizeof(struct sdhci_host),
	.plat_auto	= sizeof(struct lmt_sdhci_plat),
};