/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2023 Horizon Robotics Co,. ltd.
 */

#ifndef _FB_MTD_H_
#define _FB_MTD_H_

#include <jffs2/load_kernel.h>

/**
 * fastboot_mtd_get_part_info() - Lookup MTD partition by name
 *
 * @part_name: Named device to lookup
 * @part_info: Pointer to returned part_info pointer
 * @response: Pointer to fastboot response buffer
 */
int fastboot_mtd_get_part_info(const char *part_name,
				struct part_info **part_info, char *response);

/**
 * fastboot_mtd_flash_write() - Write image to MTD for fastboot
 *
 * @cmd: Named partition to write image to
 * @download_buffer: Pointer to image data
 * @download_bytes: Size of image data
 * @response: Pointer to fastboot response buffer
 */
void fastboot_mtd_flash_write(const char *cmd, void *download_buffer,
			      u32 download_bytes, char *response);
/**
 * fastboot_mtd_flash_erase() - Erase MTD for fastboot
 *
 * @cmd: Named partition to erase
 * @response: Pointer to fastboot response buffer
 */
void fastboot_mtd_erase(const char *cmd, char *response);
#endif


