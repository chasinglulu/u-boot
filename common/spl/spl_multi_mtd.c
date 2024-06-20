// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <spl.h>
#include <mtd.h>
#include <linux/compiler.h>
#include <android_ab.h>
#include <errno.h>
#include <asm/u-boot.h>
#include <errno.h>
#include <image.h>

#define MAX_NAME_LEN  32

/* Weak default function for arch/board-specific fixups to the spl_image_info */
void __weak
spl_board_perform_legacy_fixups(struct spl_image_info *spl_image)
{
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

int __weak spl_multi_mtd_ab_select(void)
{
	return 0;
}

static ulong spl_mtd_load_read(struct spl_load_info *load,
                ulong from, ulong count, void *buf)
{
	int err;
	struct mtd_info *mtd = load->dev;
	size_t retlen;

	debug("%s: offset 0x%lx, size 0x%lx, buffer %p\n",
	            __func__, from, count, buf);

	err = mtd_read(mtd, from, count, &retlen, buf);
	if (err < 0)
		return 0;

	return retlen;
}

static int mtd_load_image_offset(struct spl_image_info *spl_image,
                struct spl_boot_device *bootdev,
                struct mtd_info *mtd, unsigned long from)
{
	struct image_header *header;
	size_t retlen;
	int ret = -EINVAL;

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));

	/* read image header to find the image size & load address */
	mtd_read(mtd, 0, sizeof(*header), &retlen, (u_char *)header);
	debug("mtd read offset 0x%llx, count=%lu\n", mtd->offset, retlen);
	if (retlen == 0) {
		ret = -EIO;
		goto end;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
		image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = mtd;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = 1;
		load.read = spl_mtd_load_read;
		ret = spl_load_simple_fit(spl_image, &load, from, header);
	} else if (IS_ENABLED(CONFIG_SPL_LEGACY_IMAGE_FORMAT) &&
		    image_get_magic(header) == IH_MAGIC) {
		struct spl_load_info load;

		debug("Found legacy image\n");
		load.dev = mtd;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = 1;
		load.read = spl_mtd_load_read;
		ret = spl_load_legacy_img(spl_image, bootdev, &load, from);
	}

end:
	if (ret) {
		printf("mtd read error at offset 0x%llx\n", mtd->offset + from);
		return -1;
	}

	return 0;
}

static
struct mtd_info *get_mtd_by_part(struct mtd_info *master,
                                    const char *name)
{
	struct mtd_info *part;

	list_for_each_entry(part, &master->partitions, node)
		if (!strcmp(name, part->name))
			return part;

	debug("Partition '%s' not found on MTD device %s\n", name, master->name);
	return ERR_PTR(-ENODEV);
}

static
struct mtd_info *get_mtd_by_name(const char *name,
                                  struct spl_boot_device *bootdev)
{
	struct mtd_info *mtd, *master;
	const char *mtd_nm;

	mtd_probe_devices();

	switch (bootdev->boot_device) {
	case BOOT_DEVICE_NAND:
		mtd_nm = "spi-nand0";
		break;
	case BOOT_DEVICE_NOR:
#if CONFIG_IS_ENABLED(SPI_FLASH_TINY)
		mtd_nm = "spi-flash";
#else
		mtd_nm = "nor0";
#endif
		break;
	default:
		pr_err("Not supported boot device: %d\n", bootdev->boot_device);
		return NULL;
	}
	debug("MTD device name: %s:%s\n", mtd_nm, name);

	master = get_mtd_device_nm(mtd_nm);
	if (IS_ERR_OR_NULL(master)) {
		printf("MTD device \"%s\" not found, ret %ld\n",
		            name, PTR_ERR(master));
		return master;
	}

	mtd = get_mtd_by_part(master, name);
	if (IS_ERR_OR_NULL(mtd)) {
		printf("MTD part \"%s\" not found, ret %ld\n", name,
		       PTR_ERR(mtd));
		put_mtd_device(master);
		return mtd;
	}
	put_mtd_device(master);
	__get_mtd_device(mtd);

	return mtd;
}

static int spl_mtd_load_multi_image(struct spl_image_info *spl_image,
               struct spl_boot_device *bootdev)
{
	struct spl_image_info image_info;
	static struct mtd_info *mtd;
	bool has_uboot = false;
	bool has_optee = false;
	bool has_atf = false;
	char name[MAX_NAME_LEN];
	int ab_slot;
	int err = 0;
	int i;

	ab_slot = spl_multi_mtd_ab_select();
	if (ab_slot < 0) {
		debug("failed to get ab slot\n");
		return -EINVAL;
	}

	for (i = 0; spl_board_get_part_name(i); i++) {
		snprintf(name, sizeof(name), "%s_%c",
		                spl_board_get_part_name(i),
		                BOOT_SLOT_NAME(ab_slot));
		mtd = get_mtd_by_name(name, bootdev);
		if (IS_ERR_OR_NULL(mtd)) {
			debug("MTD part \"%s\" is not exist\n", name);
			snprintf(name, sizeof(name), "%s",
			            spl_board_get_part_name(i));
			mtd = get_mtd_by_name(name, bootdev);
			if (IS_ERR_OR_NULL(mtd)) {
				debug("MTD part \"%s\" also is not exist\n", name);
				continue;
			}
		}

		memset(&image_info, 0, sizeof(image_info));
		err = mtd_load_image_offset(&image_info, bootdev,
			                    mtd, 0);
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

SPL_LOAD_IMAGE_METHOD("NOR", 0, BOOT_DEVICE_NOR, spl_mtd_load_multi_image);
SPL_LOAD_IMAGE_METHOD("NAND", 0, BOOT_DEVICE_NAND, spl_mtd_load_multi_image);
