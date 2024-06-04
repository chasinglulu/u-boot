/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <mtd.h>
#include <linux/string.h>

#define MTDPARTS_LEN    256
#define MTDIDS_LEN      128

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

	if (mtd_initialized) {
		*mtdids = ids;
		*mtdparts = parts;
		return;
	}

	memset(parts, 0, sizeof(parts));
	memset(ids, 0, sizeof(ids));

	/* probe all SPI Flash devices */
	uclass_foreach_dev_probe(UCLASS_SPI_FLASH, dev) {
		debug("SPI Flash device = %s\n", dev->name);
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

	*mtdids = ids;
	*mtdparts = parts;
	debug("%s:\nmtdids   : %s\nmtdparts : %s\n", __func__, ids, parts);
}
