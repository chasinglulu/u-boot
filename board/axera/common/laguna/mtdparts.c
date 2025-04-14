/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <mtd.h>
#include <env.h>
#include <hexdump.h>

#include <asm/arch/bootparams.h>
#include <linux/string.h>
#include <linux/mtd/mtdparts.h>

#define MTDPARTS_LEN    256
#define MTDIDS_LEN      128

#if defined(CONFIG_LUA_MTDPARTS_READ)
loff_t board_mtdparts_offset(uint8_t mtdtype, bool backup)
{
	loff_t offset = 0;
	size_t sbl, metadata;

	switch (mtdtype) {
	case MTD_NORFLASH:
		sbl = CONFIG_LUA_SBL_SIZE_IN_NORFLASH;
		metadata = CONFIG_LUA_METADATA_SIZE_IN_NORFLASH;
		break;
	case MTD_NANDFLASH:
	case MTD_MLCNANDFLASH:
		sbl = CONFIG_LUA_SBL_SIZE_IN_NANDFLASH;
		metadata = CONFIG_LUA_METADATA_SIZE_IN_NANDFLASH;
		break;
	default:
		pr_err("Not supported MTD type\n");
		return -EAGAIN;
	}

	offset = sbl;
#ifdef CONFIG_LUA_AB
	/* A/B slot */
	offset <<= 1;
#endif
	if (backup)
		offset += metadata;

	return offset;
}
#endif

static __maybe_unused
struct blk_desc *find_vaild_mtdblock_device(void)
{
	struct blk_desc *blk_desc;
	ulong main_mtd;
	ulong safe_mtd;
	int bootdev;

	bootdev = get_bootdevice(NULL);
	if (bootdev < 0) {
		debug("%s: Invaild boot device\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	switch (bootdev) {
	case BOOTDEVICE_ONLY_NAND:
		main_mtd = env_get_ulong(env_get_name(ENV_MAIN_MTD), 10, ~0UL);
		if (unlikely(main_mtd == ~0UL))
			main_mtd = CONFIG_FDL_FLASH_NAND_MTD_DEV;

		blk_desc = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(blk_desc))
			return ERR_PTR(-ENXIO);
		break;
	case BOOTDEVICE_BOTH_NOR_NAND:
		/* mtdparts string written into safety SPI nor flash */
		safe_mtd = env_get_ulong(env_get_name(ENV_SAFE_MTD), 10, ~0UL);
		if (unlikely(safe_mtd == ~0UL))
			safe_mtd = CONFIG_FDL_FLASH_NOR_MTD_DEV;

		blk_desc = get_blk_by_devnum(IF_TYPE_MTD, safe_mtd);
		if (IS_ERR_OR_NULL(blk_desc))
			return ERR_PTR(-ENXIO);
		break;
	case BOOTDEVICE_BOTH_NOR_EMMC:
		safe_mtd = env_get_ulong(env_get_name(ENV_SAFE_MTD), 10, ~0UL);
		if (unlikely(safe_mtd == ~0UL))
			safe_mtd = CONFIG_FDL_FLASH_NOR_MTD_DEV;

		blk_desc = get_blk_by_devnum(IF_TYPE_MTD, safe_mtd);
		if (IS_ERR_OR_NULL(blk_desc))
			return ERR_PTR(-ENXIO);
		break;
	default:
		debug("%s: Not supported boot device\n", __func__);
		return ERR_PTR(-EBUSY);
	}

	return blk_desc;
}

static __maybe_unused
int mtdpart_name_check(const char *name, const char **part_id, int len)
{
	int i;
	int slen1, slen2;

	slen1 = strlen(name);
	for (i = 0; i < len; i++) {
		slen2 = strlen(part_id[i]);
		if (slen1 != slen2 ||
		    strncmp(part_id[i], name, slen1))
			continue;
		else
			return 0;
	}

	return -EINVAL;
}

static __maybe_unused
int splitup_mtdparts(char *full_mtdparts, char *sub_mtdparts)
{
	struct mtd_info *mtd = NULL;
	struct blk_desc *blk_desc;
	char *p, *comma, *bracket;
	char *safe_mtdparts;
	char *main_mtdparts;
	int safe_mtdparts_len;
	int main_mtdparts_len;
	char *name = NULL;
	ulong main_mtd;
	int mtdpart_len;
	int bootdev;
	int name_len;
	char buf[32];

	if (!full_mtdparts || !sub_mtdparts)
		return -EINVAL;

	bootdev = get_bootdevice(NULL);
	if (bootdev < 0) {
		debug("Invaild boot device\n");
		return -EINVAL;
	}

	switch (bootdev) {
	case BOOTDEVICE_BOTH_NOR_NAND:
		/* Find Main SPI Nand flash */
		main_mtd = env_get_ulong("main_mtd", 10, ~0UL);
		if (unlikely(main_mtd == ~0UL))
			main_mtd = CONFIG_FDL_FLASH_NAND_MTD_DEV;

		blk_desc = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(blk_desc))
			return -ENXIO;
		break;
	default:
		return -EBUSY;
	}
	mtd = blk_desc_to_mtd(blk_desc);

	p = full_mtdparts;
	while ((comma = strchr(p, ',')) != NULL) {
		mtdpart_len = comma - p + 1;
		bracket = strchr(p, '(');
		name = ++bracket;
		bracket = strchr(name, ')');
		name_len = bracket - name;
		memcpy(buf, name, name_len);
		buf[name_len] = '\0';

		if (mtdpart_name_check(buf, safe_part_id,
		             get_safe_part_id_count()))
			break;

		p += mtdpart_len;
	}
	safe_mtdparts_len = p - full_mtdparts;
	main_mtdparts_len = strlen(full_mtdparts) - safe_mtdparts_len;

	safe_mtdparts = full_mtdparts;
	main_mtdparts = sub_mtdparts;

	strncat(main_mtdparts, mtd->name, strlen(mtd->name));
	strncat(main_mtdparts, ":", MTDPARTS_LEN);
	strncat(main_mtdparts, full_mtdparts + safe_mtdparts_len, MTDPARTS_LEN);
	debug("main mtdparts: %s\n", main_mtdparts);

	full_mtdparts[safe_mtdparts_len - 1] = '\0';
	debug("safe mtdparts: %s\n", safe_mtdparts);

	return 0;
}

static __maybe_unused
void board_get_mtdparts(const char *dev, const char *partition,
                          char *mtdids, char *mtdparts)
{
	/* mtdids: "<dev>=<dev>, ...." */
	if (mtdids[0] != '\0')
		strcat(mtdids, ",");
	strcat(mtdids, dev);
	strcat(mtdids, "=");
	strcat(mtdids, dev);

	/* mtdparts: "mtdparts=<dev>:<mtdparts_<dev>>;..." */
	if (mtdparts[0] != '\0')
		strncat(mtdparts, ";", MTDPARTS_LEN);
	else
		strcat(mtdparts, "mtdparts=");

	strncat(mtdparts, dev, MTDPARTS_LEN);
	strncat(mtdparts, ":", MTDPARTS_LEN);
	strncat(mtdparts, partition, MTDPARTS_LEN);
}

void board_mtdparts_default(const char **mtdids, const char **mtdparts)
{
	struct mtd_info __maybe_unused *mtd;
	struct udevice __maybe_unused *dev;
	const char __maybe_unused *mtd_parts;
	const char __maybe_unused *mtd_name;
	static char parts[3 * MTDPARTS_LEN + 1];
	static char ids[MTDIDS_LEN + 1];
	static bool mtd_initialized;
	struct blk_desc __maybe_unused *dev_desc;
	char __maybe_unused *full_mtdparts;
	char __maybe_unused sub_mtdparts[MTDPARTS_LEN];
	char __maybe_unused *hashtag = NULL;
	int __maybe_unused prefix_len = 0;
	int __maybe_unused ret;

	if (mtd_initialized) {
		*mtdids = ids;
		*mtdparts = parts;
		return;
	}

	memset(parts, 0, sizeof(parts));
	memset(ids, 0, sizeof(ids));

#if defined(CONFIG_LUA_MTDPARTS_READ)
	/* probe all MTD devices */
	uclass_foreach_dev_probe(UCLASS_MTD, dev)
		debug("MTD device: %s\n", dev->name);

	dev_desc = find_vaild_mtdblock_device();
	if (IS_ERR_OR_NULL(dev_desc)) {
		debug("Unable to find vaild MTD block device\n");
		return;
	}
	debug("MTD block device: %s\n", dev_desc->bdev->name);
	mtd = blk_desc_to_mtd(dev_desc);

	ret = read_full_mtdparts(dev_desc, &full_mtdparts);
	if (ret < 0) {
		debug("Failed to read mtdparts from mtd block device\n");
		return;
	}
	debug("full mtdparts: %s\n", full_mtdparts);
	hashtag = strchr(full_mtdparts, '#');
	if (hashtag)
		prefix_len = hashtag - full_mtdparts + 1;

	memset(sub_mtdparts, 0, sizeof(sub_mtdparts));
	ret = splitup_mtdparts(full_mtdparts, sub_mtdparts);
	if (!ret) {
		char *p;
		p = strchr(sub_mtdparts, ':');
		*p++ = '\0';
		board_get_mtdparts(sub_mtdparts, p, ids, parts);
	}

	if (!IS_ERR_OR_NULL(mtd) && strlen(full_mtdparts)) {
		board_get_mtdparts(mtd->name, full_mtdparts + prefix_len, ids, parts);
		mtd_initialized = true;
	}

	if (full_mtdparts)
		free(full_mtdparts);
#endif

#if defined(CONFIG_LUA_SPI_NOR_RUNTIME)
	/* probe all SPI NOR Flash devices */
	uclass_foreach_dev_probe(UCLASS_SPI_FLASH, dev) {
		debug("SPI NOR Flash device = %s\n", dev->name);
	}

#if defined(CONFIG_LUA_NOR0)
	mtd_parts = CONFIG_LUA_MTDPARTS_NOR0;

#if CONFIG_IS_ENABLED(SPI_FLASH_TINY)
	mtd_name = "spi-flash";
#else
	mtd_name = "nor0";
#endif

	mtd = get_mtd_device_nm(mtd_name);
	if (!IS_ERR_OR_NULL(mtd) && strlen(mtd_parts)) {
		board_get_mtdparts(mtd_name, mtd_parts, ids, parts);
		put_mtd_device(mtd);
		mtd_initialized = true;
	}
#endif

#if defined(CONFIG_LUA_NOR1)
	mtd_parts = CONFIG_LUA_MTDPARTS_NOR1;
	mtd_name = "nor1";
	mtd = get_mtd_device_nm(mtd_name);
	if (!IS_ERR_OR_NULL(mtd) && strlen(mtd_parts)) {
		board_get_mtdparts(mtd_name, mtd_parts, ids, parts);
		put_mtd_device(mtd);
		mtd_initialized = true;
	}
#endif
#endif /* CONFIG_LUA_SPI_NOR_RUNTIME */

#if defined(CONFIG_LUA_SPI_NAND_RUNTIME)
	/* probe all SPI NAND Flash devices */
	uclass_foreach_dev_probe(UCLASS_MTD, dev)
		debug("SPI NAND Flash device = %s\n", dev->name);

#if defined(CONFIG_LUA_NAND0)
	mtd_parts = CONFIG_LUA_MTDPARTS_NAND0;

#if CONFIG_IS_ENABLED(CMD_NAND) && CONFIG_IS_ENABLED(MTD_RAW_NAND)
	mtd_name = "nand0";
#else
	mtd_name = "spi-nand0";
#endif
	mtd = get_mtd_device_nm(mtd_name);
	if (!IS_ERR_OR_NULL(mtd) && strlen(mtd_parts)) {
		board_get_mtdparts(mtd_name, mtd_parts, ids, parts);
		put_mtd_device(mtd);
		mtd_initialized = true;
	}
#endif
#endif /* CONFIG_LUA_SPI_NAND_RUNTIME */

	*mtdids = ids;
	*mtdparts = parts;
	debug("%s:\nmtdids   : %s\nmtdparts : %s\n", __func__, ids, parts);
}
