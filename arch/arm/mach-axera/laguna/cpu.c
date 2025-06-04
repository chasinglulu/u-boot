/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <misc.h>
#include <asm/system.h>

#include <asm/arch/bootparams.h>

static const char *safety_abort_str[] = {
	[SAFETY_ABORT_UNKNOWN] = "Unknown",
	[SAFETY_ABORT_SWRST_ALARM] = "SWRST",
	[SAFETY_ABORT_FOSU_ALARM] = "FOSU",
	[SAFETY_ABORT_FCU_ALARM] = "FCU",
	[SAFETY_ABORT_WTD4_ALARM] = "WTD4",
	[SAFETY_ABORT_WTD5_ALARM] = "WTD5",
	[SAFETY_ABORT_THM_ALARM] = "THM",
	[SAFETY_ABORT_LBIST_ALARM] = "LBIST",
};

static const char *main_abort_str[] = {
	[ABORT_UNKNOWN] = "Unknown",
	[ABORT_WDT0_ALARM] = "WDT0",
	[ABORT_WDT1_ALARM] = "WDT1",
	[ABORT_WDT2_ALARM] = "WDT2",
	[ABORT_WDT3_ALARM] = "WDT3",
	[ABORT_SAFETY_HW_ALARM] = "Safety HW",
	[ABORT_SAFETY_SW_ALARM] = "Safety SW",
	[ABORT_MAIN_SW_ALARM] = "Main SW",
	[ABORT_ALARM_REQ] = "Alarm Request",
};

int get_abort(bool safety, const char **name)
{
	struct udevice *dev;
	uint32_t abort;

	uclass_foreach_dev_probe(UCLASS_MISC, dev) {
		abort = misc_read(dev, 0, NULL, 0);
		if (abort < 0) {
			debug("Failed to read data from '%s': %d\n",
			       dev->name, abort);
			continue;
		}

		if (safety && !strcmp(dev->name, "safety_abort_alarm")) {
			if (name)
				*name = abort < SAFETY_ABORT_COUNT ? safety_abort_str[abort] : "Unknown";
			return abort < SAFETY_ABORT_COUNT ? abort : -EIO;
		} else if (!safety && !strcmp(dev->name, "abort_alarm")) {
			if (name)
				*name = abort < ABORT_COUNT ? main_abort_str[abort] : "Unknown";
			return abort < ABORT_COUNT ? abort : -EIO;
		}
	}

	if (name)
		*name = "Unknown";
	return -ENODEV;
}

static void print_abort_alarm(uint32_t abort, bool safety)
{
	if (safety) {
		switch (abort) {
		case SAFETY_ABORT_SWRST_ALARM:
		case SAFETY_ABORT_FOSU_ALARM:
		case SAFETY_ABORT_FCU_ALARM:
		case SAFETY_ABORT_WTD4_ALARM:
		case SAFETY_ABORT_WTD5_ALARM:
		case SAFETY_ABORT_THM_ALARM:
		case SAFETY_ABORT_LBIST_ALARM:
			printf("%s\n", safety_abort_str[abort]);
			break;
		default:
			printf("Unknown\n");
			break;
		}
	} else {
		switch (abort) {
		case ABORT_WDT0_ALARM:
		case ABORT_WDT1_ALARM:
		case ABORT_WDT2_ALARM:
		case ABORT_WDT3_ALARM:
		case ABORT_SAFETY_HW_ALARM:
		case ABORT_SAFETY_SW_ALARM:
		case ABORT_MAIN_SW_ALARM:
		case ABORT_ALARM_REQ:
			printf("%s\n", main_abort_str[abort]);
			break;
		default:
			printf("Unknown\n");
			break;
		}
	}
}

#if defined(CONFIG_DISPLAY_CPUINFO) && !defined(CONFIG_SPL_BUILD)
static int print_reset_cause(void)
{
	struct udevice *dev;
	uint32_t abort;

	uclass_foreach_dev_probe(UCLASS_MISC, dev) {
		abort = misc_read(dev, 0, NULL, 0);
		if (abort < 0) {
			debug("Failed to read data from '%s': %d\n",
			       dev->name, abort);
			continue;
		}

		if (!strcmp(dev->name, "safety_abort_alarm")) {
			printf("Safe Reset Cause: ");
			print_abort_alarm(abort, true);
		} else if (!strcmp(dev->name, "abort_alarm")) {
			printf("Main Reset Cause: ");
			print_abort_alarm(abort, false);
		}
	}

	return 0;
}

int print_cpuinfo(void)
{
	const char *name = NULL;

	printf("EL Level:      EL%d\n", current_el());

	get_bootdevice(&name);
	printf("Boot Device:   %s\n", name ?: "Unknown");

	print_reset_cause();
	return 0;
}
#endif