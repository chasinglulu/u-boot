// SPDX-License-Identifier: BSD-2-Clause
/*
 * Laguna FDL image update functions
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <part.h>
#include <spl.h>
#include <errno.h>
#include <blk.h>
#include <env.h>
#include <image.h>
#include <mtd.h>
#include <fdl.h>
#include <memalign.h>
#include <hexdump.h>

#include <asm/arch/bootparams.h>
#include <linux/mtd/mtdparts.h>
#include <linux/compiler.h>
#include <asm/u-boot.h>
#include <android_ab.h>


/* PARTITION_SYSTEM_GUID */
const unsigned char default_type_guid_bin[UUID_BIN_LEN] = {
	0x28, 0x73, 0x2A, 0xC1,
	0x1F, 0xF8, 0xD2, 0x11,
	0xBA, 0x4B, 0x00, 0xA0,
	0xC9, 0x3E, 0xC9, 0x3B
};

/* uuid -F BIN | xxd -i */
const unsigned char default_part_guid_bin[UUID_BIN_LEN] = {
	0x02, 0x4E, 0x69, 0x76,
	0xC2, 0x71, 0x11, 0xEF,
	0xB3, 0x4C, 0x9F, 0x1E,
	0x7A, 0xAB, 0xB0, 0xAF
};

const unsigned char default_disk_uuid_bin[UUID_BIN_LEN] = {
	0x75, 0xFF, 0x1A, 0x66,
	0xC2, 0x83, 0x11, 0xEF,
	0xB9, 0xF0, 0x5F, 0xFA,
	0x4B, 0x8C, 0x8E, 0xFD
};

static int
gpt_partitions_fill(struct blk_desc *dev_desc,
         struct disk_partition *partitions, int part_count)
{
	int i;
	struct disk_partition *part;
	lbaint_t offset = DIV_ROUND_UP_ULL(SZ_1M, dev_desc->blksz);

	part = partitions;
	for (i = 0; i < part_count; i++, part++) {
		part->start += offset;

#ifdef CONFIG_PARTITION_UUIDS
#if defined(CONFIG_RANDOM_UUID)
		gen_rand_uuid_str(part->uuid, UUID_STR_FORMAT_STD);
#else
		uuid_bin_to_str(default_part_guid_bin,
		      part->uuid, UUID_STR_FORMAT_STD);
#endif
#endif

#ifdef CONFIG_PARTITION_TYPE_GUID
		uuid_bin_to_str(default_type_guid_bin,
		      part->type_guid, UUID_STR_FORMAT_GUID);
#endif
	}

	return 0;
}

static int
gpt_partitions_verify(struct blk_desc *dev_desc,
             struct disk_partition *partitions, int part_count)
{
	ALLOC_CACHE_ALIGN_BUFFER_PAD(gpt_header, gpt_head, 1, dev_desc->blksz);
	gpt_entry *gpt_pte = NULL;
	int ret;

	/* Check partition layout with provided pattern */
	ret = gpt_verify_partitions(dev_desc, partitions, part_count,
	                gpt_head, &gpt_pte);
	if (!ret)
		free(gpt_pte);
	return ret;
}

static int
get_partition_count_by_id(struct fdl_part_table *tab,
                                char **part_id, int len)
{
	struct disk_partition *part;
	int i, j;
	int count = 0;
	int strlen1, strlen2;

	if (!tab || !part_id)
		return -EINVAL;

	part = tab->part;
	for (i = 0; i < tab->number; i++, part++) {
		for (j = 0; j < len; j++) {
			strlen1 = strlen((void *)part->name);
			strlen2 = strlen(part_id[j]);
			if (strlen1 != strlen2 ||
			    strcmp((void *)part->name, part_id[j])) {
				continue;
			} else {
				count++;
				break;
			}
		}
	}

	return count;
}

static struct fdl_part_table
*get_partitions_by_id(struct fdl_part_table *tab,
                            char **part_id, int len)
{
	struct fdl_part_table *pt;
	struct disk_partition *part;
	int strlen1, strlen2;
	int count, i, j;
	int idx = 0;

	if (!tab || !part_id)
		return ERR_PTR(-EINVAL);

	count = get_partition_count_by_id(tab, part_id, len);
	if (count <= 0) {
		pr_err("Invaild partition IDs\n");
		return ERR_PTR(count);
	}

	pt = malloc(sizeof(*pt) + count * sizeof(*part));
	if (!pt) {
		pr_err("Out of memory\n");
		return ERR_PTR(-ENOMEM);
	}
	pt->number = count;

	part = tab->part;
	for (i = 0; i < tab->number; i++, part++) {
		for (j = 0; j < len; j++) {
			strlen1 = strlen((void *)part->name);
			strlen2 = strlen(part_id[j]);
			if (strlen1 != strlen2 ||
			    strcmp((void *)part->name, part_id[j])) {
				continue;
			} else {
				memcpy(&pt->part[idx++], part, sizeof(*part));
				break;
			}
		}
	}

	return pt;
}

static lbaint_t
calculate_partitions_size(struct disk_partition *partitions,
                        int part_count)
{
	struct disk_partition *part;
	lbaint_t size = 0;

	part = &partitions[0];
	while (part_count--) {
		size += part->size;
		part++;
	}

	return size;
}

static void
fixup_mmc_partitions_start(struct disk_partition *partitions,
                            int part_count, lbaint_t offset)
{
	struct disk_partition *part;

	part = &partitions[0];
	while (part_count--) {
		part->start -= offset;
		part++;
	}
}

static void
align_nand_partitions(struct blk_desc *dev_desc, struct disk_partition *partitions, int part_count)
{
	struct disk_partition *part;
	struct mtd_info *mtd = blk_desc_to_mtd(dev_desc);
	uint64_t size;

	switch (mtd->type) {
	case MTD_NANDFLASH:
	case MTD_MLCNANDFLASH:
		debug("erasesize: 0x%x\n", mtd->erasesize);
		break;
	default:
		return;
	}

	part = partitions;
	while (part_count--) {
		size = part->size * SZ_512;
		size = ALIGN(size , mtd->erasesize);
		part->size = DIV_ROUND_UP(size, dev_desc->blksz);
		part++;
	}
}

int fdl_blk_write_data(const char *part_name, size_t image_size)
{
	struct blk_desc *blk_desc = NULL;
	struct blk_desc *blk_desc1 = NULL;
	struct disk_partition pi;
	struct blk_desc *dev;
	ulong blkcnt, written;
	ulong main_mtd;
	ulong safe_mtd;
	ulong mmc_dev;
	int bootdev;
	int ret;

	bootdev = get_bootdevice(NULL);
	if (bootdev < 0) {
		debug("Invaild boot device\n");
		return -EINVAL;
	}
	debug("%s: bootdevice: %d\n", __func__, bootdev);
	set_bootdevice_env(bootdev);

	switch (bootdev) {
	case BOOTDEVICE_ONLY_EMMC:
		mmc_dev = env_get_ulong("mmc_dev", 10, ~0UL);
		if (unlikely(mmc_dev == ~0UL))
			mmc_dev = CONFIG_FDL_FLASH_MMC_DEV;

		blk_desc = get_blk_by_devnum(IF_TYPE_MMC, mmc_dev);
		if (IS_ERR_OR_NULL(blk_desc))
			return -ENXIO;
		break;
	case BOOTDEVICE_ONLY_NAND:
		main_mtd = env_get_ulong("main_mtd", 10, ~0UL);
		if (unlikely(main_mtd == ~0UL))
			main_mtd = CONFIG_FDL_FLASH_NAND_MTD_DEV;

		blk_desc = get_blk_by_devnum(IF_TYPE_MTD,
		              main_mtd);
		if (IS_ERR_OR_NULL(blk_desc))
			return -ENXIO;
		break;
	case BOOTDEVICE_BOTH_NOR_NAND:
		safe_mtd = env_get_ulong("safe_mtd", 10, ~0UL);
		if (unlikely(safe_mtd == ~0UL))
			safe_mtd = CONFIG_FDL_FLASH_NOR_MTD_DEV;
		main_mtd = env_get_ulong("main_mtd", 10, ~0UL);
		if (unlikely(main_mtd == ~0UL))
			main_mtd = CONFIG_FDL_FLASH_NAND_MTD_DEV;

		blk_desc = get_blk_by_devnum(IF_TYPE_MTD,
		               CONFIG_FDL_FLASH_NOR_MTD_DEV);
		blk_desc1 = get_blk_by_devnum(IF_TYPE_MTD,
		               CONFIG_FDL_FLASH_NAND_MTD_DEV);
		if (IS_ERR_OR_NULL(blk_desc) ||
		     IS_ERR_OR_NULL(blk_desc1))
			return -ENXIO;
		break;
	case BOOTDEVICE_BOTH_NOR_EMMC:
		blk_desc = get_blk_by_devnum(IF_TYPE_MTD,
		                CONFIG_FDL_FLASH_NOR_MTD_DEV);
		blk_desc1 = get_blk_by_devnum(IF_TYPE_MMC,
		                    CONFIG_FDL_FLASH_MMC_DEV);
		if (IS_ERR_OR_NULL(blk_desc) ||
		     IS_ERR_OR_NULL(blk_desc1))
			return -ENXIO;
		break;
	default:
		debug("Not supported boot device\n");
		return -EBUSY;
	}

	ret = get_part_by_name(blk_desc1, part_name, &pi);
	if (ret > 0) {
		dev = blk_desc1;
		goto writing;
	}

	ret = get_part_by_name(blk_desc, part_name, &pi);
	if (ret < 0) {
		debug("Not found '%s' partition\n", part_name);
		return -EIO;
	}
	dev = blk_desc;

writing:
	blkcnt = DIV_ROUND_UP_ULL(image_size, dev->blksz);
	written = blk_dwrite(dev, pi.start,
	                             blkcnt, fdl_buf_addr);
	if (written != blkcnt) {
		debug("Size of written image not equal to expected size (%lu != %lu)\n",
		                written * dev->blksz, image_size);
		return -EIO;
	}
	printf("\nwritting of %lu bytes finished\n", image_size);

	return 0;
}

int fdl_blk_write_partition(struct fdl_part_table *ptab)
{
	struct blk_desc *mmc_desc = NULL;
	struct blk_desc *mtd_desc = NULL;
	struct disk_partition *mmc_partitions = NULL;
	struct disk_partition *mtd_partitions = NULL;
	struct fdl_part_table *mmc_tab = NULL;
	struct fdl_part_table *mtd_tab = NULL;
	int mmc_part_count, mtd_part_count;
	char *str_disk_guid = NULL;
	char bootmode[32];
	char *prefix = NULL;
	lbaint_t mtdparts_size;
	bool gpt = false;
	bool mtdparts = false;
	ulong main_mtd;
	ulong safe_mtd;
	ulong mmc_dev;
	int bootdev;
	int ret;

	ret = partitions_id_check(ptab);
	if (ret < 0) {
		pr_err("invaild partitons id\n");
		return ret;
	}

	str_disk_guid = malloc(UUID_STR_LEN + 1);
	if (str_disk_guid == NULL)
		return -ENOMEM;
#ifdef CONFIG_RANDOM_UUID
	gen_rand_uuid_str(str_disk_guid, UUID_STR_FORMAT_STD);
#else
	uuid_bin_to_str(default_disk_uuid_bin,
	     str_disk_guid, UUID_STR_FORMAT_STD);
#endif

	bootdev = get_bootdevice(NULL);
	if (bootdev < 0) {
		debug("Invaild boot device\n");
		return -EINVAL;
	}
	debug("%s: bootdevice: %d\n", __func__, bootdev);
	set_bootdevice_env(bootdev);

	switch (bootdev) {
	case BOOTDEVICE_ONLY_EMMC:
		mmc_dev = env_get_ulong("mmc_dev", 10, ~0UL);
		if (unlikely(mmc_dev == ~0UL))
			mmc_dev = CONFIG_FDL_FLASH_MMC_DEV;

		mmc_desc = get_blk_by_devnum(IF_TYPE_MMC, mmc_dev);
		if (IS_ERR_OR_NULL(mmc_desc))
			return -ENXIO;

		gpt = true;
		mmc_partitions = ptab->part;
		mmc_part_count = ptab->number;
		break;
	case BOOTDEVICE_ONLY_NAND:
		/* mtdparts string would written into main SPI nand flash */
		main_mtd = env_get_ulong("main_mtd", 10, ~0UL);
		if (unlikely(main_mtd == ~0UL))
			main_mtd = CONFIG_FDL_FLASH_NAND_MTD_DEV;

		mtd_desc = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(mtd_desc))
			return -ENXIO;

		mtdparts = true;
		mtd_partitions = ptab->part;
		mtd_part_count = ptab->number;
		align_nand_partitions(mtd_desc,
		    mtd_partitions, mtd_part_count);
		break;
	case BOOTDEVICE_BOTH_NOR_NAND:
		/* Align & Fixup Main Nand partitions */
		main_mtd = env_get_ulong("main_mtd", 10, ~0UL);
		if (unlikely(main_mtd == ~0UL))
			main_mtd = CONFIG_FDL_FLASH_NAND_MTD_DEV;

		mtd_desc = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(mtd_desc))
			return -ENXIO;

		mtd_tab = get_partitions_by_id(ptab, (void *)main_part_id,
		                        get_main_part_id_count());
		if (IS_ERR_OR_NULL(mtd_tab))
			return -EINVAL;
		align_nand_partitions(mtd_desc,
		       mtd_tab->part, mtd_tab->number);

		/* mtdparts string would written into safety SPI nor flash */
		safe_mtd = env_get_ulong("safe_mtd", 10, ~0UL);
		if (unlikely(safe_mtd == ~0UL))
			safe_mtd = CONFIG_FDL_FLASH_NOR_MTD_DEV;

		mtd_desc = get_blk_by_devnum(IF_TYPE_MTD, safe_mtd);
		if (IS_ERR_OR_NULL(mtd_desc))
			return -ENXIO;

		mtdparts = true;
		mtd_partitions = ptab->part;
		mtd_part_count = ptab->number;
		snprintf(bootmode, sizeof(bootmode), "nand*%d#", BOOTMODE_MAIN_NAND);
		prefix = bootmode;
		break;
	case BOOTDEVICE_BOTH_NOR_EMMC:
		mmc_dev = env_get_ulong("mmc_dev", 10, ~0UL);
		if (unlikely(mmc_dev == ~0UL))
			mmc_dev = CONFIG_FDL_FLASH_MMC_DEV;
		safe_mtd = env_get_ulong("safe_mtd", 10, ~0UL);
		if (unlikely(safe_mtd == ~0UL))
			safe_mtd = CONFIG_FDL_FLASH_NOR_MTD_DEV;

		mmc_desc = get_blk_by_devnum(IF_TYPE_MMC, mmc_dev);
		mtd_desc = get_blk_by_devnum(IF_TYPE_MTD, safe_mtd);
		if (IS_ERR_OR_NULL(mmc_desc) ||
		     IS_ERR_OR_NULL(mtd_desc))
			return -ENXIO;

		mtd_tab = get_partitions_by_id(ptab, (void *)safe_part_id,
		                        get_safe_part_id_count());
		mmc_tab = get_partitions_by_id(ptab, (void *)main_part_id,
		                        get_main_part_id_count());
		if (IS_ERR_OR_NULL(mtd_tab) ||
		     IS_ERR_OR_NULL(mmc_tab))
			return -EINVAL;
		mmc_partitions = mmc_tab->part;
		mmc_part_count = mmc_tab->number;
		mtd_partitions = mtd_tab->part;
		mtd_part_count = mtd_tab->number;
		mtdparts_size = calculate_partitions_size(mtd_partitions,
		                      mtd_part_count);
		fixup_mmc_partitions_start(mmc_partitions,
		         mmc_part_count, mtdparts_size);
		gpt = mtdparts = true;
		snprintf(bootmode, sizeof(bootmode), "emmc*%d#", BOOTMODE_MAIN_EMMC);
		prefix = bootmode;
		break;
	default:
		debug("Not supported boot device\n");
		return -EBUSY;
	}

	if (gpt) {
		gpt_partitions_fill(mmc_desc,
		    mmc_partitions, mmc_part_count);

		if (gpt_partitions_verify(mmc_desc,
		    mmc_partitions, mmc_part_count)) {
			debug("GPT verification fail.\n");
			ret = gpt_restore(mmc_desc, str_disk_guid,
			                 mmc_partitions,
			                 mmc_part_count);
			if (ret < 0) {
				debug("Unable to write GPT\n");
				return ret;
			}
			/* Invalid part_type filled in the previous part_init */
			mmc_desc->part_type = PART_TYPE_UNKNOWN;
		}
	}

	if (mtdparts){
		ret = mtdparts_restore(mtd_desc, str_disk_guid,
		               prefix,
		               mtd_partitions,
		               mtd_part_count);
		if (ret < 0) {
			debug("Unable to write mtdparts\n");
			return ret;
		}
	}

	if (mmc_tab)
		free(mmc_tab);

	if (mtd_tab)
		free(mtd_tab);

	if (str_disk_guid)
		free(str_disk_guid);

	return 0;
}

int fdl_blk_erase(const char *part_name, size_t size)
{
	struct blk_desc *mmc_desc = NULL;
	struct blk_desc *mtd0_desc = NULL;
	struct blk_desc *mtd1_desc = NULL;
	bool eraseall = false;
	ulong main_mtd;
	ulong safe_mtd;
	ulong mmc_dev;
	int bootdev;
	int ret;

	if (!part_name && size == ~0ULL)
		eraseall = true;

	bootdev = get_bootdevice(NULL);
	if (bootdev < 0) {
		debug("Invaild boot device\n");
		return -EINVAL;
	}
	debug("%s: bootdevice: %d\n", __func__, bootdev);
	set_bootdevice_env(bootdev);

	switch (bootdev) {
	case BOOTDEVICE_ONLY_EMMC:
		mmc_dev = env_get_ulong("mmc_dev", 10, ~0UL);
		if (unlikely(mmc_dev == ~0UL))
			mmc_dev = CONFIG_FDL_FLASH_MMC_DEV;

		mmc_desc = get_blk_by_devnum(IF_TYPE_MMC, mmc_dev);
		if (IS_ERR_OR_NULL(mmc_desc))
			return -ENXIO;
		break;
	case BOOTDEVICE_ONLY_NAND:
		main_mtd = env_get_ulong("main_mtd", 10, ~0UL);
		if (unlikely(main_mtd == ~0UL))
			main_mtd = CONFIG_FDL_FLASH_NAND_MTD_DEV;

		mtd0_desc = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(mtd0_desc))
			return -ENXIO;
		break;
	case BOOTDEVICE_BOTH_NOR_NAND:
		safe_mtd = env_get_ulong("safe_mtd", 10, ~0UL);
		if (unlikely(safe_mtd == ~0UL))
			safe_mtd = CONFIG_FDL_FLASH_NOR_MTD_DEV;
		main_mtd = env_get_ulong("main_mtd", 10, ~0UL);
		if (unlikely(main_mtd == ~0UL))
			main_mtd = CONFIG_FDL_FLASH_NAND_MTD_DEV;

		mtd0_desc = get_blk_by_devnum(IF_TYPE_MTD, safe_mtd);
		mtd1_desc = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(mtd0_desc) ||
		     IS_ERR_OR_NULL(mtd1_desc))
			return -ENXIO;
		break;
	case BOOTDEVICE_BOTH_NOR_EMMC:
		mmc_dev = env_get_ulong("mmc_dev", 10, ~0UL);
		if (unlikely(mmc_dev == ~0UL))
			mmc_dev = CONFIG_FDL_FLASH_MMC_DEV;
		safe_mtd = env_get_ulong("safe_mtd", 10, ~0UL);
		if (unlikely(safe_mtd == ~0UL))
			safe_mtd = CONFIG_FDL_FLASH_NOR_MTD_DEV;

		mmc_desc = get_blk_by_devnum(IF_TYPE_MMC, mmc_dev);
		mtd0_desc = get_blk_by_devnum(IF_TYPE_MTD, safe_mtd);
		if (IS_ERR_OR_NULL(mmc_desc) ||
		     IS_ERR_OR_NULL(mtd0_desc))
			return -ENXIO;
		break;
	default:
		debug("Not supported boot device\n");
		return -EBUSY;
	}

	if (eraseall) {
		if (mmc_desc) {
			printf("mmc lba: 0x%lx\n", mmc_desc->lba);
			ret = blk_derase(mmc_desc, 0, mmc_desc->lba);
		}
		if (mtd0_desc)
			ret = blk_derase(mtd0_desc, 0, mtd0_desc->lba);
		if (mtd1_desc)
			ret = blk_derase(mtd1_desc, 0, mtd1_desc->lba);

		if (ret < 0) {
			printf("Failed to erase the whole device (ret = %d)\n", ret);
			return ret;
		}
		printf("%s\n", __func__);
	} else {
		printf("TODO\n");
	}

	return 0;
}