// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef DISK_MTDPARTS_H_
#define DISK_MTDPARTS_H_

#include <linux/compiler.h>
#include <blk.h>
#include <part.h>

#define MTDPARTS_SIGNATURE_UBOOT 0x535452415044544DULL     // 'MTDPARTS'
#define MTDPARTS_REVISION_V1     0x00010000

#define MTDPART_NAME_LEN                 (SZ_32)
#define MTDPARTS_SIZE                    (SZ_1K)
#define MTDPARTS_BASE                    (SZ_128K - MTDPARTS_SIZE)
#define MTDPARTS_BAKEUP_NORFLASH_BASE    (SZ_128K + MTDPARTS_BASE)
#define MTDPARTS_BAKEUP_NANDFLASH_BASE   (SZ_512K + SZ_256K + MTDPARTS_BASE)

typedef struct {
	u8 b[16];
} mtdparts_guid_t __attribute__((aligned(8)));

typedef struct mtd_parts {
	__le64 signature;
	__le32 revision;
	__le32 header_size;
	__le32 header_crc32;
	__le64 my_lba;
	__le32 my_offset;
	__le64 alternate_lba;
	__le32 alternate_offset;
	mtdparts_guid_t disk_guid;
	__le32 total_size;
	__le32 sizeof_mtdparts;
	__le32 mtdparts_crc32;
	char mtdparts_str[];
} __packed mtdparts_t;

int write_mtdparts(struct blk_desc *dev_desc, mtdparts_t *mpr);
int read_full_mtdparts(struct blk_desc *dev_desc, char **mtdparts);
int mtdparts_restore(struct blk_desc *dev_desc, const char *str_disk_guid,
                       const char *prefix,
                       struct disk_partition *parts, int part_count);
int mtdparts_verify(struct blk_desc *dev_desc, mtdparts_t *mpr);

#endif