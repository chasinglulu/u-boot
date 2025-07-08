/*
 * SPDX-License-Identifier: GPL-2.0+
 *
 * Rootfs dm-verity support
 *
 * Copyright (C) 2025 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <log.h>
#include <part.h>
#include <errno.h>
#include <env.h>
#include <linux/err.h>
#include <malloc.h>
#include <memalign.h>
#include <u-boot/rsa.h>
#include <linux/sizes.h>
#include <fs.h>
#include <ubi_uboot.h>
#include <ext4fs.h>
#include <squashfs.h>

#include <asm/global_data.h>
#include <asm/arch/bootparams.h>

DECLARE_GLOBAL_DATA_PTR;

/*
  verity_header format: (total size 4096)

  0                4                8               12                16
  +----------------+----------------+----------------+----------------+ 0
  |      magic     |    version     | root_hash_ofs  | root_hash_size |
  +----------------+----------------+----------------+----------------+ 16
  |  signature_ofs | signature_size | hash_tree_ofs  | hash_tree_size |
  +----------------+----------------+----------------+----------------+ 32
  |                              reserved                             |
  +-------------------------------------------------------------------+ 128
  |                                                                   |
  |                              root hash                            |
  |                                                                   |
  +-------------------------------------------------------------------+
  |                                                                   |
  |                              signature                            |
  |                                                                   |
  +-------------------------------------------------------------------+
  |                              reserved1                            |
  +-------------------------------------------------------------------+ 4096
  |                                                                   |
  |                              hash tree                            |
  |                                                                   |
  +-------------------------------------------------------------------+
*/

struct dm_verity_header {
	uint32_t magic;
	uint32_t version;
	uint32_t root_hash_ofs;
	uint32_t root_hash_size;
	uint32_t signature_ofs;
	uint32_t signature_size;
	uint32_t hash_tree_ofs;
	uint32_t hash_tree_size;
};

#define DM_VERITY_INFO_SIZE        (1 << 12)
#define ROOTFS_VERITY_MAGIC_NUMBER (0xb001b001)

/**
 * Parse the value corresponding to the key from the verity header information.
 * Supports key with or without colon, and skips whitespace.
 * @param roothash: input roothash string
 * @param key:      key to search for
 * @param value:    output buffer for value
 * @param value_len:buffer length
 * @return 0 on success, <0 on failure
 */
static int get_verity_value(const char *verity_header, const char *key,
                            char *value, size_t value_len)
{
	size_t key_len, len;
	const char *p,*q;
	const char *val_start;

	if (!verity_header || !key || !value || value_len == 0)
		return -EINVAL;

	key_len = strlen(key);
	p = verity_header;

	while (*p) {
		// Check if this line starts with the key
		if (strncmp(p, key, key_len) == 0) {
			q = p + key_len;
			// Skip colon, spaces, or tabs after key
			while (*q == ':' || *q == ' ' || *q == '\t')
				q++;

			val_start = q;

			while (*q && *q != '\n')
				q++;
			len = q - val_start;
			if (len >= value_len)
				len = value_len - 1;
			strncpy(value, val_start, len);
			value[len] = '\0';
			return 0;
		}
		// Move to the next line
		while (*p && *p != '\n')
			p++;
		if (*p == '\n')
			p++;
	}
	return -ENOENT;
}

static int process_dm_verity_bootargs(const char *verity_header)
{
	char verity_args[CONFIG_SYS_CBSIZE] = { 0 };
	char verity_table[CONFIG_SYS_CBSIZE] = { 0 };
	const char *verity_name = "rootfs-verity";
	const char *verity_flags = "ro";
	const char *verity_minor = "";
	const char *root= NULL;
	char value[SZ_256] = { 0 };
	char devname[PART_NAME_LEN];
	int data_blocks = 0, data_block_size = 0;
	int hash_block_size = 0;
	char uuid[SZ_128] = { 0 };
	char hash_algo[SZ_64] = { 0 };
	char root_hash[SZ_256] = { 0 };
	char salt[SZ_256] = { 0 };
	int sectors_num, start_sector;
	int hash_start_block;
	int sectors_per_block;
	int ret, i;
	struct {
		const char *key;
		char *buf;
		size_t bufsize;
		int *intval;
	} fields[] = {
		{ "UUID", uuid, sizeof(uuid), NULL },
		{ "Data blocks", NULL, -1, &data_blocks },
		{ "Data block size", NULL, -1, &data_block_size },
		{ "Hash block size", NULL, -1, &hash_block_size },
		{ "Hash algorithm", hash_algo, sizeof(hash_algo), NULL },
		{ "Root hash", root_hash, sizeof(root_hash), NULL },
		{ "Salt", salt, sizeof(salt), NULL },
	};

	if (!verity_header) {
		pr_err("Invalid verity header information.\n");
		return -EINVAL;
	}

	root = env_get("root");
	if (!root) {
		debug("'root' environment variable not found\n");
		return -ENODATA;
	}
	strncpy(devname, root, sizeof(devname));
	devname[sizeof(devname) - 1] = '\0';

	// Check if devname is /dev/mmcblk* or /dev/mtdblock*
	if (strncmp(devname, "/dev/mmcblk", 11) &&
	      strncmp(devname, "/dev/mtdblock", 13)) {
		pr_err("'%s' is not supported for dm verity\n", devname);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(fields); i++) {
		memset(value, 0, sizeof(value));
		ret = get_verity_value(verity_header, fields[i].key, value,
		                           sizeof(value));
		if (ret) {
			pr_err("'%s' field not found in VERITY header\n", fields[i].key);
			return ret;
		}
		if (fields[i].buf) {
			strncpy(fields[i].buf, value, fields[i].bufsize);
			fields[i].buf[fields[i].bufsize - 1] = '\0';
			debug("Found '%s': '%s'\n", fields[i].key, fields[i].buf);
		}
		if (fields[i].intval) {
			*(fields[i].intval) = simple_strtoul(value, NULL, 10);
			debug("Found '%s': %u\n", fields[i].key, *(fields[i].intval));
		}
	}
	sectors_per_block = data_block_size / SZ_512;

	strcat(verity_args, "dm-mod.create=\"");
	strcat(verity_args, verity_name);
	strcat(verity_args, ",");
	strcat(verity_args, uuid);
	strcat(verity_args, ",");
	strcat(verity_args, verity_minor);
	strcat(verity_args, ",");
	strcat(verity_args, verity_flags);
	strcat(verity_args, ",");

	start_sector = 0;
	sectors_num = data_blocks * sectors_per_block;

	/*
	 * The hash tree starts after the data blocks. We add 2 blocks to skip
	 * the verity superblock and the verity header by default.
	 * verity superblock format:
	 * https://gitlab.com/cryptsetup/cryptsetup/-/wikis/DMVerity#verity-superblock-format
	 */
	hash_start_block = data_blocks + 2;

	snprintf(verity_table, sizeof(verity_table),
	              "%u %u verity 1 %s %s %u %u %u %u %s %s %s",
	              start_sector, sectors_num, devname, devname,
	              data_block_size, hash_block_size, data_blocks,
	              hash_start_block, hash_algo, root_hash,
	              strlen(salt) ? salt : "-");
	strcat(verity_args, verity_table);
	strcat(verity_args, "\"");

	strcat(verity_args, " dm-mod.waitfor=");
	strcat(verity_args, devname);
	debug("DM verity args: %s\n", verity_args);

	env_set("dm_verity_args", verity_args);
	env_set("root", "/dev/dm-0");
	return 0;
}

static size_t get_rootfs_img_size(void)
{
	struct fs_size {
		const char *name;
		int (*get_fssize)(loff_t *size);
	} fssizes[] = {
#if CONFIG_IS_ENABLED(FS_EXT4)
		{
			.name = "ext4",
			.get_fssize = ext4fs_fssize,
		},
#endif
#if CONFIG_IS_ENABLED(FS_SQUASHFS)
		{
			.name = "squashfs",
			.get_fssize = sqfs_fssize,
		},
#endif
		{
			.name = "unsupported",
			.get_fssize = NULL,
		},
	};
	size_t fssize = 0;
	const char *rootfstype = NULL;
	int i, ret;

	rootfstype = env_get("rootfstype");
	if (!rootfstype) {
		debug("Environment variable 'rootfstype' not found\n");
		return 0;
	}

	for (i = 0; fssizes[i].get_fssize; i++) {
		if (strcmp(fssizes[i].name, rootfstype) == 0) {
			loff_t size = 0;

			ret = fssizes[i].get_fssize(&size);
			if (ret < 0) {
				debug("Unable to get size for '%s' filesystem (ret = %d)\n",
				      fssizes[i].name, ret);
				return 0;
			}
			fssize = size;
			break;
		}
	}
	return fssize;
}

int verify_rootfs_dm_verity(void)
{
	struct dm_verity_header *header = NULL;
	struct disk_partition part_info = { 0 };
	struct image_sign_info info = { 0 };
	struct image_region region = { 0 };
	struct blk_desc *dev_desc = NULL;
	const char *varname = NULL;
	const char *devtype = NULL;
	const char *partname = NULL;
	ulong devnum, blkcnt;
	size_t fssize = 0, size;
	lbaint_t offset = 0;
	char *buffer = NULL;
	char *hash_algo, *rsa_algo;
	char verify_algo[SZ_64] = { 0 };
	int part, ret;

	varname = env_get_name(ENV_DEVNUM);
	devnum = env_get_ulong(varname, 10, ~0UL);
	if (devnum == ~0UL) {
		debug("'%s' environment variable not found\n", varname);
		return -ENODATA;
	}

	varname = env_get_name(ENV_DEVTYPE);
	devtype = env_get(varname);
	if (unlikely(!devtype)) {
		debug("'%s' environment variable not found\n", varname);
		return -ENODATA;
	}

	dev_desc = blk_get_dev(devtype, devnum);
	if (IS_ERR_OR_NULL(dev_desc)) {
		debug("Unable to get '%s %lu' block device\n",
		               devtype, devnum);
		return -ENODEV;
	}

	partname = get_rootfs_part_name();
	if (unlikely(!partname)) {
		debug("Unable to get rootfs partition name\n");
		return -ENODATA;
	}
	part = part_get_info_by_name(dev_desc, partname, &part_info);
	if (part < 0) {
		debug("Unable to get '%s' partition information on '%s' device\n",
		           partname, devtype);
		return part;
	}

	fssize = get_rootfs_img_size();
	if (fssize == 0) {
		debug("Unable to get rootfs image size\n");
		return -EINVAL;
	}
	debug("Rootfs image size: %zu bytes\n", fssize);

	size = roundup(DM_VERITY_INFO_SIZE, part_info.blksz);
	buffer = malloc(size);
	if (!buffer) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}

	blkcnt = DIV_ROUND_UP(DM_VERITY_INFO_SIZE, part_info.blksz);
	offset = DIV_ROUND_UP(fssize, part_info.blksz);
	debug("%s: offset = %lu, blkcnt = %lu, size = %zu\n",
	                __func__, offset, blkcnt, size);
	ret = blk_dread(dev_desc, part_info.start + offset, blkcnt, buffer);
	if (ret != blkcnt) {
		debug("Unable to read rootfs verity header (ret = %d\n)", ret);
		ret = -EIO;
		goto failed;
	}

	header = (struct dm_verity_header *)buffer;
	if (ROOTFS_VERITY_MAGIC_NUMBER != header->magic) {
		pr_err("rootfs roothash magic '0x%.08x' not match '0x%.08x'\n",
		               header->magic, ROOTFS_VERITY_MAGIC_NUMBER);
		ret = -EFAULT;
		goto failed;
	}

#ifdef CONFIG_LUA_SHA512
	hash_algo = "sha512";
#endif
#ifdef CONFIG_LUA_SHA384
	hash_algo = "sha384";
#endif
#ifdef CONFIG_LUA_SHA256
	hash_algo = "sha256";
#endif

#if defined (CONFIG_LUA_RSA2048)
	info.keyname = "akcipher2048";
	rsa_algo = "rsa2048";
#elif defined(CONFIG_LUA_RSA3072)
	info.keyname = "akcipher3072";
	rsa_algo = "rsa3072";
#endif

	snprintf(verify_algo, sizeof(verify_algo), "%s,%s", hash_algo, rsa_algo);
	info.name = verify_algo;
	info.padding = image_get_padding_algo("pkcs-1.5");
	info.checksum = image_get_checksum_algo(verify_algo);
	info.crypto = image_get_crypto_algo(info.name);
	info.fdt_blob = gd->fdt_blob;
	info.required_keynode = -1;
	region.data = buffer + header->root_hash_ofs;
	region.size = header->root_hash_size;
	ret = rsa_verify(&info, &region, 1, buffer + header->signature_ofs, header->signature_size);
	if (ret < 0) {
		pr_err("Unable to verify rootfs roothash signature (ret = %d)\n", ret);
		goto failed;
	}

	ret = process_dm_verity_bootargs(buffer + header->root_hash_ofs);
	if (ret) {
		pr_err("Could not set dm verity bootargs (ret = %d)\n", ret);
		goto failed;
	}

failed:
	free(buffer);
	return ret;
}