// SPDX-License-Identifier: GPL-2.0+
/*
 * SMCCC based System Control Interface Driver
 *
 * Copyright (C) 2022 Horizon Robotics Co,.Ltd.
 *
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device.h>
#include <dm/devres.h>
#include <dm/device-internal.h>
#include <dm/of.h>
#include <dm/ofnode.h>
#include <dm/uclass-id.h>
#include <dm/uclass.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/arm-smccc.h>

#include "hobot_sci.h"


#define DRIVER_NAME	"hobot_sci"
#define SMCCC_DRIVER_NAME "hobot_sci_smccc"

enum {
	HOBOT_SCI_BOOTFLAGS_SOC,
	HOBOT_SCI_BOOTFLAGS_BOARD,
	HOBOT_SCI_BOOTFLAGS_EFUSE,
	HOBOT_SCI_BOOTFLAGS_COUNT,
};

struct hobot_sci {
	struct udevice *dev;
	u64 base;
	u64 size;
	int match;
};

struct hobot_sci_ops {
	unsigned long (*read)(struct udevice *dev, unsigned long addr);
	unsigned long (*write)(struct udevice *dev, unsigned long addr, unsigned long val);
	unsigned long (*update)(struct udevice *dev, unsigned long addr, unsigned long val, unsigned long mask);
};

/**
 * struct hobot_sci_smccc - Description of an hobot SCI SMCCC transport
 * @func_id:	SMCCC function ID used by the hobot SCI
 * @cmd_num:		0:read 1:write 2:update
 */
struct hobot_sci_smccc {
	unsigned long func_id;
	unsigned long cmd_num;
};

struct udevice* do_sci_probe(int match)
{
	struct hobot_sci *plat;
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_FIRMWARE, &uc);
	if (ret)
		return ERR_PTR(-ENODEV);
	
	uclass_foreach_dev(dev, uc) {
		if (dev->driver == DM_DRIVER_GET(hobot_sci)) {
			plat = dev_get_plat(dev);
			if (plat->match == match) {
				if (device_probe(dev))
					return ERR_PTR(-EINVAL);
				return dev;
			}
		}
	}

	return ERR_PTR(-ENODEV);
}

int do_sci_smccc_probe(struct udevice *dev)
{
	int ret;

	if (!dev)
		return -EINVAL;

	ret = uclass_get_device_by_driver(UCLASS_FIRMWARE, DM_DRIVER_GET(hobot_sci_smccc), &dev); 
		return ret;

	return 0;
}

u32 hobot_sci_get_action(void)
{
	struct udevice *dev;
	const struct hobot_sci *plat;
	const struct hobot_sci_ops *ops;

	dev = do_sci_probe(HOBOT_SCI_BOOTFLAGS_SOC);
	if (dev == NULL)
		return -ENXIO;
	plat = dev_get_plat(dev);

	ops = dev_get_driver_ops(plat->dev);
	if (!ops || !ops->read)
		return -ENOSYS;

	return ops->read(dev, plat->base);
}

static unsigned long invoke_smc_fn
		(unsigned long function_id, unsigned long arg0,
		 unsigned long arg1, unsigned long arg2,
		 unsigned long arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, arg3, 0, 0, 0, &res);

	return res.a0;
}

static unsigned long hobot_sci_read(struct udevice *dev, unsigned long addr)
{
	struct hobot_sci_smccc *info = dev_get_priv(dev);

	return invoke_smc_fn(info->func_id, 0, addr, 0, 0);
}

static unsigned long hobot_sci_write(struct udevice *dev, unsigned long addr, unsigned long val)
{
	struct hobot_sci_smccc *info = dev_get_priv(dev);

	return invoke_smc_fn(info->func_id, 1, addr, val, 0);
}

static unsigned long hobot_sci_update(struct udevice *dev, unsigned long addr, unsigned long val, unsigned long mask)
{
	struct hobot_sci_smccc *info = dev_get_priv(dev);

	return invoke_smc_fn(info->func_id, 2, addr, val, mask);
}

static int hobot_sci_smccc_probe(struct udevice *dev)
{
	struct hobot_sci_smccc *info = dev_get_priv(dev);
	unsigned int func_id, cmd_num;

	if (dev_read_u32(dev, "arm,smc-id", &func_id)) {
		dev_err(dev, "Missing property func-id\n");
		return -EINVAL;
	}
	info->func_id = func_id;

	if (dev_read_u32(dev, "hobot,sci-cmds", &cmd_num)) {
		dev_err(dev, "Missing property sci-cmds\n");
		return -EINVAL;
	}
	info->cmd_num = cmd_num;

	return 0;
}

static int hobot_sci_of_to_plat(struct udevice *dev)
{
	struct hobot_sci *plat = dev_get_plat(dev);
	fdt_addr_t addr;
	fdt_size_t size;
	int index;

	addr = dev_read_addr_size(dev, "reg", &size);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;
	plat->base = addr;
	plat->size = size;

	if (dev_read_u32(dev, "hobot,sci-index", &index)) {
		dev_err(dev, "Missing property func-id\n");
		return -EINVAL;
	}
	plat->match = index;

	return 0;
}

static int hobot_sci_probe(struct udevice *dev)
{
	struct hobot_sci *plat = dev_get_plat(dev);
	struct udevice *smccc_dev = NULL;
	int ret;

	ret = do_sci_smccc_probe(smccc_dev);
	if (ret)
		return ret;
	plat->dev = smccc_dev;

	return 0;
}

static struct hobot_sci_ops sci_ops = {
	.read = hobot_sci_read,
	.write = hobot_sci_write,
	.update = hobot_sci_update,
};

static const struct udevice_id hobot_sci_smccc_ids[] = {
	{
		.compatible = "hobot,j5-sci-smccc",
	},
	{ /* Sentinel */ },
};

U_BOOT_DRIVER(hobot_sci_smccc) = {
	.name = SMCCC_DRIVER_NAME,
	.id = UCLASS_FIRMWARE,
	.of_match = hobot_sci_smccc_ids,
	.probe = hobot_sci_smccc_probe,
	.ops = &sci_ops,
	.priv_auto = sizeof(struct hobot_sci_smccc),
};

static const struct udevice_id hobot_sci_ids[] = {
	{
		.compatible = "hobot,j5-sci",
	},
	{ /* Sentinel */ },
};

U_BOOT_DRIVER(hobot_sci) = {
	.name = DRIVER_NAME,
	.id = UCLASS_FIRMWARE,
	.of_match = hobot_sci_ids,
	.of_to_plat = hobot_sci_of_to_plat,
	.probe = hobot_sci_probe,
	.plat_auto = sizeof(struct hobot_sci),
};
