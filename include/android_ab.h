/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2017 The Android Open Source Project
 */

#ifndef __ANDROID_AB_H
#define __ANDROID_AB_H

/*
 * Identifiers for the AB slot marking.
 */
enum ab_slot_mark {
	AB_MARK_SUCCESSFUL,
	AB_MARK_UNBOOTABLE,
	AB_MARK_ACTIVE
};

struct blk_desc;
struct disk_partition;

/* Android standard boot slot names are 'a', 'b', 'c', ... */
#define BOOT_SLOT_NAME(slot_num) ('a' + (slot_num))
/* printing boot slot names are 'A', 'B', 'C', ... */
#define SLOT_NAME(slot_num) ('A' + (slot_num))

/* Number of slots */
#define NUM_SLOTS 2

#include <android_bootloader_message.h>

int ab_control_create_from_disk(struct blk_desc *dev_desc,
				       const struct disk_partition *part_info,
				       struct bootloader_control **abc);
int ab_control_create_from_buffer(struct bootloader_message_ab *buffer,
				     struct bootloader_control *abc);
int ab_control_store(struct blk_desc *dev_desc,
			    const struct disk_partition *part_info,
			    struct bootloader_control *abc);
int ab_control_store_into_buffer(struct bootloader_message_ab *buffer,
				      struct bootloader_control *abc);

/**
 * Select the slot where to boot from.
 *
 * On Android devices with more than one boot slot (multiple copies of the
 * kernel and system images) selects which slot should be used to boot from and
 * registers the boot attempt. This is used in by the new A/B update model where
 * one slot is updated in the background while running from the other slot. If
 * the selected slot did not successfully boot in the past, a boot attempt is
 * registered before returning from this function so it isn't selected
 * indefinitely.
 *
 * @param[in] dev_desc Place to store the device description pointer
 * @param[in] part_info Place to store the partition information
 * Return: The slot number (>= 0) on success, or a negative on error
 */
int ab_select_slot(struct blk_desc *dev_desc, struct disk_partition *part_info);

/**
 * Select the slot where to boot from using a buffer.
 *
 * This function is similar to ab_select_slot(), but it loads the A/B metadata
 * from storage into a temporary buffer and operates on that. It selects which
 * slot should be used to boot from and registers the boot attempt. This is
 * used by the new A/B update model where one slot is updated in the
 * background while running from the other slot. If the selected slot did not
 * successfully boot in the past, a boot attempt is registered before returning
 * from this function so it isn't selected indefinitely.
 *
 * The A/B metadata is loaded from and stored back to the default bootloader
 * message partition.
 *
 * @return The slot number (>= 0) on success, or a negative value on error.
 */
int ab_select_slot_from_buffer(void);

/**
 * Mark the slot as successfully booted.
 *
 * @param[in] slot The slot number to mark as successfully booted
 *
 * Return: 0 on success, or a negative on error
 */
int ab_control_mark_successful(int slot);

/**
 * Mark the slot as unbootable.
 *
 * @param[in] slot The slot number to mark as unbootable
 *
 * Return: 0 on success, or a negative on error
 */
int ab_control_mark_unbootable(int slot);

/**
 * Compare two slots
 */
int ab_control_compare_slots(void);

#endif /* __ANDROID_AB_H */
