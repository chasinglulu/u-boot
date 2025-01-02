// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <part.h>
#include <errno.h>
#include <blk.h>
#include <fdl.h>
#include <mtd.h>
#include <env.h>
#include <mmc.h>

#include <dm/device-internal.h>
#include <linux/err.h>
#include <bootdev/bootdevice.h>
#include <asm/arch/bootparams.h>

/* Supported partition identifier for safety R5F */
const char *safe_part_id[] = {
	"sbl",
	"spl",
	"rtos",
	"sbl_a",
	"sbl_b",
	"spl_a",
	"spl_b",
	"rtos_a",
	"rtos_b",
};

/* Supported partition identifier for Main A55 */
const char *main_part_id[] = {
	"misc",
	"ubootenv",
	"atf",
	"optee",
	"uboot",
	"boot",
	"kernel",
	"rootfs",
	"misc_bak",
	"uboot_bak",
	"atf_a",
	"atf_b",
	"optee_a",
	"optee_b",
	"uboot_a",
	"uboot_b",
	"boot_a",
	"boot_b",
	"kernel_a",
	"kernel_b",
	"rootfs_a",
	"rootfs_b",
};

inline int get_safe_part_id_count(void)
{
	return ARRAY_SIZE(safe_part_id);
}

inline int get_main_part_id_count(void)
{
	return ARRAY_SIZE(main_part_id);
}

struct blk_desc *get_blk_by_devnum(enum if_type if_type, int devnum)
{
	struct blk_desc *dev_desc;
	int max_devnum;

	if (!blk_enabled())
		return NULL;

	max_devnum = blk_find_max_devnum(if_type);
	debug("max devnum: %d\n", max_devnum);
	if (max_devnum < 0)
		return ERR_PTR(max_devnum);
	if (devnum > max_devnum)
		return ERR_PTR(-EINVAL);

	dev_desc = blk_get_devnum_by_type(if_type, devnum);
	if (IS_ERR_OR_NULL(dev_desc)) {
		debug("No device for iface '%s', dev %d\n",
		               blk_get_if_type_name(if_type), devnum);
		return ERR_PTR(-ENODEV);
	}

	return dev_desc;
}

int get_part_by_name(struct blk_desc *dev_desc,
                      const char *name,
                      struct disk_partition *part)
{
	int ret;

	if (IS_ERR_OR_NULL(part) ||
	    IS_ERR_OR_NULL(dev_desc))
		return -EINVAL;

	if (dev_desc->if_type == IF_TYPE_MTD)
		mtd_probe_devices();

	ret = part_get_info_by_name(dev_desc, name, part);
	if (ret < 0) {
		debug("Unable to find '%s' partition (if_type %u devnum %d)\n",
		              name, dev_desc->if_type, dev_desc->devnum);
		return ret;
	}

	return ret;
}

int set_bootdevice_env(int bootdev)
{
	static bool initialised = false;
	struct uclass *uc;
	struct udevice *dev;
	int ret;

	if (likely(initialised))
		return 0;

	switch (bootdev) {
	case BOOTDEVICE_ONLY_EMMC:
		ret = uclass_get(UCLASS_MMC, &uc);
		if (ret)
			return ret;

		/* iterate through available devices of MMC uclass */
		uclass_foreach_dev(dev, uc) {
			struct mmc *mmc = NULL;
			ret = device_probe(dev);
			if (ret)
				continue;

			mmc = mmc_get_mmc_dev(dev);
			if (mmc && !mmc_init(mmc) && IS_MMC(mmc)) {
				env_set_ulong("mmc_dev", dev_seq(dev));
				break;
			}
		}
		break;
	case BOOTDEVICE_ONLY_NAND:
		/* iterate through available devices of MTD uclass */
		ret = uclass_get(UCLASS_MTD, &uc);
		if (ret)
			return ret;
		uclass_foreach_dev(dev, uc) {
			struct mtd_info *mtd = NULL;
			ret = device_probe(dev);
			if (ret)
				continue;

			mtd = dev_get_uclass_priv(dev);

			if (mtd && (mtd->type == MTD_NANDFLASH ||
			       mtd->type == MTD_MLCNANDFLASH)) {
				env_set_ulong("main_mtd", dev_seq(dev));
				break;
			}
		}
		break;
	case BOOTDEVICE_BOTH_NOR_NAND:
		/* probe all SPI NOR Flash devices */
		uclass_foreach_dev_probe(UCLASS_SPI_FLASH, dev)
			;

		/* iterate through available devices of MTD uclass */
		ret = uclass_get(UCLASS_MTD, &uc);
		if (ret)
			return ret;
		uclass_foreach_dev(dev, uc) {
			struct mtd_info *mtd = NULL;
			ret = device_probe(dev);
			if (ret)
				continue;

			mtd = dev_get_uclass_priv(dev);

			if (mtd && (mtd->type == MTD_NANDFLASH ||
			       mtd->type == MTD_MLCNANDFLASH))
				env_set_ulong("main_mtd", dev_seq(dev));

			if (mtd && mtd->type == MTD_NORFLASH)
				env_set_ulong("safe_mtd", dev_seq(dev));
		}
		debug("main_mtd: 0x%lx safe_mtd: 0x%lx\n",
		           env_get_ulong("main_mtd", 10, ~0ULL),
		           env_get_ulong("safe_mtd", 10, ~0ULL));
		break;
	case BOOTDEVICE_BOTH_NOR_EMMC:
		ret = uclass_get(UCLASS_MMC, &uc);
		if (ret)
			return ret;

		uclass_foreach_dev(dev, uc) {
			struct mmc *mmc = NULL;
			ret = device_probe(dev);
			if (ret)
				continue;

			mmc = mmc_get_mmc_dev(dev);
			if (mmc && !mmc_init(mmc) && IS_MMC(mmc)) {
				env_set_ulong("mmc_dev", dev_seq(dev));
				break;
			}
		}

		/* probe all SPI NOR Flash devices */
		uclass_foreach_dev_probe(UCLASS_SPI_FLASH, dev)
			;

		/* iterate through available devices of MTD uclass */
		ret = uclass_get(UCLASS_MTD, &uc);
		if (ret)
			return ret;
		uclass_foreach_dev(dev, uc) {
			struct mtd_info *mtd = NULL;
			ret = device_probe(dev);
			if (ret)
				continue;

			mtd = dev_get_uclass_priv(dev);
			if (mtd && mtd->type == MTD_NORFLASH) {
				env_set_ulong("safe_mtd", dev_seq(dev));
				break;
			}
		}
		debug("safe mtd: 0x%lx\n",
		            env_get_ulong("safe_mtd", 10, ~0ULL));

		break;
	default:
		debug("Not supported boot device\n");
		return -EBUSY;
	}

	initialised = true;
	return 0;
}

int get_bootdevice(const char **name)
{
#if CONFIG_IS_ENABLED(DM_BOOT_DEVICE)
	struct udevice *dev;
	int bootdev;

	uclass_get_device_by_name(UCLASS_BOOT_DEVICE,
	                        "boot-device", &dev);
	if (dev)
		bootdev = dm_boot_device_get(dev, name ?: NULL);
	else
		return -ENODEV;

	return bootdev > 0 ? bootdev : -ENXIO;
#endif

	return -ENODEV;
}

int partitions_id_check(struct fdl_part_table *tab)
{
	int i, j;
	struct disk_partition *part;
	int len, len1;

	if (!tab)
		return -EINVAL;

	for (part = tab->part; part < tab->part + tab->number; part++) {
		for (i = 0; i < ARRAY_SIZE(safe_part_id); i++) {
			len = strlen((void *)part->name);
			len1 = strlen(safe_part_id[i]);
			if (len != len1 ||
			    strcmp((void *)part->name, safe_part_id[i]))
				continue;
			else
				break;
		}

		if (i != ARRAY_SIZE(safe_part_id))
			continue;

		for (j = 0; j < ARRAY_SIZE(main_part_id); j++) {
			len = strlen((void *)part->name);
			len1 = strlen(main_part_id[j]);
			if (len != len1 ||
			    strcmp((void *)part->name, main_part_id[j]))
				continue;
			else
				break;
		}

		if (j == ARRAY_SIZE(main_part_id))
			return -EINVAL;
	}

	return 0;
}