// SPDX-License-Identifier: GPL-2.0+
/*
 * SMCCC based System Control Interface Driver
 *
 * Copyright (C) 2022 Horizon Robotics Co,.Ltd.
 *
 */

#include "dm/of.h"
#include "dm/ofnode.h"
#include "linux/errno.h"
#include <common.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/devres.h>
#include <linux/arm-smccc.h>

#include "hobot_sci.h"

/*
 *	hb_sci {
 * 		compatible = "hobot,j5-sci";
 *		fid = <0x820009>;
 *
 * 		soc-state {
 * 			reg = <>;
 * 			#bootstate-cells = <2>;
 * 		};
 *
 * 		board-state {
 * 			reg = <>
 * 			#bootstate-cells = <2>;
 * 		};
 *
 *		efuse-state {
 * 			reg= <0x>;
 * 			#bootstate-cells = <2>;
 * 		};
 *	};
 *	reboot-mode {
 * 		compatible = "syscon-reboot-mode";
 * 		offset = <&board 0x08>;
 * 	};
 *
 *	reboot-reason {
 * 		compatible = "syscon-reboot-reason";
 * 		offset = <&board 0x04>;
 * 	};
 *
 */

#define DRIVER_NAME	"hobot_sci"

enum {
	BOOTSTATE_SOC = 0,
	BOOTSTATE_BOARD,
	BOOTSTATE_EFUSE,
	BOOTSTATE_COUNT,
};

struct bootstate {
	u64 base_addr;
	u64 size;
};

struct hobot_sci_ops {
	unsigned long (*read)(struct udevice *dev, unsigned long addr);
	unsigned long (*write)(struct udevice *dev, unsigned long addr, unsigned long val);
	unsigned long (*update)(struct udevice *dev, unsigned long addr, unsigned long val, unsigned long mask);
};

struct hobot_sci_info {
	struct udevice *dev;
	unsigned long function_id;
	struct bootstate ranges[BOOTSTATE_COUNT];
};

unsigned long __maybe_unused hobot_sci_get_base(struct udevice *dev, unsigned long addr)
{
	struct  hobot_sci_info *info = dev_get_priv(dev);
	struct bootstate *ranges = info->ranges;

	unsigned long start, end;
	int i;

	for(i = BOOTSTATE_SOC; i < BOOTSTATE_COUNT; i++)
	{
		start = ranges[i].base_addr;
		end = ranges[i].base_addr + ranges[i].size - 1;

		if (addr >= start && addr <= end)
			return start;
	}

	return 0;
}

struct udevice* do_sci_probe(void)
{
	struct udevice *dev;

	uclass_get_device_by_name(UCLASS_FIRMWARE, DRIVER_NAME, &dev);

	return dev;
}

u32 hobot_sci_get_action(void)
{
	struct udevice *dev;
	const struct hobot_sci_ops *ops;

	dev = do_sci_probe();
	if (dev == NULL)
		return -ENXIO;

	ops = dev_get_driver_ops(dev);
	if (!ops || !ops->read)
		return -ENOSYS;

	return ops->read(dev, BOOTSTATE_ACTION_ADDR);
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
	struct hobot_sci_info *info = dev_get_priv(dev);

	return invoke_smc_fn(info->function_id, 0, addr, 0, 0);
}

static unsigned long hobot_sci_write(struct udevice *dev, unsigned long addr, unsigned long val)
{
	struct hobot_sci_info *info = dev_get_priv(dev);

	return invoke_smc_fn(info->function_id, 1, addr, val, 0);
}

static unsigned long hobot_sci_update(struct udevice *dev, unsigned long addr, unsigned long val, unsigned long mask)
{
	struct hobot_sci_info *info = dev_get_priv(dev);

	return invoke_smc_fn(info->function_id, 2, addr, val, mask);
}

static int hobot_sci_probe(struct udevice *dev)
{
	struct hobot_sci_info *info = dev_get_priv(dev);
	struct bootstate *ranges = info->ranges;
	unsigned int function_id;
	ofnode child;
	int ret, i = 0;

	ret = ofnode_read_u32(dev_ofnode(dev), "fid", &function_id);
	if (ret)
		return ret;

	info->function_id = function_id;

	ofnode_for_each_subnode(child, dev_ofnode(dev)) {
		ranges[i].base_addr = ofnode_get_addr(child);
		ranges[i].size = ofnode_get_size(child);
		i++;
	}

	return 0;
}

static const struct udevice_id hobot_sci_ids[] = {
	{
		.compatible = "hobot,j5-sci",
	},
	{ /* Sentinel */ },
};

static struct hobot_sci_ops sci_ops = {
	.read = hobot_sci_read,
	.write = hobot_sci_write,
	.update = hobot_sci_update,
};

U_BOOT_DRIVER(ti_sci) = {
	.name = DRIVER_NAME,
	.id = UCLASS_FIRMWARE,
	.of_match = hobot_sci_ids,
	.probe = hobot_sci_probe,
	.ops = &sci_ops,
	.priv_auto = sizeof(struct hobot_sci_info),
};