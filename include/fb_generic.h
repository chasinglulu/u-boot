/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2023 Horizon Robotics Co,. ltd.
 */

#ifndef _FB_GENERIC_H_
#define _FB_GENERIC_H_

struct blk_desc;
struct disk_partition;

/**
 * fastboot_generic_get_part_info() - Lookup Block Device partion by name
 *
 * @part_name: Named partition to lookup
 * @dev_desc: Pointer to returned blk_desc pointer
 * @part_info: Pointer to returned struct disk_partition
 * @response: Pointer to fastboot response buffer
 */
int fastboot_generic_get_part_info(const char *part_name,
			       struct blk_desc **dev_desc,
			       struct disk_partition *part_info,
			       char *response);

/**
 * fastboot_generic_flash_write() - Write image to Block Device for fastboot
 *
 * @cmd: Named partition to write image to
 * @download_buffer: Pointer to image data
 * @download_bytes: Size of image data
 * @response: Pointer to fastboot response buffer
 */
void fastboot_generic_flash_write(const char *cmd, void *download_buffer,
			      u32 download_bytes, char *response);
/**
 * fastboot_generic_flash_erase() - Erase Block Device for fastboot
 *
 * @cmd: Named partition to erase
 * @response: Pointer to fastboot response buffer
 */
void fastboot_generic_erase(const char *cmd, char *response);
#endif
