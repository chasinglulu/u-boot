// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <env.h>
#include <log.h>
#include <spl.h>
#include <asm/u-boot.h>
#include <fat.h>
#include <errno.h>
#include <image.h>
#include <linux/libfdt.h>
#include <linux/err.h>
#include <fs.h>

static int fat_registered;

const char * __weak spl_board_get_filename(int index)
{
	static const char *filename[] = {
		"bl31.img",
		"optee.img",
		"u-boot.img",
	};

	if (index >= ARRAY_SIZE(filename))
		return NULL;

	return filename[index];
}

static int spl_setup_fat_device(struct blk_desc *dev_dev, struct disk_partition *info)
{
	int err = 0;

	if (fat_registered)
		return err;

	err = fat_set_blk_dev(dev_dev, info);
	if (err) {
		printf("%s: fat register err - %d\n", __func__, err);
		return err;
	}

	fat_registered = 1;

	return err;
}

static ulong spl_fat_load_read(struct spl_load_info *load,
               ulong file_offset,
               ulong size, void *buf)
{
	char *filename = (char *)load->filename;
	loff_t actread;
	int ret;

	ret = fat_read_file(filename, buf, file_offset, size, &actread);
	if (ret)
		return ret;

	return actread;
}

int spl_sdcard_load_image(struct spl_image_info *spl_image,
            struct spl_boot_device *bootdev,
            struct blk_desc *dev_desc,
            struct disk_partition *part,
            const char *filename)
{
	struct image_header *header;
	int ret;

	ret = spl_setup_fat_device(dev_desc, part);
	if (ret < 0)
		return -ENODEV;

	if (!fat_exists(filename)) {
		debug("The file (%s) does not exist\n", filename);
		return -ENOENT;
	}

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));

	/* read image header to find the image size & load address */
	ret = file_fat_read(filename, header, sizeof(*header));
	debug("%s: filename %s\n", __func__, filename);
	if (ret <= 0) {
		debug("failed to load file header by fat_read\n");
		goto end;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = NULL;
		load.priv = NULL;
		load.filename = filename;
		load.bl_len = 1;
		load.read = spl_fat_load_read;
		ret = spl_load_simple_fit(spl_image, &load, 0, header);
	} else if (IS_ENABLED(CONFIG_SPL_LEGACY_IMAGE_FORMAT) &&
	            image_get_magic(header) == IH_MAGIC) {
		struct spl_load_info load;

		debug("Found legacy image\n");
		load.dev = NULL;
		load.priv = NULL;
		load.filename = filename;
		load.bl_len = 1;
		load.read = spl_fat_load_read;
		ret = spl_load_legacy_img(spl_image, bootdev, &load, 0);
	}

end:
	if (ret < 0) {
		printf(" fat reading error (filename %s ret %d)\n", filename, ret);
		return ret;
	}

	return 0;
}

static struct mmc *spl_sdcard_get_dev_by_index(int idx)
{
	struct mmc *mmc;
	int err;

	err = mmc_init_device(idx);
	if (err < 0) {
		debug("Could not initialize mmc (error %d mmc_idx %d)\n", err, idx);
		return NULL;
	}

	mmc = find_mmc_device(idx);
	if (IS_ERR_OR_NULL(mmc)) {
		debug("Could not find mmc device %d (err %d)\n", idx, err);
		return NULL;
	}

	err = mmc_init(mmc);
	if (err) {
		debug("mmc init failed with error: %d (mmc_dev %d)\n", err, idx);
		return NULL;
	}

	if (!IS_SD(mmc)) {
		debug("Not SD version");
		return NULL;
	}

	return mmc;
}

static struct mmc *spl_sdcard_find_device(struct spl_boot_device *bootdev)
{
	int mmc_dev = CONFIG_VAL(MULTI_FAT_MMC_INDEX);
	int i, max_devnum;
	struct mmc *mmc;

	mmc = spl_sdcard_get_dev_by_index(mmc_dev);
	if (IS_ERR_OR_NULL(mmc)) {
		debug("Could not get mmc device (idx %d)\n", mmc_dev);
		goto fallback;
	}

	return mmc;

fallback:
	/*
	 * Find the first sdcard device
	 */
	mmc = NULL;
	max_devnum = blk_find_max_devnum(IF_TYPE_MMC);
	for (i = 0; i < max_devnum; i++) {
		if (i == mmc_dev)
			continue;

		mmc = spl_sdcard_get_dev_by_index(i);
		if (!IS_ERR_OR_NULL(mmc))
			return mmc;
	}

	if (IS_ERR_OR_NULL(mmc)) {
		pr_err("No vaild mmc device found\n");
		return ERR_PTR(-ENODEV);
	}

	return NULL;
}

static int spl_sdcard_check_fat_part(struct blk_desc *dev_desc, int part)
{
	int ret;
	int fs_type;

	ret = fs_set_blk_dev_with_part(dev_desc, part);
	if (ret) {
		debug("fs_set_blk_dev error %d\n", ret);
		return -ENXIO;
	}

	fs_type = fs_get_type();
	if (fs_type != FS_TYPE_FAT) {
		debug("Not fat partition (part %d)", part);
		return -EINVAL;
	}

	fs_close();

	return 0;
}

static int spl_sdcard_get_part_info(struct blk_desc *dev_desc,
                                      struct disk_partition *info)
{
	int part = CONFIG_VAL(MULTI_FAT_PART);
	int ret, i;

	if (IS_ERR_OR_NULL(info))
		return -EINVAL;

	ret = spl_sdcard_check_fat_part(dev_desc, part);
	if (ret < 0) {
		debug("Could not find FAT/vFAT filesystem (part %d)\n", part);
		goto fallback;
	}

	ret = part_get_info(dev_desc, part, info);
	if (ret) {
		debug("Failed to get part info (idx %d)\n", part);
		goto fallback;
	}

	return 0;

fallback:
	/*
	 * Find the first fat/vfat partition.
	 */
	part = 0;
	for (i = 1; i <= MAX_SEARCH_PARTITIONS; i++) {
		if (i == CONFIG_VAL(MULTI_FAT_PART))
			continue;

		ret = spl_sdcard_check_fat_part(dev_desc, i);
		if (ret < 0)
			continue;

		if (!part)
			part = i;

		if (!ret)
			break;
	}
	/* If we found any acceptable partition */
	if (part) {
		ret = part_get_info(dev_desc, part, info);
		if (ret)
			return -ENODEV;
	} else {
		pr_err("No valid partitions found\n");
		return -ENXIO;
	}

	return 0;
}

static
int spl_sdcard_load_multi_images(struct spl_image_info *spl_image,
                                struct spl_boot_device *bootdev)
{
	struct spl_image_info image_info;
	struct disk_partition info;
	struct blk_desc *dev_desc;
	static struct mmc *mmc;
	bool has_uboot = false;
	bool has_optee = false;
	bool has_atf = false;
	int err = 0;
	int i;

	mmc = spl_sdcard_find_device(bootdev);
	if (IS_ERR_OR_NULL(mmc)) {
		pr_err("Unable to find available mmc device\n");
		return -ENODEV;
	}

	dev_desc = mmc_get_blk_desc(mmc);
	if (!dev_desc) {
		debug("Failed to get block descriptor\n");
		return -ENODEV;
	}

	err = spl_sdcard_get_part_info(dev_desc, &info);
	if (err < 0) {
		pr_err("Could not find \"FAT/vFAT\" partition\n");
		return -ENODEV;
	}

	for (i = 0; spl_board_get_filename(i); i++) {
		memset(&image_info, 0, sizeof(image_info));
		err = spl_sdcard_load_image(&image_info, bootdev,
		                    dev_desc, &info,
		          spl_board_get_filename(i));
		if (err)
			continue;

		if (image_info.os == IH_OS_ARM_TRUSTED_FIRMWARE
			|| (image_info.os == IH_OS_U_BOOT && !has_atf)) {
			spl_image->name = image_info.name;
			spl_image->load_addr = image_info.load_addr;
			spl_image->entry_point = image_info.entry_point;
		}

		switch (image_info.os) {
		case IH_OS_ARM_TRUSTED_FIRMWARE:
			spl_image->os = IH_OS_ARM_TRUSTED_FIRMWARE;
			has_atf = true;
			break;
		case IH_OS_U_BOOT:
			has_uboot = true;
#if CONFIG_IS_ENABLED(LOAD_FIT) || CONFIG_IS_ENABLED(LOAD_FIT_FULL)
			spl_image->fdt_addr = image_info.fdt_addr;
#endif

			if (has_atf)
				break;

			spl_image->os = IH_OS_U_BOOT;
			break;
		case IH_OS_TEE:
			/* SPL does not support direct jump to optee */
			has_optee = true;
			break;
		default:
			pr_err("Invaild Image: %d\n", image_info.os);
			return -EINVAL;
		}
	}

	if (!has_uboot)
		return -ENODATA;

#if CONFIG_IS_ENABLED(LOAD_STRICT_CHECK)
	if (!has_atf || !has_optee)
		return -ENODATA;
#endif

	return err;
}
SPL_LOAD_IMAGE_METHOD("SDCard", 0, BOOT_DEVICE_MMC2, spl_sdcard_load_multi_images);