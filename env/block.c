// SPDX-License-Identifier: GPL-2.0+
/*
 * Environment operations for block devices
 *
 * Copyright (C) 2025 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <command.h>
#include <fdtdec.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <memalign.h>
#include <part.h>
#include <search.h>
#include <errno.h>
#include <dm/ofnode.h>
#include <env.h>
#include <env_internal.h>
#include <mtd.h>
#include <hexdump.h>

DECLARE_GLOBAL_DATA_PTR;

__weak const char *env_blk_get_intf(void)
{
	return (const char *)CONFIG_ENV_BLK_INTERFACE;
}

__weak const char *env_blk_get_dev_part(int copy, bool only_dev)
{
	// return (const char *)CONFIG_ENV_BLK_DEVICE_AND_PART;
	mtd_probe_devices();

	if (only_dev)
		return (const char *)CONFIG_ENV_BLK_DEVICE;

#if defined(CONFIG_ENV_BLK_PARTITION_REDUND)
	if (copy) {
			return (const char *)CONFIG_ENV_BLK_DEVICE_AND_PART_REDUND;
	} else
#endif
	{
		return (const char *)CONFIG_ENV_BLK_DEVICE_AND_PART;
	}
}

#if CONFIG_IS_ENABLED(OF_CONTROL)
const struct {
	const char *partition_reund;
	const char *partition;
	const char *offset_redund;
	const char *offset;
} dt_prop = {
	.partition_reund = "u-boot,blk-env-partition-redundant",
	.partition = "u-boot,blk-env-partition",
	.offset_redund = "u-boot,blk-env-offset-redundant",
	.offset = "u-boot,blk-env-offset",
};

static inline int blk_offset_try_partition(const char *str, int copy,
            struct blk_desc **dev_desc, loff_t *val)
{
	struct disk_partition info;
	struct blk_desc *desc = NULL;
	const char *dev_str = NULL;
	const char *ifname = NULL;
	const char *propname;
	loff_t defvalue, offset;
	int i, ret;

	ifname = env_blk_get_intf();
	dev_str = env_blk_get_dev_part(copy, true);
	ret = blk_get_device_by_str(ifname, dev_str, &desc);
	if (ret < 0)
		return (ret);

	for (i = 1;;i++) {
		ret = part_get_info(desc, i, &info);
		if (ret < 0)
			return ret;

		if (!strncmp((const char *)info.name, str, sizeof(info.name)))
			break;
	}

	defvalue = CONFIG_VAL(ENV_OFFSET);
	propname = dt_prop.offset;

#if defined(CONFIG_ENV_OFFSET_REDUND)
	if (copy) {
		defvalue = CONFIG_VAL(ENV_OFFSET_REDUND);
		propname = dt_prop.offset_redund;
	}
#endif
	offset = ofnode_conf_read_int(propname, defvalue);

#if defined(CONFIG_ENV_OFFSET_REDUND)
	if (copy) {
		/* if copy is set, we use the redundant environment */
		offset += roundup(CONFIG_ENV_SIZE, info.blksz);
	}
#endif

	*val = ((info.start + DIV_ROUND_UP(offset, info.blksz)) * info.blksz);
	*dev_desc = desc;

	return 0;
}

static inline loff_t env_blk_offset(int copy, struct blk_desc **desc)
{
	loff_t val = 0, defvalue;
	const char *propname;
	const char *str;
	int err;

	/* look for the partition */
#if defined(CONFIG_ENV_BLK_PARTITION_REDUND)
	if (copy) {
		str = ofnode_conf_read_str(dt_prop.partition_reund);
	} else
#else
	{
		str = ofnode_conf_read_str(dt_prop.partition);
	}
#endif
	if (str) {
		/* try to place the environment at start of the partition */
		err = blk_offset_try_partition(str, copy, desc, &val);
		if (!err)
			return val;
	}

	defvalue = CONFIG_VAL(ENV_OFFSET);
	propname = dt_prop.offset;

#if defined(CONFIG_ENV_OFFSET_REDUND)
	if (copy) {
		defvalue = CONFIG_VAL(ENV_OFFSET_REDUND);
		propname = dt_prop.offset_redund;
	}
#endif
	val = ofnode_conf_read_int(propname, defvalue);
	*desc = NULL;
	return val;
}
#else
static inline loff_t env_blk_offset(int copy, struct blk_desc **desc)
{
	loff_t offset = CONFIG_ENV_OFFSET;

#if defined(CONFIG_ENV_OFFSET_REDUND)
	if (copy)
		offset = CONFIG_ENV_OFFSET_REDUND;
#endif

	*desc = NULL;
	return offset;
}
#endif

__weak struct blk_desc *blk_get_env_addr(int copy, loff_t *env_addr)
{
	struct blk_desc *dev_desc = NULL;
	uint32_t offset;

	offset = env_blk_offset(copy, &dev_desc);
	*env_addr = offset;

	return dev_desc;
}

#if defined(CONFIG_CMD_SAVEENV) && !defined(CONFIG_SPL_BUILD)
static inline int write_env(struct blk_desc *dev_desc, lbaint_t start,
                             void *buffer, size_t size)
{
	lbaint_t blkcnt, n;
	blkcnt = ALIGN(size, dev_desc->blksz) / dev_desc->blksz;

	n = blk_dwrite(dev_desc, start, blkcnt, buffer);

	return (n == blkcnt) ? 0 : -EIO;
}

/*
 * Initializes and validates block device and partition.
 *
 * @ifname: Interface name for the environment device.
 * @dev_and_part: Device and partition string.
 * @dev_desc: Pointer to store the block device descriptor.
 * @part_info: Pointer to store the disk partition information.
 *
 * return: 0 on success, negative error code on failure.
 */
static int env_blk_init_dev_part(const char *ifname, const char *dev_and_part,
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

	if (info.size * desc->blksz < CONFIG_ENV_SIZE) {
		pr_err("0x%x (Environment Size) > 0x%lx (Partition Size)\n",
		       CONFIG_ENV_SIZE, info.size * desc->blksz);
		return -ENOSPC;
	}

	if (dev_desc && part_info) {
		*dev_desc = desc;
		memcpy(part_info, &info, sizeof(*part_info));
	}

	return 0;
}

static int env_blk_save(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(env_t, env_new, 1);
	struct blk_desc *dev_desc = NULL;
	struct disk_partition info;
	const char *ifname = NULL;
	const char *dev_part_str = NULL;
	loff_t env_addr;
	lbaint_t start;
	bool is_abs = true;
	int ret, copy = 0;

	memset(env_new, 0, sizeof(env_t));
	ret = env_export(env_new);
	if (ret) {
		debug("Could not export environment\n");
		return ret;
	}

#ifdef CONFIG_SYS_REDUNDAND_ENVIRONMENT
	if (gd->env_valid == ENV_VALID)
		copy = 1;
#endif

	dev_desc = blk_get_env_addr(copy, &env_addr);
	if (!dev_desc) {
		is_abs = false;
		ifname = env_blk_get_intf();
		dev_part_str = env_blk_get_dev_part(copy, false);
		debug("Writing environment into %s %s\n", ifname, dev_part_str);
		ret = env_blk_init_dev_part(ifname, dev_part_str,
		                       &dev_desc, &info);
		if (ret)
			return ret;
	}

	if (!IS_ALIGNED(sizeof(env_t), dev_desc->blksz)) {
		debug("0x%lx (Environment Size) is not aligned to 0x%lx (Block Size)\n",
		            sizeof(env_t), dev_desc->blksz);
		return -EINVAL;
	}

	start = is_abs ? env_addr / dev_desc->blksz :
	               info.start + (env_addr / dev_desc->blksz);

	debug("Writing environment at 0x%lx\n", start);
	ret = write_env(dev_desc, start, env_new,
	               sizeof(env_t));
	if (ret) {
		debug("Could not write environment into %s (device %d)\n",
		       ifname, dev_desc->devnum);
		return ret;
	}

#ifdef CONFIG_SYS_REDUNDAND_ENVIRONMENT
	gd->env_valid = gd->env_valid == ENV_REDUND ? ENV_VALID : ENV_REDUND;
#endif

	return 0;
}

static inline int erase_env(struct blk_desc *dev_desc, lbaint_t start,
                            size_t size)
{
	lbaint_t blkcnt, n;
	blkcnt = ALIGN(size, dev_desc->blksz) / dev_desc->blksz;

	debug("Erasing %ld blocks at 0x%lx\n", blkcnt, start);
	n = blk_derase(dev_desc, start, blkcnt);
	debug("%ld blocks erased at 0x%lx: %s\n", n, start,
	                  (n == blkcnt) ? "OK" : "ERROR");

	return (n == blkcnt) ? 0 : -EIO;
}

static int env_blk_erase(void)
{
	struct blk_desc *dev_desc = NULL;
	struct disk_partition info;
	const char *ifname = NULL;
	const char *dev_part_str = NULL;
	int copy = 0;
	loff_t env_addr;
	bool is_abs = true;
	lbaint_t start;
	int ret = 0;

#ifdef CONFIG_SYS_REDUNDAND_ENVIRONMENT
redund:
#endif
	dev_desc = blk_get_env_addr(copy, &env_addr);
	if (!dev_desc) {
		is_abs = false;
		ifname = env_blk_get_intf();
		dev_part_str = env_blk_get_dev_part(copy, false);
		debug("Erasing environment from %s %s\n", ifname, dev_part_str);
		ret = env_blk_init_dev_part(ifname, dev_part_str,
		                       &dev_desc, &info);
		if (ret)
			return ret;
	}

	if (!IS_ALIGNED(CONFIG_ENV_SIZE, dev_desc->blksz)) {
		debug("0x%x (Environment Size) is not aligned to 0x%lx (Block Size)\n",
		            CONFIG_ENV_SIZE, dev_desc->blksz);
		return -EINVAL;
	}

	start = is_abs ? env_addr / dev_desc->blksz :
	               info.start + (env_addr / dev_desc->blksz);

	ret |= erase_env(dev_desc, start, CONFIG_ENV_SIZE);
	if (ret) {
		debug("Could not erase environment from %s (device %d)\n",
		                ifname, dev_desc->devnum);
		return ret;
	}

#ifdef CONFIG_SYS_REDUNDAND_ENVIRONMENT
	if (!copy) {
		copy = 1;
		is_abs = true;
		goto redund;
	}
#endif

	return 0;
}
#endif /* CONFIG_CMD_SAVEENV && !CONFIG_SPL_BUILD */

static inline int read_env(struct blk_desc *dev_desc, lbaint_t start,
               void *buffer, unsigned long size)
{
	uint blkcnt, n;

	blkcnt = ALIGN(size, dev_desc->blksz) / dev_desc->blksz;

	n = blk_dread(dev_desc, start, blkcnt, buffer);

	return (n == blkcnt) ? 0 : -EIO;
}

#if defined(CONFIG_SYS_REDUNDAND_ENVIRONMENT)
static int env_blk_load(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(env_t, tmp_env1, 1);
	ALLOC_CACHE_ALIGN_BUFFER(env_t, tmp_env2, 1);
	struct blk_desc *dev_desc = NULL;
	struct disk_partition info;
	const char *ifname = NULL;
	const char *dev_part_str = NULL;
	loff_t env_addr;
	bool is_abs = true;
	lbaint_t start;
	int ret, copy= 0;
	int read1_fail = 0, read2_fail = 0;

	dev_desc = blk_get_env_addr(copy, &env_addr);
	if (!dev_desc) {
		is_abs = false;
		ifname = env_blk_get_intf();
		dev_part_str = env_blk_get_dev_part(copy, false);
		debug("Loading environment from %s %s\n", ifname, dev_part_str);
		ret = env_blk_init_dev_part(ifname, dev_part_str,
		                       &dev_desc, &info);
		if (ret)
			goto failed;
	}

	if (!IS_ALIGNED(CONFIG_ENV_SIZE, dev_desc->blksz)) {
		debug("0x%x (Environment Size) is not aligned to 0x%lx (Block Size)\n",
		            CONFIG_ENV_SIZE, dev_desc->blksz);
		ret = -EINVAL;
		goto failed;
	}

	start = is_abs ? env_addr / dev_desc->blksz :
	               info.start + (env_addr / dev_desc->blksz);
	read1_fail = read_env(dev_desc, start, tmp_env1, ALIGN_DOWN(CONFIG_ENV_SIZE, dev_desc->blksz));

	is_abs = true;
	copy = 1;
	dev_desc = blk_get_env_addr(copy, &env_addr);
	if (!dev_desc) {
		is_abs = false;
		ifname = env_blk_get_intf();
		dev_part_str = env_blk_get_dev_part(copy, false);
		debug("Loading environment from %s %s\n", ifname, dev_part_str);
		ret = env_blk_init_dev_part(ifname, dev_part_str,
		                       &dev_desc, &info);
		if (ret)
			goto failed;
	}

	if (!IS_ALIGNED(CONFIG_ENV_SIZE, dev_desc->blksz)) {
		debug("0x%x (Environment Size) is not aligned to 0x%lx (Block Size)\n",
		            CONFIG_ENV_SIZE, dev_desc->blksz);
		ret = -EINVAL;
		goto failed;
	}

	start = is_abs ? env_addr / dev_desc->blksz :
	               info.start + (env_addr / dev_desc->blksz);
	read2_fail = read_env(dev_desc, start, tmp_env2, ALIGN_DOWN(CONFIG_ENV_SIZE, dev_desc->blksz));

	ret = env_import_redund((char *)tmp_env1, read1_fail, (char *)tmp_env2,
				read2_fail, H_EXTERNAL);
	if (ret) {
		debug("Could not import redundant environment from %s (device %d)\n",
		       ifname, dev_desc->devnum);
		goto exit;
	}

failed:
	if (ret)
		env_set_default("load failure", 0);

exit:
	return ret;
}
#else
static int env_blk_load(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(char, buffer, CONFIG_ENV_SIZE);
	struct blk_desc *dev_desc = NULL;
	struct disk_partition info;
	const char *ifname = NULL;
	const char *dev_part_str = NULL;
	int copy = 0;
	loff_t env_addr;
	bool is_abs = true;
	lbaint_t start;
	int ret;

	dev_desc = blk_get_env_addr(copy, &env_addr);
	if (!dev_desc) {
		is_abs = false;
		ifname = env_blk_get_intf();
		dev_part_str = env_blk_get_dev_part(copy, false);
		debug("Loading environment from %s %s\n", ifname, dev_part_str);
		ret = env_blk_init_dev_part(ifname, dev_part_str,
		                       &dev_desc, &info);
		if (ret)
			goto failed;
	}

	if (!IS_ALIGNED(CONFIG_ENV_SIZE, dev_desc->blksz)) {
		debug("0x%x (Environment Size) is not aligned to 0x%lx (Block Size)\n",
		            CONFIG_ENV_SIZE, dev_desc->blksz);
		ret = -EINVAL;
		goto failed;
	}

	start = is_abs ? env_addr / dev_desc->blksz :
	               info.start + (env_addr / dev_desc->blksz);

	ret = read_env(dev_desc, start, buffer, CONFIG_ENV_SIZE);
	if (ret) {
		debug("Could not read environment from %s (device %d)\n",
		       ifname, dev_desc->devnum);
		goto failed;
	}

	ret = env_import(buffer, true, H_EXTERNAL);
	if (ret) {
		debug("Could not import environment from %s (device %d)\n",
		       ifname, dev_desc->devnum);
		goto exit;
	}

failed:
	if (ret)
		env_set_default("load failure", 0);

exit:
	return ret;
}
#endif /* !CONFIG_SYS_REDUNDAND_ENVIRONMENT */

U_BOOT_ENV_LOCATION(blk) = {
	.location    = ENVL_BLK,
	.name        = "BLK",
	.load        = env_blk_load,
#ifndef CONFIG_SPL_BUILD
	.save        = env_save_ptr(env_blk_save),
	.erase       = ENV_ERASE_PTR(env_blk_erase)
#endif
};
