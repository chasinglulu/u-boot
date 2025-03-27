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
#include <sysinfo.h>

#include <linux/err.h>
#include <dm/device-internal.h>
#include <bootdev/bootdevice.h>
#include <asm/arch/bootparams.h>
#include <linux/mtd/mtdparts.h>

/* Supported partition identifier for safety R5F */
const char *safe_part_id[] = {
	"sbl",
	"metadata",
	"spl",
	"rtos",
	"metadata_bak",
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

#if CONFIG_IS_ENABLED(BOOT_DEVICE_SYSCON)
const char *main_bootmodes[] = {
	[BOOTMODE_MAIN_NOR]     = "NOR",
	[BOOTMODE_MAIN_NAND]    = "NAND",
	[BOOTMODE_MAIN_HYPER]   = "HYPER",
	[BOOTMODE_MAIN_EMMC]    = "EMMC",
};

static int get_main_bootmode_by_name(const char *name)
{
	int i;

	for (i = 0; i < BOOTMODE_MAIN_COUNT; i++) {
		if (!strcasecmp(main_bootmodes[i], name))
			return i;
	}

	return -EINVAL;
}

static void set_main_bootmode_env(struct disk_partition *p)
{
	int idx = -1;

	if (!p->size && !p->name[0]) {
		pr_err("Invaild part information\n");
		return;
	}

	if (!p->name[0])
		idx = p->size;

	if (!p->size)
		idx = get_main_bootmode_by_name((void *)p->name);

	if (idx >= BOOTMODE_MAIN_NOR &&
	        idx < BOOTMODE_MAIN_COUNT)
		env_set_ulong("main_bootmode", idx);
}
#else
static inline void set_main_bootmode_env(struct disk_partition *p) {}
#endif

int partitions_id_check(struct fdl_part_table *tab)
{
	struct disk_partition *start, *end;
	struct fdl_part_table *p = NULL;
	int len, len1;
	int i, j;
	int ret = 0;

	if (!tab)
		return -EINVAL;

	p = calloc(tab->number, sizeof(*start));
	if (!p)
		return -ENOMEM;

	end = tab->part + tab->number;
	for (start = tab->part; start < end; start++) {
		if (!start->size || !start->name[0]) {
			set_main_bootmode_env(start);
			continue;
		}

		j = 0;
		len1 = strlen((void *)start->name);
		while (j < p->number) {
			len = strlen((void *)p->part[j].name);
			if (len == len1 &&
			    !strncmp((void *)p->part[j].name,
			                     (void *)start->name, len)) {
				debug("Found the same partition name (%s)\n", start->name);
				ret = -EINVAL;
				goto failed;
			}
			j++;
		}
		p->part[p->number++] = *start;
	}

	end = p->part + p->number;
	for (start = p->part; start < end; start++) {
		for (i = 0; i < ARRAY_SIZE(safe_part_id); i++) {
			len = strlen((void *)start->name);
			len1 = strlen(safe_part_id[i]);
			if (len != len1 ||
			    strcmp((void *)start->name, safe_part_id[i]))
				continue;
			else
				break;
		}

		if (i != ARRAY_SIZE(safe_part_id))
			continue;

		for (j = 0; j < ARRAY_SIZE(main_part_id); j++) {
			len = strlen((void *)start->name);
			len1 = strlen(main_part_id[j]);
			if (len != len1 ||
			    strcmp((void *)start->name, main_part_id[j]))
				continue;
			else
				break;
		}

		if (j == ARRAY_SIZE(main_part_id)) {
			debug("Not allowed partition name (%s)\n", start->name);
			ret = -EINVAL;
			goto failed;
		}
	}

	tab->number = p->number;
	memcpy(tab->part, p->part, p->number * sizeof(*start));

failed:
	if (p)
		free(p);
	if (ret < 0)
		return ret;

	return 0;
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
	static bool bootdev_inited= false;
	struct uclass *uc;
	struct udevice *dev;
	int ret;

	if (likely(bootdev_inited))
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
		env_set("devtype", "mmc");
		env_set_ulong("devnum", dev_seq(dev));
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
		env_set("devtype", "mtd");
		env_set_ulong("devnum", dev_seq(dev));
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
			       mtd->type == MTD_MLCNANDFLASH)) {
				env_set_ulong("main_mtd", dev_seq(dev));
				env_set_ulong("devnum", dev_seq(dev));
			}

			if (mtd && mtd->type == MTD_NORFLASH)
				env_set_ulong("safe_mtd", dev_seq(dev));
		}
		env_set("devtype", "mtd");
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
		env_set("devtype", "mmc");
		env_set_ulong("devnum", dev_seq(dev));

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

	bootdev_inited = true;
	return 0;
}

#if CONFIG_IS_ENABLED(BOOT_DEVICE_SYSCON)
const char *bootdevice_name[] = {
	[BOOTDEVICE_ONLY_EMMC]       = "Only-eMMC",
	[BOOTDEVICE_ONLY_NOR]        = "Only-Nor",
	[BOOTDEVICE_ONLY_NAND]       = "Only-Nand",
	[BOOTDEVICE_ONLY_HYPER]      = "Only-Hyper",
	[BOOTDEVICE_BOTH_NOR_NOR]    = "Both-Nor-Nor",
	[BOOTDEVICE_BOTH_NOR_NAND]   = "Both-Nor-Nand",
	[BOOTDEVICE_BOTH_NOR_HYPER]  = "Both-Nor-Hyper",
	[BOOTDEVICE_BOTH_NAND_NOR]   = "Both-Nand-Nor",
	[BOOTDEVICE_BOTH_NAND_NAND]  = "Both-Nand-Nand",
	[BOOTDEVICE_BOTH_NAND_HYPER] = "Both-Nand-Hyper",
	[BOOTDEVICE_BOTH_NOR_EMMC]   = "Both-Nor-eMMC",
	[BOOTDEVICE_BOTH_NAND_EMMC]  = "Both-Nand-eMMC",
};

static struct blk_desc *find_safety_nor(void)
{
	struct uclass *uc = NULL;
	struct mtd_info *mtd = NULL;
	struct blk_desc *dev_desc = NULL;
	struct udevice *dev, *parent, *child;
	fdt_addr_t addr;
	int ret;

	ret = uclass_get(UCLASS_SPI_FLASH, &uc);
	if (ret < 0)
		return NULL;

	uclass_foreach_dev(dev, uc) {
		parent = dev_get_parent(dev);
		addr = dev_read_addr(parent);
		if (addr > 0x800000)
			continue;

		ret = device_probe(dev);
		if (ret < 0)
			continue;

		device_find_first_child(dev, &child);
		if (!child)
			continue;
		mtd = dev_get_uclass_priv(child);
		if (mtd && mtd->type == MTD_NORFLASH) {
			dev_desc = get_blk_by_devnum(IF_TYPE_MTD, dev_seq(child));
			break;
		}
	}

	return IS_ERR_OR_NULL(dev_desc) ? NULL : dev_desc;
}

static struct blk_desc *find_safety_nand(void)
{
	/* FIXME: TODO */

	return NULL;
}

static int get_bootstrap(const char **name)
{
	const char *dev_name = "syscon-bootdev";
	struct udevice *dev;
	int bootstrap;

	uclass_get_device_by_name(UCLASS_BOOT_DEVICE,
	                        dev_name, &dev);
	if (dev)
		bootstrap = boot_device_get(dev, name ?: NULL);
	else
		return -ENODEV;

	return bootstrap >= 0 ? bootstrap : -ENXIO;
}
#endif /* BOOT_DEVICE_SYSCON */

int get_bootstate(void)
{
#if CONFIG_IS_ENABLED(BOOT_DEVICE_SYSCON)
	int bootstrap;

	bootstrap = get_bootstrap(NULL);
	if (bootstrap < 0)
		return bootstrap;

	if (bootstrap & BIT(4))
		return BOOTSTATE_DOWNLOAD;

	return BOOTSTATE_POWERUP;
#endif

	return -EINVAL;
}

static __maybe_unused
void set_bootstate_env(uint32_t bootstate)
{
	static bool bootstate_inited= false;

#ifndef CONFIG_SPL_BUILD
	if (!(gd->flags & GD_FLG_RELOC))
		return;
#endif

	if (likely(bootstate_inited))
		return;

	env_set_ulong("bootstate", bootstate);
	bootstate_inited = true;
}

int get_bootdevice(const char **name)
{
#if CONFIG_IS_ENABLED(BOOT_DEVICE_GPIO)
	const char *dev_name = "gpio-bootdev";
	struct udevice *dev;
	int bootdev;

	uclass_get_device_by_name(UCLASS_BOOT_DEVICE,
	                        dev_name, &dev);
	if (dev)
		bootdev = boot_device_get(dev, name ?: NULL);
	else
		return -ENODEV;

	return bootdev > 0 ? bootdev : -ENXIO;
#endif

#if CONFIG_IS_ENABLED(BOOT_DEVICE_SYSCON)
	struct blk_desc *dev_desc;
	char *full_mtdparts = NULL;
	char *asterisk = NULL;
	int bootstate, bootstrap;
	ulong main_bootmode;
	bool is_nor = false;
	uint32_t bootdev;
	int ret;

	bootstate = get_bootstate();
	if (bootstate < 0)
		return -ENXIO;
	set_bootstate_env(bootstate);

	bootstrap = get_bootstrap(NULL);
	if (bootstrap < 0)
		return -ENXIO;

	debug("%s: bootstrap: %d\n", __func__, bootstrap);
	switch (bootstrap & 0xF) {
	case BOOTSTRAP_SAFETY_NOR:
		is_nor = true;
		goto main_select;
	case BOOTSTRAP_SAFETY_NAND_2KMP:
	case BOOTSTRAP_SAFETY_NAND_4KMP:
	case BOOTSTRAP_SAFETY_NAND_2K:
	case BOOTSTRAP_SAFETY_NAND_4K:
		is_nor = false;
		goto main_select;
	case BOOTSTRAP_MAIN_EMMC_DUAL_BOOT:
	case BOOTSTRAP_MAIN_EMMC_BOOT:
	case BOOTSTRAP_MAIN_EMMC:
		bootdev = BOOTDEVICE_ONLY_EMMC;
		break;
	case BOOTSTRAP_MAIN_NOR:
		bootdev = BOOTDEVICE_ONLY_NOR;
		break;
	case BOOTSTRAP_MAIN_NAND_2KMP:
	case BOOTSTRAP_MAIN_NAND_4KMP:
	case BOOTSTRAP_MAIN_NAND_2K:
	case BOOTSTRAP_MAIN_NAND_4K:
		bootdev = BOOTDEVICE_ONLY_NAND;
		break;
	case BOOTSTRAP_MAIN_HYPER:
		bootdev = BOOTDEVICE_ONLY_HYPER;
		break;
	default:
		debug("Not supported bootstrap value\n");
		return -ENODEV;
	}

	goto result;

main_select:
	if (bootstate == BOOTSTATE_DOWNLOAD) {
		/* FDL download */
		main_bootmode = env_get_ulong("main_bootmode", 10, ~0ULL);
		if (unlikely(main_bootmode == ~0ULL))
			return -EINVAL;
	} else {
		/* Normal boot */
		if (is_nor)
			dev_desc = find_safety_nor();
		else
			dev_desc = find_safety_nand();
		ret = read_full_mtdparts(dev_desc, &full_mtdparts);
		if (ret < 0)
			return ret;
		asterisk = strchr(full_mtdparts, '*');
		main_bootmode = simple_strtoul(asterisk + 1, NULL, 10);
		free(full_mtdparts);
	}

	debug("main bootmode: %lu\n", main_bootmode);
	switch (main_bootmode) {
	case BOOTMODE_MAIN_EMMC:
		if (is_nor)
			bootdev = BOOTDEVICE_BOTH_NOR_EMMC;
		else
			bootdev = BOOTDEVICE_BOTH_NAND_EMMC;
		break;
	case BOOTMODE_MAIN_NAND:
		if (is_nor)
			bootdev = BOOTDEVICE_BOTH_NOR_NAND;
		else
			bootdev = BOOTDEVICE_BOTH_NAND_NAND;
		break;
	case BOOTMODE_MAIN_HYPER:
		if (is_nor)
			bootdev = BOOTDEVICE_BOTH_NOR_HYPER;
		else
			bootdev = BOOTDEVICE_BOTH_NAND_HYPER;
		break;
	default:
		debug("Not supported main bootmode\n");
		return -ENODEV;
	}

result:
	if (name)
		*name = bootdevice_name[bootdev];
	return bootdev;
#endif

	return -ENODEV;
}

int get_downif(void)
{
	const char *dev_name = "download_if";
	struct udevice *dev;
	int downif, ret;

	uclass_get_device_by_name(UCLASS_SYSINFO,
	                        dev_name, &dev);
	if (IS_ERR_OR_NULL(dev)) {
		pr_err("Not found '%s' node\n", dev_name);
		return -ENODEV;
	}

	ret = sysinfo_detect(dev);
	if (ret < 0) {
		pr_err("Unable to detect download interface information.\n");
		return ret;
	}

	ret = sysinfo_get_int(dev, 0, &downif);
	if (ret < 0) {
		pr_err("Failed to read 'downif'\n");
		return ret;
	}

	return downif;
}

bool is_secure_boot(void)
{
#if defined(CONFIG_LUA_SECURE_BOOT)
	return true;
#endif

	return false;
}

void set_secureboot_env(bool secure)
{
	env_set_ulong("secureboot", secure ? 1 : 0);
}