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
#include <init.h>
#include <linux/delay.h>

#define NPU_OCM_SIZE        SZ_2M
#define SAFETY_IRAM_SIZE    SZ_64K

static int ocm_rw(uint64_t addr, int size)
{
	char *buf = map_sysmem(addr, size);
	int i, count = 0;

	memset(buf, '\xff', size);

	for (i = 0; i < size / 4; i++)
		writel(i, buf + i * 4);

	for (i = 0; i < size / 4; i++) {
		if (readl(buf + i * 4) != i) {
			printf("got = 0x%x, i = 0x%x [%p]\n",
				readl(buf + i * 4), i,
				buf + i * 4);
			count++;
		}
	}

	if (count)
		return -1;

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
		ut_assertok(ocm_rw(CONFIG_LUA_OCM_BASE, NPU_OCM_SIZE));
		elapsed += get_timer(ticks);
		printf("\rElapsed Time: %lld.%03ds ", elapsed / 1000, (int)(elapsed % 1000));
	}
	putc('\n');
	printf("%s: pass\n", __func__);

	return 0;
}
LAGUNA_STRESS_TEST(test_ocm_rw_stress, UT_TESTF_CONSOLE_REC);

static int test_npu_ocm(struct unit_test_state *uts)
{
	ut_assertok(ocm_rw(CONFIG_LUA_OCM_BASE, NPU_OCM_SIZE));

	printf("%s: pass\n", __func__);
	return 0;
}
LAGUNA_TEST(test_npu_ocm, UT_TESTF_CONSOLE_REC);

static int test_spl_npu_ocm_cacheable(struct unit_test_state *uts)
{
	u64 time_ms = 6UL * 60 * 60 * 1000;
	u64 elapsed = 0;
	u64 ticks;
	u64 addr = CONFIG_LUA_OCM_BASE + SZ_1M;

	while (elapsed < time_ms) {
		if (ctrlc())
			ut_assertf(0, "press Ctrl-C");
		ticks = get_timer(0);
		ut_assertok(ocm_rw(addr, SZ_512K));

		invalidate_icache_all();
		invalidate_dcache_range(addr, SZ_512K);
		flush_dcache_range(addr, SZ_512K);

		elapsed += get_timer(ticks);
		printf("\rElapsed Time: %lld.%03ds ", elapsed / 1000, (int)(elapsed % 1000));
	}
	putc('\n');
	printf("%s: pass\n", __func__);

	return 0;
}
LAGUNA_SPL_TEST(test_spl_npu_ocm_cacheable, UT_TESTF_CONSOLE_REC);

static int test_spl_npu_ocm_noncacheable(struct unit_test_state *uts)
{
	u64 time_ms = 6UL * 60 * 60 * 1000;
	u64 elapsed = 0;
	u64 ticks;
	u64 addr = CONFIG_LUA_OCM_BASE + SZ_1M;

	while (elapsed < time_ms) {
		if (ctrlc())
			ut_assertf(0, "press Ctrl-C");
		ticks = get_timer(0);
		ut_assertok(ocm_rw(addr, SZ_512K));
		elapsed += get_timer(ticks);
		printf("\rElapsed Time: %lld.%03ds ", elapsed / 1000, (int)(elapsed % 1000));
	}
	putc('\n');
	printf("%s: pass\n", __func__);

	return 0;
}
LAGUNA_SPL_TEST(test_spl_npu_ocm_noncacheable, UT_TESTF_CONSOLE_REC);

static int safety_iram_rw(int size)
{
	char *buf = map_sysmem(CONFIG_LUA_IRAM_BASE, size);
	int i, count = 0;

	memset(buf, '\xff', size);

	for (i = 0; i < size / 4; i++)
		writel(i, buf + i * 4);

	for (i = 0; i < size / 4; i++) {
		if (readl(buf + i * 4) != i) {
			printf("got = 0x%x, i = 0x%x [%p]\n",
				readl(buf + i * 4), i,
				buf + i * 4);
			count++;
		}
	}

	/* iram first 4K reserved for boot_params_t */
	memset(buf, 0, SZ_4K);

	if (count)
		return -1;

	return 0;
}

static int test_safety_iram(struct unit_test_state *uts)
{
	ut_assertok(safety_iram_rw(SAFETY_IRAM_SIZE));

	printf("%s: pass\n", __func__);
	return 0;
}
LAGUNA_TEST(test_safety_iram, UT_TESTF_CONSOLE_REC);

static int test_spl_safety_iram(struct unit_test_state *uts)
{
	ut_assertok(safety_iram_rw(SAFETY_IRAM_SIZE));

	printf("%s: pass\n", __func__);
	return 0;
}
LAGUNA_SPL_TEST(test_spl_safety_iram, UT_TESTF_CONSOLE_REC);

static int test_ddr(struct unit_test_state *uts)
{
	bool mismatch = true;

	mismatch = (get_ram_size((void *)CONFIG_LUA_DRAM_BASE, SZ_4G) != SZ_4G);
	ut_assertok(mismatch);

	printf("%s: pass\n", __func__);
	return 0;
}
LAGUNA_TEST(test_ddr, UT_TESTF_CONSOLE_REC);

static int test_generic_timer(struct unit_test_state *uts)
{
	int wait = 50;
	unsigned long start = get_timer(0);
	mdelay(wait);

	ut_assert(wait == get_timer(start));

	printf("%s: pass\n", __func__);
	return 0;
}
LAGUNA_TEST(test_generic_timer, UT_TESTF_CONSOLE_REC);
