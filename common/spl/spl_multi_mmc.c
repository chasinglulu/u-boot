// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <part.h>
#include <spl.h>
#include <linux/compiler.h>
#include <errno.h>
#include <asm/u-boot.h>
#include <errno.h>
#include <mmc.h>
#include <image.h>
#include <android_ab.h>

#define MAX_NAME_LEN  32

/* Weak default function for arch/board-specific fixups to the spl_image_info */
int __weak
spl_board_perform_legacy_fixups(struct spl_image_info *spl_image)
{
	return 0;
}

static int mmc_load_legacy(struct spl_image_info *spl_image,
			   struct spl_boot_device *bootdev,
			   struct mmc *mmc,
			   ulong sector, struct image_header *header)
{
	u32 image_offset_sectors;
	u32 image_size_sectors;
	unsigned long count;
	u32 image_offset;
	int ret;

	ret = spl_parse_image_header(spl_image, bootdev, header);
	if (ret)
		return ret;

	/* For some baord image, the load address was not
	 * specified in legacy image header. We can't use it.
	 * So fixup it in the case.
	 */
	spl_board_perform_legacy_fixups(spl_image);

	/* convert offset to sectors - round down */
	image_offset_sectors = spl_image->offset / mmc->read_bl_len;
	/* calculate remaining offset */
	image_offset = spl_image->offset % mmc->read_bl_len;

	/* convert size to sectors - round up */
	image_size_sectors = (spl_image->size + mmc->read_bl_len - 1) /
		                       mmc->read_bl_len;

	/* Read the header too to avoid extra memcpy */
	count = blk_dread(mmc_get_blk_desc(mmc),
			  sector + image_offset_sectors,
			  image_size_sectors,
			  (void *)(ulong)spl_image->load_addr);
	debug("read %x sectors to %lx\n", image_size_sectors,
	            spl_image->load_addr);
	if (count != image_size_sectors)
		return -EIO;

	if (image_offset)
		memmove((void *)(ulong)spl_image->load_addr,
			(void *)(ulong)spl_image->load_addr + image_offset,
			spl_image->size);

	return 0;
}

static ulong h_spl_load_read(struct spl_load_info *load, ulong sector,
			     ulong count, void *buf)
{
	struct mmc *mmc = load->dev;

	return blk_dread(mmc_get_blk_desc(mmc), sector, count, buf);
}

static int mmc_load_image_raw_sector(struct spl_image_info *spl_image,
			      struct spl_boot_device *bootdev,
			      struct mmc *mmc, unsigned long sector)
{
	unsigned long count;
	struct image_header *header;
	struct blk_desc *bd = mmc_get_blk_desc(mmc);
	int ret = -EINVAL;

	header = spl_get_load_buffer(-sizeof(*header), bd->blksz);

	/* read image header to find the image size & load address */
	count = blk_dread(bd, sector, 1, header);
	debug("hdr read sector %lx, count=%lu\n", sector, count);
	if (count == 0) {
		ret = -EIO;
		goto end;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
		image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = mmc;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = mmc->read_bl_len;
		load.read = h_spl_load_read;
		ret = spl_load_simple_fit(spl_image, &load, sector, header);
	} else if (IS_ENABLED(CONFIG_SPL_LEGACY_IMAGE_FORMAT) &&
		    image_get_magic(header) == IH_MAGIC) {
		debug("Found legacy image\n");

		ret = mmc_load_legacy(spl_image, bootdev, mmc, sector, header);
	}

end:
	if (ret) {
		printf("mmc block read error at [0x%lx] offset\n", sector * mmc->read_bl_len);
		return -1;
	}

	return 0;
}

static int spl_mmc_get_device_index(u32 boot_device)
{
	switch (boot_device) {
	case BOOT_DEVICE_MMC1:
		return 0;
	case BOOT_DEVICE_MMC2:
		return 1;
	}

	printf("spl: unsupported mmc boot device.\n");
	return -ENODEV;
}

static int spl_mmc_find_device(struct mmc **mmcp, u32 boot_device)
{
	int err, mmc_dev;

	mmc_dev = spl_mmc_get_device_index(boot_device);
	if (mmc_dev < 0)
		return mmc_dev;

#if CONFIG_IS_ENABLED(DM_MMC)
	err = mmc_init_device(mmc_dev);
#else
	err = mmc_initialize(NULL);
#endif
	if (err) {
		printf("spl: could not initialize mmc. error: %d\n", err);
		return err;
	}
	*mmcp = find_mmc_device(mmc_dev);
	err = *mmcp ? 0 : -ENODEV;
	if (err) {
		printf("spl: could not find mmc device %d. error: %d\n",
				mmc_dev, err);
		return err;
	}

	return 0;
}

static int spl_mmc_get_mmc_devnum(struct mmc *mmc)
{
	struct blk_desc *block_dev;
#if !CONFIG_IS_ENABLED(BLK)
	block_dev = &mmc->block_dev;
#else
	block_dev = dev_get_uclass_plat(mmc->dev);
#endif
	return block_dev->devnum;
}

u32 __weak spl_mmc_boot_mode(struct mmc *mmc, const u32 boot_device)
{
	return MMCSD_MODE_RAW;
}

const char * __weak spl_board_get_part_name(int index)
{
	static const char *part_lable[] = {
		"atf",
		"optee",
		"uboot",
	};

	if (index >= ARRAY_SIZE(part_lable))
		return NULL;

	return part_lable[index];
}

int __weak spl_multi_mmc_ab_select(struct blk_desc *dev_desc)
{
	return 0;
}

static int spl_mmc_load_multi_image(struct spl_image_info *spl_image,
		       struct spl_boot_device *bootdev)
{
	struct spl_image_info image_info;
	struct disk_partition info;
	struct blk_desc *dev_desc;
	static struct mmc *mmc;
	bool has_uboot = false;
	bool has_optee = false;
	bool has_atf = false;
	char name[MAX_NAME_LEN];
	int mmc_dev;
	int ab_slot;
	int err = 0;
	int i;

	/* Perform peripheral init only once for an mmc device */
	mmc_dev = spl_mmc_get_device_index(bootdev->boot_device);
	if (!mmc || spl_mmc_get_mmc_devnum(mmc) != mmc_dev) {
		err = spl_mmc_find_device(&mmc, bootdev->boot_device);
		if (err)
			return err;

		err = mmc_init(mmc);
		if (err) {
			mmc = NULL;
			printf("spl: mmc init failed with error: %d\n", err);
			return err;
		}
	}

	dev_desc = mmc_get_blk_desc(mmc);
	if (!dev_desc) {
		debug("failed to get block descriptor\n");
		return -ENODEV;
	}

	ab_slot = spl_multi_mmc_ab_select(dev_desc);
	if (ab_slot < 0) {
		debug("failed to get ab slot\n");
		return -EINVAL;
	}

	for (i = 0; spl_board_get_part_name(i); i++) {
		snprintf(name, sizeof(name), "%s_%c",
		                spl_board_get_part_name(i),
		                BOOT_SLOT_NAME(ab_slot));
		err = part_get_info_by_name(dev_desc, name, &info);
		if (err < 0) {
			debug("%s part not exist\n", name);
			snprintf(name, sizeof(name), "%s",
			            spl_board_get_part_name(i));
			err = part_get_info_by_name(dev_desc, name, &info);
			if (err < 0) {
				debug("%s part also not exist\n", name);
				continue;
			}
		}

		memset(&image_info, 0, sizeof(image_info));
		err = mmc_load_image_raw_sector(&image_info, bootdev,
			                mmc, info.start);
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

SPL_LOAD_IMAGE_METHOD("MMC1", 0, BOOT_DEVICE_MMC1, spl_mmc_load_multi_image);
SPL_LOAD_IMAGE_METHOD("MMC2", 0, BOOT_DEVICE_MMC2, spl_mmc_load_multi_image);
