/* SPDX-License-Identifier: GPL-2.0+
 *
 * Android AB-specific Bootloader Message Management
 *
 * Copyright (C) 2025 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <linux/err.h>
#include <u-boot/crc.h>
#include <malloc.h>
#include <memalign.h>
#include <part.h>
#include <errno.h>
#include <android_ab.h>
#include <android_bootloader_message.h>

typedef struct bootloader_message_ab bl_msg_ab_t;

__weak const char *bootloader_message_ab_get_blk_ifname(void)
{
	return (const char *)CONFIG_BOOTLOADER_MESSAGE_AB_IFNAME;
}

__weak const char *bootloader_message_ab_get_blk_dev_part(int copy)
{
#ifdef CONFIG_BOOTLOADER_MESSAGE_AB_REDUNDANT
	if (copy)
		return (const char *)CONFIG_BOOTLOADER_MESSAGE_AB_DEV_PART_REDUND;
#endif

	return (const char *)CONFIG_BOOTLOADER_MESSAGE_AB_DEV_PART;
}

bool bootloader_message_ab_is_redund(void)
{
	return IS_ENABLED(CONFIG_BOOTLOADER_MESSAGE_AB_REDUNDANT);
}

#if defined(CONFIG_BOOTLOADER_MESSAGE_AB_REDUNDANT)
/* Value for AB-specific bootloader message validity */
enum bootloader_message_ab_valid {
	BOOTLOADER_MESSAGE_AB_INVALID,	/* No valid AB-specific bootloader message */
	BOOTLOADER_MESSAGE_AB_VALID,	/* First or only AB-specific bootloader message is valid */
	BOOTLOADER_MESSAGE_AB_REDUND,	/* Redundant AB-specific bootloader message is valid */
};

static unsigned long bootloader_message_ab_valid = BOOTLOADER_MESSAGE_AB_INVALID;
static uint8_t bootloader_message_ab_flags = 0;

static int check_redund(const char *buf1, int buf1_read_fail,
		     const char *buf2, int buf2_read_fail)
{
	int crc1_ok = 0, crc2_ok = 0;
	uint32_t crc1, crc2;
	bl_msg_ab_t *tmp1, *tmp2;

	tmp1 = (bl_msg_ab_t *)buf1;
	tmp2 = (bl_msg_ab_t *)buf2;

	if (buf1_read_fail && buf2_read_fail) {
		puts("*** Error - No Valid AB-specific Bootloader Message Area found\n");
		return -EIO;
	} else if (buf1_read_fail || buf2_read_fail) {
		puts("*** Warning - some problems detected ");
		puts("reading AB-specific bootloader message; recovered successfully\n");
	}

	if (!buf1_read_fail) {
		crc1 = tmp1->crc32_le;
		debug("%s: CRC1 = 0x%08x\n", __func__, crc1);

		tmp1->crc32_le = 0; /* clear CRC for calculation */
		crc1_ok = crc32(0, (void *)tmp1,
		            offsetof(bl_msg_ab_t, crc32_le)) == crc1;
	}

	if (!buf2_read_fail) {
		crc2 = tmp2->crc32_le;
		debug("%s: CRC2 = 0x%08x\n", __func__, crc2);

		tmp2->crc32_le = 0; /* clear CRC for calculation */
		crc2_ok = crc32(0, (void *)tmp2,
		            offsetof(bl_msg_ab_t, crc32_le)) == crc2;
	}

	if (crc1_ok && !crc2_ok) {
		bootloader_message_ab_valid = BOOTLOADER_MESSAGE_AB_VALID;
	} else if (!crc1_ok && crc2_ok) {
		bootloader_message_ab_valid = BOOTLOADER_MESSAGE_AB_REDUND;
	} else {
		/* both ok or not okay - check serial */
		if (tmp1->flags == 255 && tmp2->flags == 0)
			bootloader_message_ab_valid = BOOTLOADER_MESSAGE_AB_REDUND;
		else if (tmp2->flags == 255 && tmp1->flags == 0)
			bootloader_message_ab_valid = BOOTLOADER_MESSAGE_AB_VALID;
		else if (tmp1->flags > tmp2->flags)
			bootloader_message_ab_valid = BOOTLOADER_MESSAGE_AB_VALID;
		else if (tmp2->flags > tmp1->flags)
			bootloader_message_ab_valid = BOOTLOADER_MESSAGE_AB_REDUND;
		else /* flags are equal - almost impossible */
			bootloader_message_ab_valid = BOOTLOADER_MESSAGE_AB_VALID;
	}

	return 0;
}

static bl_msg_ab_t *load_redund(const char *buf1, int buf1_read_fail,
                         const char *buf2, int buf2_read_fail)
{
	bl_msg_ab_t *ep;
	int ret;

	ret = check_redund(buf1, buf1_read_fail, buf2, buf2_read_fail);
	if (ret == -EIO) {
		return ERR_PTR(-EIO);
	} else if (ret == -ENOMSG) {
		return ERR_PTR(-ENOMSG);
	}

	if (bootloader_message_ab_valid == BOOTLOADER_MESSAGE_AB_VALID)
		ep = (bl_msg_ab_t *)buf1;
	else
		ep = (bl_msg_ab_t *)buf2;

	bootloader_message_ab_flags = ep->flags;

	return ep;
}
#endif /* CONFIG_BOOTLOADER_MESSAGE_AB_REDUNDANT */

static int find_dev_info(const char *ifname, const char *dev_and_part,
                                 struct blk_desc **dev_desc,
                                 struct disk_partition *part_info)
{
	struct blk_desc *desc;
	struct disk_partition info;
	int ret;

	if (!ifname || !dev_and_part) {
		debug("Interface or device/partition not defined.\n");
		return -ENODEV;
	}

	ret = part_get_info_by_dev_and_name_or_num(ifname,
						   dev_and_part, &desc,
						   &info, 1);
	if (ret < 0) {
		pr_err("Could not retrieve partition info\n");
		return ret;
	}

	if (info.size * desc->blksz < sizeof(bl_msg_ab_t)) {
		pr_err("0x%lx (Bootloader metadata Size) > 0x%lx (Partition Size)\n",
		       sizeof(bl_msg_ab_t), info.size * desc->blksz);
		return -ENOSPC;
	}

	if (dev_desc && part_info) {
		*dev_desc = desc;
		memcpy(part_info, &info, sizeof(*part_info));
	}

	return 0;
}

static inline int
read_bootloader_control(struct blk_desc *dev_desc, lbaint_t start,
                        void *buffer, unsigned long size)
{
	uint blkcnt, n;

	blkcnt = ALIGN(size, dev_desc->blksz) / dev_desc->blksz;

	n = blk_dread(dev_desc, start, blkcnt, buffer);

	return (n == blkcnt) ? 0 : -EIO;
}

#if defined(CONFIG_BOOTLOADER_MESSAGE_AB_REDUNDANT)
int bootloader_message_ab_load(struct bootloader_message_ab *buffer)
{
	ALLOC_CACHE_ALIGN_BUFFER(bl_msg_ab_t, tmp1, 1);
	ALLOC_CACHE_ALIGN_BUFFER(bl_msg_ab_t, tmp2, 1);
	struct blk_desc *dev_desc = NULL;
	struct disk_partition info;
	bl_msg_ab_t *ep = NULL;
	const char *ifname = NULL;
	const char *dev_part_str = NULL;
	int ret, copy= 0;
	int read1_fail = 0, read2_fail = 0;

	if (IS_ERR_OR_NULL(buffer)) {
		debug("Invalid buffer pointer\n");
		return -EINVAL;
	}

	ifname = bootloader_message_ab_get_blk_ifname();
	dev_part_str = bootloader_message_ab_get_blk_dev_part(copy);
	debug("Loading AB-specific bootloader message from '%s %s'\n",
	                  ifname, dev_part_str);
	ret = find_dev_info(ifname, dev_part_str,
	                      &dev_desc, &info);
	if (ret)
		return ret;

	if (!IS_ALIGNED(sizeof(bl_msg_ab_t), dev_desc->blksz)) {
		debug("0x%lx (Bootloader Metadata Size) is not aligned to 0x%lx (Block Size)\n",
		            sizeof(bl_msg_ab_t), dev_desc->blksz);
		return -EINVAL;
	}
	read1_fail = read_bootloader_control(dev_desc, info.start,
	                          tmp1, sizeof(bl_msg_ab_t));

	copy = 1; /* load redundant copy */
	ifname = bootloader_message_ab_get_blk_ifname();
	dev_part_str = bootloader_message_ab_get_blk_dev_part(copy);
	debug("Loading redundant AB-specific bootloader message from '%s %s'\n",
	                        ifname, dev_part_str);
	ret = find_dev_info(ifname, dev_part_str,
	               &dev_desc, &info);
	if (ret)
		return ret;

	if (!IS_ALIGNED(sizeof(bl_msg_ab_t), dev_desc->blksz)) {
		debug("0x%lx (Bootloader Message Size) is not aligned to 0x%lx (Block Size)\n",
		            sizeof(bl_msg_ab_t), dev_desc->blksz);
		return -EINVAL;
	}

	read2_fail = read_bootloader_control(dev_desc, info.start,
	                          tmp2, sizeof(bl_msg_ab_t));

	ep = load_redund((char *)tmp1, read1_fail, (char *)tmp2,
				read2_fail);
	if (IS_ERR_OR_NULL(ep)) {
		debug("Could not load redundant AB-specific bootloader message from '%s %s'\n",
		       ifname, dev_part_str);
		return PTR_ERR(ep);
	}

	memcpy(buffer, ep, sizeof(bl_msg_ab_t));
	return 0;
}
#else
int bootloader_message_ab_load(struct bootloader_message_ab *buffer)
{
	ALLOC_CACHE_ALIGN_BUFFER(bl_msg_ab_t, tmp, 1);
	struct blk_desc *dev_desc = NULL;
	struct disk_partition info;
	const char *ifname = NULL;
	const char *dev_part_str = NULL;
	int ret, copy= 0;

	if (IS_ERR_OR_NULL(buffer)) {
		debug("Invalid buffer pointer\n");
		return -EINVAL;
	}

	ifname = bootloader_message_ab_get_blk_ifname();
	dev_part_str = bootloader_message_ab_get_blk_dev_part(copy);
	debug("Loading AB-specific bootloader message from '%s %s'\n", ifname, dev_part_str);
	ret = find_dev_info(ifname, dev_part_str,
	                      &dev_desc, &info);
	if (ret)
		return ret;

	if (!IS_ALIGNED(sizeof(bl_msg_ab_t), dev_desc->blksz)) {
		debug("0x%lx (Bootloader Message Size) is not aligned to 0x%lx (Block Size)\n",
		            sizeof(bl_msg_ab_t), dev_desc->blksz);
		return -EINVAL;
	}

	ret = read_bootloader_control(dev_desc, info.start,
	                           tmp, sizeof(bl_msg_ab_t));
	if (ret < 0) {
		debug("Could not read AB-specific bootloader message from '%s %s'\n",
		       ifname, dev_part_str);
		return ret;
	}

	memcpy(buffer, tmp, sizeof(bl_msg_ab_t));
	return 0;
}
#endif /* CONFIG_BOOTLOADER_MESSAGE_AB_REDUNDANT */

static inline int
write_bootloader_control(struct blk_desc *dev_desc, lbaint_t start,
                             void *buffer, size_t size)
{
	lbaint_t blkcnt, n;
	blkcnt = ALIGN(size, dev_desc->blksz) / dev_desc->blksz;

	n = blk_dwrite(dev_desc, start, blkcnt, buffer);

	return (n == blkcnt) ? 0 : -EIO;
}

int bootloader_message_ab_store(struct bootloader_message_ab *buffer)
{
	struct blk_desc *dev_desc = NULL;
	struct disk_partition info;
	const char *ifname = NULL;
	const char *dev_part_str = NULL;
	bl_msg_ab_t *ab_msg_new = NULL;
	int ret, copy = 0;

	ab_msg_new = buffer;
#ifdef CONFIG_BOOTLOADER_MESSAGE_AB_REDUNDANT
	ab_msg_new->crc32_le = 0;
	ab_msg_new->crc32_le = crc32(0, (void *)ab_msg_new, sizeof(bl_msg_ab_t));
	ab_msg_new->flags = ++bootloader_message_ab_flags; /* increase the serial */
	if (bootloader_message_ab_valid == BOOTLOADER_MESSAGE_AB_VALID)
		copy = 1;
#endif

	ifname = bootloader_message_ab_get_blk_ifname();
	dev_part_str = bootloader_message_ab_get_blk_dev_part(copy);
	debug("Storing bootloader metadata into '%s %s'\n", ifname, dev_part_str);
	ret = find_dev_info(ifname, dev_part_str,
	                      &dev_desc, &info);
	if (ret)
		return ret;

	if (!IS_ALIGNED(sizeof(bl_msg_ab_t), dev_desc->blksz)) {
		debug("0x%lx (Bootloader Message Size) is not aligned to 0x%lx (Block Size)\n",
		            sizeof(bl_msg_ab_t), dev_desc->blksz);
		return -EINVAL;
	}

	debug("Writing AB-specific bootloader message at offset '0x%08lx'\n",
	                      info.start * info.blksz);
	ret = write_bootloader_control(dev_desc, info.start, ab_msg_new,
	               sizeof(bl_msg_ab_t));
	if (ret) {
		debug("Could not write AB-specific bootloader message into '%s %s'\n",
		       ifname, dev_part_str);
		return ret;
	}

#ifdef CONFIG_BOOTLOADER_MESSAGE_AB_REDUNDANT
	if (bootloader_message_ab_valid == BOOTLOADER_MESSAGE_AB_REDUND)
		bootloader_message_ab_valid = BOOTLOADER_MESSAGE_AB_VALID;
	else
		bootloader_message_ab_valid = BOOTLOADER_MESSAGE_AB_REDUND;
#endif

	return 0;
}