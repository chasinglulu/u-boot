
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
#include <blk.h>
#include <image.h>
#include <mtd.h>
#include <android_ab.h>
#include <hexdump.h>

#define MAX_NAME_LEN    32

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

int __weak spl_multi_blk_ab_select(void)
{
	return 0;
}

static struct blk_desc *get_blk_by_devnum(struct spl_boot_device *bootdev, int devnum)
{
	enum if_type if_type;
	struct blk_desc *dev_desc;
	int max_devnum;

	if (!blk_enabled())
		return NULL;

	switch (bootdev->boot_device) {
	case BOOT_DEVICE_NAND:
		if_type = IF_TYPE_MTD;
		break;
	case BOOT_DEVICE_NOR:
		if_type = IF_TYPE_MTD;
		break;
	case BOOT_DEVICE_MMC1:
		if_type = IF_TYPE_MMC;
		break;
	default:
		pr_err("Not supported boot device: %d\n", bootdev->boot_device);
		return NULL;
	}

	if (if_type == IF_TYPE_MTD)
		mtd_probe_devices();

	max_devnum = blk_find_max_devnum(if_type);
	debug("max devnum: %d\n", max_devnum);
	if (max_devnum < 0)
		return ERR_PTR(max_devnum);
	if (devnum > max_devnum)
		return ERR_PTR(-EINVAL);

	dev_desc = blk_get_devnum_by_type(if_type, devnum);
	if (IS_ERR_OR_NULL(dev_desc)) {
		pr_err("No device for iface '%s', dev %d\n", blk_get_if_type_name(if_type), devnum);
		return ERR_PTR(-ENODEV);
	}

	return dev_desc;
}

static int get_part_by_name(struct blk_desc *dev_desc, const char *name,
                      struct disk_partition *part)
{
	int ret;

	if (IS_ERR_OR_NULL(part))
		return -EINVAL;

	ret = part_get_info_by_name(dev_desc, name, part);
	if (ret < 0) {
		debug("Unable to find '%s' partition (if_type %u devnum %d)\n",
		              name, dev_desc->if_type, dev_desc->devnum);
		return ret;
	}

	return ret;
}

static ulong spl_blk_load_read(struct spl_load_info *load,
                ulong sector, ulong count, void *buf)
{
	int blks_read;
	struct blk_desc *dev_desc = load->dev;

	debug("%s: start 0x%lx, blkcnt 0x%lx, buffer %p\n",
	            __func__, sector, count, buf);

	blks_read = blk_dread(dev_desc, sector, count, buf);
	if (blks_read == (ulong)-ENOSYS)
		return 0;

	return blks_read;
}

static ulong spl_blk_load_legacy(struct spl_load_info *load,
                ulong start, ulong size, void *buf)
{
	struct blk_desc *dev_desc = load->dev;
	ulong blkcnt, sector, len;
	ulong blks_read;
	void *data;

	debug("%s: start 0x%lx, size 0x%lx, buffer %p\n",
	            __func__, start, size, buf);

	blkcnt = DIV_ROUND_UP_ULL(size, load->bl_len);
	sector = DIV_ROUND_CLOSEST(start, load->bl_len);
	len = blkcnt * load->bl_len;

	data = malloc(len);
	if (IS_ERR_OR_NULL(data)) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	};

	debug("%s: sector: 0x%lx blkcnt 0x%lx\n", __func__, sector, blkcnt);
	blks_read = blk_dread(dev_desc, sector, blkcnt, data);
	if (blks_read == (ulong)-ENOSYS) {
		pr_err("blk_dread failed\n");
		blks_read = 0;
		goto failed;
	}

	memcpy(buf, data, size);

failed:
	free(data);
	return blks_read;
}

static int spl_blk_load_image_part(struct spl_image_info *spl_image,
                struct spl_boot_device *bootdev,
                struct blk_desc *dev_desc,
                struct disk_partition *part)
{
	struct image_header *header;
	size_t retlen;
	ulong blkcnt;
	int ret = -EINVAL;

	debug("%s: part start 0x%lx size 0x%lx blksz 0x%lx name %s\n",
	         __func__, part->start, part->size, part->blksz, part->name);

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));

	blkcnt = DIV_ROUND_UP_ULL(sizeof(*header), dev_desc->blksz);

	/* read image header to find the image size & load address */
	retlen = blk_dread(dev_desc, part->start, blkcnt, (char *)header);
	debug("blk_dread start 0x%lx blkcnt %lu\n", part->start, retlen);
	if (retlen == (ulong)-ENOSYS) {
		ret = -EIO;
		goto end;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = dev_desc;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = part->blksz;
		load.read = spl_blk_load_read;
		ret = spl_load_simple_fit(spl_image, &load, part->start, header);
	} else if (IS_ENABLED(CONFIG_SPL_LEGACY_IMAGE_FORMAT) &&
	            image_get_magic(header) == IH_MAGIC) {
		struct spl_load_info load;

		debug("Found legacy image\n");
		load.dev = dev_desc;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = part->blksz;
		load.read = spl_blk_load_legacy;
		ret = spl_load_legacy_img(spl_image, bootdev, &load, part->start * part->blksz);
	}

end:
	if (ret < 0) {
		printf("blk_dread error (start 0x%lx ret=%d)\n", part->start, ret);
		return ret;
	}

	return 0;
}

static int spl_blk_load_multi_images(struct spl_image_info *spl_image,
                                struct spl_boot_device *bootdev)
{
	struct disk_partition info;
	struct spl_image_info image_info;
	struct blk_desc *dev_desc;
	bool has_uboot = false;
	bool has_optee = false;
	bool has_atf = false;
	char name[MAX_NAME_LEN];
	int devnum = CONFIG_VAL(MULTI_BLK_DEVNUM);
	int ab_slot;
	int err = 0;
	int i;

	ab_slot = spl_multi_blk_ab_select();
	if (ab_slot < 0) {
		pr_err("failed to get ab slot\n");
		return -EINVAL;
	}

	dev_desc = get_blk_by_devnum(bootdev, devnum);
	if (IS_ERR_OR_NULL(dev_desc)) {
		pr_err("device not available (devnum %d)\n", devnum);
		return -ENODEV;
	}

	for (i = 0; spl_board_get_part_name(i); i++) {
		snprintf(name, sizeof(name), "%s_%c",
		            spl_board_get_part_name(i),
		            BOOT_SLOT_NAME(ab_slot));
		err = get_part_by_name(dev_desc, name, &info);
		if (err < 0) {
			debug("Could not find \"%s\" partition", name);
			snprintf(name, sizeof(name), "%s",
			            spl_board_get_part_name(i));
			err = get_part_by_name(dev_desc, name, &info);
			if (err < 0) {
				debug("Also could not find \"%s\" partition", name);
				continue;
			}
		}

		memset(&image_info, 0, sizeof(image_info));
		err = spl_blk_load_image_part(&image_info, bootdev,
		                          dev_desc, &info);
		if (err) {
			if (err == -EPERM) {
				pr_err("Verfication failed in secure booting mode\n");
				goto failed;
			}
			continue;
		}

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

failed:
	return err;
}
SPL_LOAD_IMAGE_METHOD("nor_mtd_blk", 0, BOOT_DEVICE_NOR, spl_blk_load_multi_images);
SPL_LOAD_IMAGE_METHOD("nand_mtd_blk", 0, BOOT_DEVICE_NAND, spl_blk_load_multi_images);
SPL_LOAD_IMAGE_METHOD("mmc_blk", 0, BOOT_DEVICE_MMC1, spl_blk_load_multi_images);