// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 * Unit tests for Main Domain A55 CPU functions
 */

#include <common.h>
#include <test/laguna.h>
#include <test/test.h>
#include <test/ut.h>
#include <linux/sizes.h>
#include <asm/io.h>
#include <mapmem.h>

#define NPU_OCM_SIZE        SZ_2M
#define SAFETY_IRAM_SIZE    SZ_64K

static int ocm_rw(int size)
{
	char *buf = map_sysmem(CONFIG_LUA_OCM_BASE, size);
	int i;

	memset(buf, '\xff', size);

	for (i = 0; i < size / 4; i++)
		writel(i, buf + i * 4);

	for (i = 0; i < size / 4; i++) {
		if (readl(buf + i * 4) != i)
			return -1;
	}

	return 0;
}

static int test_ocm_rw_stress(struct unit_test_state *uts)
{
	u64 time_ms = 12UL * 60 * 60 * 1000;
	u64 elapsed = 0;
	u64 ticks;

	while (elapsed < time_ms) {
		if (ctrlc())
			ut_assertf(0, "press Ctrl-C");
		ticks = get_timer(0);
		ut_assertok(ocm_rw(NPU_OCM_SIZE));
		elapsed += get_timer(ticks);
		printf("\rElapsed Time: %lld.%03ds ", elapsed / 1000, (int)(elapsed % 1000));
	}
	putc('\n');

	return 0;
}
LAGUNA_STRESS_TEST(test_ocm_rw_stress, UT_TESTF_CONSOLE_REC);

static int test_npu_ocm(struct unit_test_state *uts)
{
	ut_assertok(ocm_rw(NPU_OCM_SIZE));

	return 0;
}
LAGUNA_TEST(test_npu_ocm, UT_TESTF_CONSOLE_REC);

static int safety_iram_rw(int size)
{
	char *buf = map_sysmem(CONFIG_LUA_OCM_BASE, size);
	int i;

	memset(buf, '\xff', size);

	for (i = 0; i < size / 4; i++)
		writel(i, buf + i * 4);

	for (i = 0; i < size / 4; i++) {
		if (readl(buf + i * 4) != i)
			return -1;
	}

	return 0;
}

static int test_safety_iram(struct unit_test_state *uts)
{
	ut_assertok(safety_iram_rw(SAFETY_IRAM_SIZE));

	return 0;
}
LAGUNA_TEST(test_safety_iram, UT_TESTF_CONSOLE_REC);


static int test_ddr(struct unit_test_state *uts)
{

	return 0;
}
LAGUNA_TEST(test_ddr, UT_TESTF_CONSOLE_REC);