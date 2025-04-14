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
const unsigned char default_system_guid_bin[UUID_BIN_LEN] = {
	0x28, 0x73, 0x2A, 0xC1,
	0x1F, 0xF8, 0xD2, 0x11,
	0xBA, 0x4B, 0x00, 0xA0,
	0xC9, 0x3E, 0xC9, 0x3B
};

/* PARTITION_BASIC_DATA_GUID */
const unsigned char default_data_guid_bin[UUID_BIN_LEN] = {
	0xA2, 0xA0, 0xD0, 0xEB,
	0xE5, 0xB9, 0x33, 0x44,
	0x87, 0xC0, 0x68, 0xB6,
	0xB7, 0x26, 0x99, 0xC7
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

		if (!strncasecmp((void *)part->name, "kernel", 6))
			part->bootable = PART_BOOTABLE;
		else
			part->bootable = 0;

#ifdef CONFIG_PARTITION_TYPE_GUID
		uuid_bin_to_str(default_data_guid_bin,
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
mtdparts_partitions_fixup(struct blk_desc *dev_desc,
         struct disk_partition *partitions, int part_count)
{
	struct disk_partition *part;
	int i;

	for (i = 0, part = partitions; i < part_count; i++, part++) {
		if (!strncasecmp((void *)part->name, "kernel", 6))
			part->bootable = PART_BOOTABLE;
		else
			part->bootable = 0;
	}
	return 0;
}

static int
get_partition_count_by_id(struct fdl_part_table *tab,
                                char **part_id, int len)
{
	struct disk_partition *part;
	int i, count = 0;

	if (!tab || !part_id)
		return -EINVAL;

	part = tab->part;
	for (part = tab->part; part < tab->part + tab->number; part++) {
		for (i = 0; i < len; i++) {
			if (strcmp((void *)part->name, part_id[i]))
				continue;

			count++;
			break;
		}
	}

	return count;
}

static struct fdl_part_table
*get_partitions_by_id(struct fdl_part_table *tab,
                            char **part_id, int len)
{
	struct fdl_part_table *new_tab;
	struct disk_partition *part;
	int count, i;
	int idx = 0;

	if (!tab || !part_id)
		return ERR_PTR(-EINVAL);

	count = get_partition_count_by_id(tab, part_id, len);
	if (count <= 0) {
		pr_err("Invaild partition IDs\n");
		return ERR_PTR(count);
	}

	new_tab = malloc(sizeof(*new_tab) + count * sizeof(*part));
	if (!new_tab) {
		pr_err("Out of memory\n");
		return ERR_PTR(-ENOMEM);
	}
	new_tab->number = count;

	for (part = tab->part; part < tab->part + tab->number; part++) {
		for (i = 0; i < len; i++) {
			if (strcmp((void *)part->name, part_id[i]))
				continue;

			memcpy(&new_tab->part[idx++], part, sizeof(*part));
			break;
		}
	}

	return new_tab;
}

static void update_partitions_by_tab(struct fdl_part_table *orig_tab,
                                     struct fdl_part_table *new_tab)
{
	struct disk_partition *p1, *p2;
	uint32_t part_cnt, i = 0;

	if (!orig_tab || !new_tab)
		return;

	part_cnt = min(orig_tab->number, new_tab->number);

	for (p1 = orig_tab->part; p1 < orig_tab->part + orig_tab->number; p1++) {
		for (p2 = new_tab->part; p2 < new_tab->part + new_tab->number; p2++) {
			if (!strcmp((void *)p1->name, (void *)p2->name)) {
				memcpy(p1, p2, sizeof(*p1));
				i++;
			}
		}
		if (i == part_cnt)
			break;
	}

	if (unlikely(i != part_cnt))
		pr_warn("New partition tab is not a subset of original partition tab\n");
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
		debug("MTD Nand erasesize: 0x%x\n", mtd->erasesize);
		break;
	default:
		return;
	}

	part = partitions;
	while (part_count--) {
		debug("%s: Partition '%s': size: 0x%lx, Device '%s': blksz: 0x%lx\n", __func__,
		        part->name, part->size, dev_desc->bdev->name, dev_desc->blksz);
		size = ALIGN(part->size * SZ_512 , mtd->erasesize);
		part->size = DIV_ROUND_UP(size, SZ_512);
		part++;
	}
}

static int
fdl_blk_get_dev_and_part(const char *part_name,
                         struct blk_desc **dev_desc,
                         struct disk_partition *part)
{
	struct blk_desc *blk_desc = NULL;
	struct blk_desc *blk_desc1 = NULL;
	struct blk_desc *found_dev = NULL;
	struct disk_partition part_info;
	ulong main_mtd, safe_mtd, mmc_dev;
	int bootdev, ret;

	if (!part_name || !dev_desc || !part)
		return -EINVAL;

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

		blk_desc = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
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

		blk_desc = get_blk_by_devnum(IF_TYPE_MTD, safe_mtd);
		blk_desc1 = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(blk_desc) ||
		    IS_ERR_OR_NULL(blk_desc1))
			return -ENXIO;
		break;
	case BOOTDEVICE_BOTH_NOR_EMMC:
		mmc_dev = env_get_ulong("mmc_dev", 10, ~0UL);
		if (unlikely(mmc_dev == ~0UL))
			mmc_dev = CONFIG_FDL_FLASH_MMC_DEV;
		safe_mtd = env_get_ulong("safe_mtd", 10, ~0UL);
		if (unlikely(safe_mtd == ~0UL))
			safe_mtd = CONFIG_FDL_FLASH_NOR_MTD_DEV;

		blk_desc = get_blk_by_devnum(IF_TYPE_MTD, safe_mtd);
		blk_desc1 = get_blk_by_devnum(IF_TYPE_MMC, mmc_dev);
		if (IS_ERR_OR_NULL(blk_desc) ||
		    IS_ERR_OR_NULL(blk_desc1))
			return -ENXIO;
		break;
	default:
		debug("Not supported boot device\n");
		return -EBUSY;
	}

	ret = get_part_by_name(blk_desc1, part_name, &part_info);
	if (ret > 0) {
		found_dev = blk_desc1;
		goto found;
	}

	ret = get_part_by_name(blk_desc, part_name, &part_info);
	if (ret < 0) {
		debug("Not found '%s' partition\n", part_name);
		return -ENODEV;
	}
	found_dev = blk_desc;

found:
	*dev_desc = found_dev;
	memcpy(part, &part_info, sizeof(*part));
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
	char *str_disk_guid = NULL, *prefix = NULL;
	ulong main_mtd, safe_mtd, mmc_dev;
	char bootmode[32];
	lbaint_t mtdparts_size;
	bool gpt = false;
	bool mtdparts = false;
	int ret, bootdev;

	ret = partitions_id_check(ptab);
	if (ret < 0) {
		pr_err("Invaild partitons ID\n");
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
		update_partitions_by_tab(ptab, mtd_tab);

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
		mtdparts_partitions_fixup(mtd_desc,
		    mtd_partitions, mtd_part_count);
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
	struct blk_desc *blk_desc = NULL;
	ulong main_mtd;
	int bootdev, ret;
	struct udevice *dev;
	struct uclass *uc;
	struct mtd_info *mtd;

	if (likely(!part_name) && likely(size == ~0ULL))
		goto eraseall;

	debug("Erasing partition: '%s' size: 0x%lx\n", part_name, size);
	/* FIXME: TODO */
	printf("TODO: Not Implemented yet\n");

	return 0;

eraseall:
	bootdev = get_bootdevice(NULL);
	if (bootdev < 0) {
		if (env_get_yesno("need_main_bootmode") == 1)
			goto erase_both_dev;
		debug("Invaild boot device\n");
		return -EINVAL;
	}
	debug("%s: bootdevice: %d\n", __func__, bootdev);
	set_bootdevice_env(bootdev);

	switch (bootdev) {
	case BOOTDEVICE_ONLY_NAND:
		main_mtd = env_get_ulong(env_get_name(ENV_MAIN_MTD), 10, ~0UL);
		if (unlikely(main_mtd == ~0UL))
			main_mtd = CONFIG_FDL_FLASH_NAND_MTD_DEV;

		blk_desc = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(blk_desc))
			return -ENXIO;
		break;
	case BOOTDEVICE_ONLY_EMMC:
		debug("Not need erasing the whole eMMC\n");
		return 0;
	case BOOTDEVICE_BOTH_NOR_NAND:
	case BOOTDEVICE_BOTH_NOR_EMMC:
		goto erase_both_dev;
	default:
		debug("Not supported boot device\n");
		return -EBUSY;
	}

	debug("Erasing single device '%s'...\n", blk_desc->bdev->name);
	ret = blk_derase(blk_desc, 0, blk_desc->lba);
	if (ret != blk_desc->lba) {
		pr_err("Failed to erase the whole device '%s' (ret = %d)\n", blk_desc->bdev->name, ret);
		return -EIO;
	}

	return 0;

erase_both_dev:
	debug("Erasing both device...\n");

	uclass_foreach_dev_probe(UCLASS_MTD, dev) {
		printf("MTD device: %s\n", dev->name);
	}

	ret = uclass_get(UCLASS_BLK, &uc);
	if (ret)
		return ret;
	uclass_foreach_dev(dev, uc) {
		debug("Block device: %s\n", dev->name);

		blk_desc = dev_get_uclass_plat(dev);
		if (IS_ERR_OR_NULL(blk_desc)) {
			pr_err("Failed to get device descriptor\n");
			continue;
		}

		if (blk_desc->if_type != IF_TYPE_MTD) {
			debug("Skip device '%s' (not MTD)\n", blk_desc->bdev->name);
			continue;
		}

		mtd = blk_desc_to_mtd(blk_desc);
		if (!mtd_type_is_nand(mtd)) {
			debug("Skip device '%s' (not NAND)\n", blk_desc->bdev->name);
			continue;
		}

		debug("Erasing device '%s'...\n", blk_desc->bdev->name);
		ret = blk_derase(blk_desc, 0, blk_desc->lba);
		if (ret != blk_desc->lba) {
			pr_err("Failed to erase the whole device '%s' (ret = %d)\n", blk_desc->bdev->name, ret);
			return -EIO;
		}

#ifdef DEBUG
		uint64_t off;
		printf("\nMTD device %s bad blocks list:\n", mtd->name);
		for (off = 0; off < mtd->size; off += mtd->erasesize) {
			if (mtd_block_isbad(mtd, off))
				printf("\t0x%08llx\n", off);
		}
		printf("\n");
#endif
	}

	return 0;
}

int fdl_blk_write_data(const char *part_name, size_t image_size)
{
	struct blk_desc *dev_desc = NULL;
	struct disk_partition part_info = { 0 };
	ulong sector, blkcnt, written;
	int ret;

	ret = fdl_blk_get_dev_and_part(part_name, &dev_desc, &part_info);
	if (ret) {
		pr_err("%s: Failed to get device descriptor and partition information\n", __func__);
		return ret;
	}

	blkcnt = DIV_ROUND_UP_ULL(image_size, dev_desc->blksz);
	sector = part_info.start;
	debug("'%s' partition: sector: 0x%lx, size: 0x%lx\n",
	      part_info.name, part_info.start, part_info.size);
	debug("'%s' device: blkcnt: 0x%lx blksz: 0x%lx\n",
	      dev_desc->bdev->name, blkcnt, dev_desc->blksz);

	written = blk_dwrite(dev_desc, sector,
	                       blkcnt, fdl_buf_addr);
	debug("blk_dwrite: ret = %ld\n", written);
	if (written != blkcnt) {
		pr_err("Size of written image not equal to expected size (%lu != %lu)\n",
		      written * dev_desc->blksz, image_size);
		return -EIO;
	}
	printf("\nwritting of %lu bytes into '%s' partition finished\n",
	                        image_size, part_name);

	return 0;
}

int fdl_blk_get_part_info(const char *part_name, struct disk_partition *pi)
{
	struct blk_desc *dev = NULL;
	int ret;

	ret = fdl_blk_get_dev_and_part(part_name, &dev, pi);
	if (ret) {
		pr_err("%s: Failed to get device descriptor and partition information\n", __func__);
		return ret;
	}

	debug("'%s' partition: start: 0x%lx, size: 0x%lx\n",
	      pi->name, pi->start, pi->size);

	return 0;
}

int fdl_blk_read_data(const char *part_name, size_t image_size)
{
	struct blk_desc *dev_desc = NULL;
	struct disk_partition part_info;
	ulong sector, blkcnt, read_blk;
	int ret;

	ret = fdl_blk_get_dev_and_part(part_name, &dev_desc, &part_info);
	if (ret) {
		pr_err("Failed to get device descriptor and partition information\n");
		return ret;
	}

	blkcnt = DIV_ROUND_UP_ULL(image_size, dev_desc->blksz);
	sector = part_info.start;
	debug("'%s' partition: sector: 0x%lx, size: 0x%lx\n",
	          part_info.name, part_info.start, part_info.size);
	debug("'%s' device: blkcnt: 0x%lx blksz: 0x%lx\n",
	      dev_desc->bdev->name, blkcnt, dev_desc->blksz);

	read_blk = blk_dread(dev_desc, sector,
	                       blkcnt, fdl_buf_addr);
	debug("blk_dread: ret = %ld\n", read_blk);
	if (read_blk != blkcnt) {
		pr_err("Size of written image not equal to expected size (%lu != %lu)\n",
		              read_blk * dev_desc->blksz, image_size);
		return -EIO;
	}
	printf("\nreading of %lu bytes from '%s' partition finished\n",
	                   image_size, part_name);

	return 0;
}