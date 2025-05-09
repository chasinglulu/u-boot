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

#ifdef CONFIG_MTD_BLOCK_BBS
ulong mtd_blk_read_bbs(struct blk_desc *dev_desc, lbaint_t part_start, lbaint_t part_size,
                       lbaint_t from, lbaint_t blkcnt, void *dst);
ulong mtd_blk_write_bbs(struct blk_desc *dev_desc, lbaint_t part_start, lbaint_t part_size,
                         lbaint_t from, lbaint_t blkcnt, const void *src);
#else
static inline ulong
mtd_blk_read_bbs(struct blk_desc *dev_desc, lbaint_t part_start, lbaint_t part_size,
                     lbaint_t from, lbaint_t blkcnt, void *dst)
{
	return -ENOTSUPP;
}

static inline ulong
mtd_blk_write_bbs(struct blk_desc *dev_desc, lbaint_t part_start, lbaint_t part_size,
                     lbaint_t from, lbaint_t blkcnt, const void *src)
{
	return -ENOTSUPP;
}
#endif

#endif /* DISK_MTDPARTS_H_ */