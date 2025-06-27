/* SPDX-License-Identifier: GPL-2.0+
 *
 * Common board-specific code
 *
 * Copyright (C) 2025 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <misc.h>
#include <android_ab.h>
#include <asm/arch/bootparams.h>

/*
 * Safety ABC register layout:
 * Bit 31-3  Reserved
 * Bit 3-2   Safety booting times, 0/1 normal booting, 2/3 enter download mode
 * Bit 1     Safety booting status, 0 for normal, 1 for safety abort
 * Bit 0     Safety AB slot, 0 for slot A, 1 for slot B
 *
 */

static int safety_abc_rw(bool read, void *buf, int size)
{
	const char *dev_name = "safety-abc-syscon";
	struct udevice *dev;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_MISC, dev_name, &dev);
	if (ret) {
		pr_err("Unable to get '%s' device (ret = %d)\n", dev_name, ret);
		return ret;
	}

	if (read)
		return misc_read(dev, 0, buf, size);
	else
		return misc_write(dev, 0, buf, size);
}

static int safety_abc_read(uint32_t *val)
{
	int ret;

	if (!val) {
		pr_err("Invaild argument\n");
		return -EINVAL;
	}

	ret = safety_abc_rw(true, val, sizeof(*val));
	if (ret < 0) {
		pr_err("Unable to read safety abc value (ret = %d)\n", ret);
		return ret;
	}

	debug("%s: safety abc value = 0x%x\n", __func__, *val);
	return 0;
}

static int safety_abc_write(uint32_t *val)
{
	int ret;

	if (!val) {
		pr_err("Invaild argument\n");
		return -EINVAL;
	}

	ret = safety_abc_rw(false, val, sizeof(*val));
	if (ret < 0) {
		pr_err("Unable to write safety abc value\n");
		return ret;
	}

	debug("%s: safety abc value = 0x%x\n", __func__, *val);
	return 0;
}

static inline int get_safety_abc_slot(uint32_t val)
{
	int slot;

	slot = val & BIT(0) ? 1 : 0;

	debug("%s: safety abc value = 0x%x, slot = %d\n",
	               __func__, val, slot);
	return slot;
}

static inline int get_safety_abc_staus(uint32_t val)
{
	int status;

	status = val & BIT(1) ? 1 : 0;

	debug("%s: safety abc value = 0x%x, status = %d\n",
	               __func__, val, status);
	return status;
}

static inline void set_safety_abc_status(uint32_t *val, int status)
{
	if (!val) {
		pr_err("Invaild argument\n");
		return;
	}

	if (status)
		*val |= BIT(1);
	else
		*val &= ~BIT(1);

	debug("%s: safety abc value = 0x%x, status = %d\n",
	               __func__, *val, status);
}

static inline int get_safety_abc_times(uint32_t val)
{
	int times;

	times = (val >> 2) & 0x3;

	debug("%s: safety abc value = 0x%x, times = %d\n",
	               __func__, val, times);
	return times;
}

static inline void set_safety_abc_times(uint32_t *val, int times)
{
	if (!val) {
		pr_err("Invaild argument\n");
		return;
	}

	if (times < 0 || times > 3) {
		pr_err("Invalid times value: %d\n", times);
		return;
	}

	*val = (*val & ~0xC) | ((times & 0x3) << 2);

	debug("%s: safety abc value = 0x%x, times = %d\n",
	               __func__, *val, times);
}

int safety_abc_setup(int mark_type, int slot)
{
	uint32_t val;
	int ret;

	if (slot < 0 || slot > 1) {
		pr_err("Invalid slot: %c\n", SLOT_NAME(slot));
		return -EINVAL;
	}

	ret = safety_abc_read(&val);
	if (ret < 0) {
		pr_err("Could not read safety abc value\n");
		return ret;
	}

	switch (mark_type) {
	case AB_MARK_SUCCESSFUL:
		val &= ~BIT(1);
		break;
	case AB_MARK_UNBOOTABLE:
		val &= ~BIT(0);
		val |= slot;
		val |= BIT(1);
		break;
	case AB_MARK_ACTIVE:
		val &= ~BIT(0);
		val |= slot;
		val &= ~BIT(1);
		set_safety_abc_times(&val, 0);
		break;
	}

	return safety_abc_write(&val);
}

int safety_abc_get_slot(void)
{
	uint32_t val;
	int ret;

	ret = safety_abc_read(&val);
	if (ret < 0) {
		pr_err("Could not read safety abc value\n");
		return ret;
	}

	if (get_safety_abc_staus(val))
		return !get_safety_abc_slot(val);
	else
		return !!get_safety_abc_slot(val);
}

int abc_board_setup(enum ab_slot_mark type, int slot)
{
	int ret;

	if (slot < 0 || slot > 1) {
		pr_err("Wrong slot (slot = %d)\n", slot);
		return -EINVAL;
	}

	ret = safety_abc_setup(type, slot);
	if (ret < 0) {
		pr_err("Unable to setup safety abc\n");
		return ret;
	}

	return 0;
}