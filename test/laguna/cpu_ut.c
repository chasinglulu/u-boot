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
#include <div64.h>
#include <linux/delay.h>
#include <rand.h>

#define SAFETY_OCM_BASE     0x00200000
#define LOOPS_EXP           0
#define LOOPS               BIT(LOOPS_EXP)

#define NPU_OCM_SIZE        SZ_2M
#define SAFETY_IRAM_SIZE    SZ_64K
#define SAFETY_OCM_SIZE     SZ_2M

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

static int read_perf(void *addr, int count)
{
	int i;

	for (i = 0; i < count; i++)
		readl(addr + i * 4);

	return 0;
}

static int write_perf(void *addr, int count)
{
	int i;

	for (i = 0; i < count; i++)
		writel(i, addr + i * 4);

	return 0;
}

static int rw_perf(void *addr, int count)
{
	int i;

	for (i = 0; i < count; i++)
		writel(i, addr + i * 4);

	for (i = 0; i < count; i++)
		readl(addr + i * 4);

	return 0;
}

static int perf_test(void *buf, ulong size)
{
	u64 elapsed = 0;
	u64 ticks, total;
	int i;

	/* Read-Write Case */
	total = size * 1000000 * 2;
	ticks = get_timer_us(0);
	for (i = 0; i < LOOPS; i++)
		rw_perf(buf, size / 4);
	elapsed += get_timer_us(ticks);
	elapsed >>= LOOPS_EXP;
	elapsed <<= 20;
	printf("       RW: [%lld.%-3lld] MB/s\n", total / elapsed, (total % elapsed) / 1000000);
	invalidate_dcache_range((ulong)buf, (ulong)buf + size);
	flush_dcache_range((ulong)buf, (ulong)buf + size);

	/* Read Case */
	total = size * 1000000;
	elapsed = 0;
	ticks = get_timer_us(0);
	for (i = 0; i < LOOPS; i++)
		read_perf(buf, size / 4);
	elapsed += get_timer_us(ticks);
	elapsed >>= LOOPS_EXP;
	elapsed <<= 20;
	printf("     Read: [%lld.%-3lld] MB/s\n", total / elapsed, (total % elapsed) / 1000000);
	invalidate_dcache_range((ulong)buf, (ulong)buf + size);
	flush_dcache_range((ulong)buf, (ulong)buf + size);

	/* Write Case */
	total = size * 1000000;
	elapsed = 0;
	ticks = get_timer_us(0);
	for (i = 0; i < LOOPS; i++)
		write_perf(buf, size / 4);
	elapsed += get_timer_us(ticks);
	elapsed >>= LOOPS_EXP;
	elapsed <<= 20;
	printf("    Write: [%lld.%-3lld] MB/s\n", total / elapsed, (total % elapsed) / 1000000);
	invalidate_dcache_range((ulong)buf, (ulong)buf + size);
	flush_dcache_range((ulong)buf, (ulong)buf + size);

	return 0;
}

static int test_ocm_rw_perf(struct unit_test_state *uts)
{
	char *buf = map_sysmem(CONFIG_LUA_OCM_BASE, NPU_OCM_SIZE);

	printf("NPU OCM Performance:\n");
	ut_assertok(perf_test(buf, NPU_OCM_SIZE));

	return 0;
}
LAGUNA_PERF_TEST(test_ocm_rw_perf, UT_TESTF_CONSOLE_REC);

static int test_safety_iram_rw_perf(struct unit_test_state *uts)
{
	char *buf = map_sysmem(CONFIG_LUA_IRAM_BASE, SAFETY_IRAM_SIZE);

	printf("Safety IRAM Performance:\n");
	ut_assertok(perf_test(buf, SAFETY_IRAM_SIZE));

	return 0;
}
LAGUNA_PERF_TEST(test_safety_iram_rw_perf, UT_TESTF_CONSOLE_REC);

static int test_safety_ocm_rw_perf(struct unit_test_state *uts)
{
	char *buf = map_sysmem(SAFETY_OCM_BASE, SAFETY_OCM_SIZE);

	printf("Safety OCM Performance:\n");
	ut_assertok(perf_test(buf, SAFETY_OCM_SIZE));

	return 0;
}
LAGUNA_PERF_TEST(test_safety_ocm_rw_perf, UT_TESTF_CONSOLE_REC);

static int test_nocache_ocm_rw_perf(struct unit_test_state *uts)
{
	char *buf = map_sysmem(CONFIG_LUA_OCM_BASE, SZ_64K);

	printf("NPU OCM With Cache Off Performance:\n");
	dcache_disable();
	ut_assertok(perf_test(buf, SZ_64K));
	dcache_enable();

	return 0;
}
LAGUNA_PERF_TEST(test_nocache_ocm_rw_perf, UT_TESTF_CONSOLE_REC);

static int test_nocache_safety_iram_rw_perf(struct unit_test_state *uts)
{
	char *buf = map_sysmem(CONFIG_LUA_IRAM_BASE, SAFETY_IRAM_SIZE);

	printf("Safety IRAM With Cache Off Performance:\n");
	dcache_disable();
	ut_assertok(perf_test(buf, SAFETY_IRAM_SIZE));
	dcache_enable();

	return 0;
}
LAGUNA_PERF_TEST(test_nocache_safety_iram_rw_perf, UT_TESTF_CONSOLE_REC);

static int test_nocache_safety_ocm_rw_perf(struct unit_test_state *uts)
{
	char *buf = map_sysmem(SAFETY_OCM_BASE, SZ_64K);

	printf("Safety OCM With Cache Off Performance:\n");
	dcache_disable();
	ut_assertok(perf_test(buf, SZ_64K));
	dcache_enable();

	return 0;
}
LAGUNA_PERF_TEST(test_nocache_safety_ocm_rw_perf, UT_TESTF_CONSOLE_REC);

#ifndef CONFIG_SPL_BUILD
static int test_ddr_memcpy(struct unit_test_state *uts)
{
	struct item {
		const char *name;
		size_t size;
	} test[] = {
		{ "SZ_4K", SZ_4K },
		{ "SZ_8K", SZ_8K },
		{ "SZ_16K", SZ_16K },
		{"SZ_256K", SZ_256K},
		{ "SZ_512K", SZ_512K },
		{ "SZ_1M", SZ_1M },
		{ "SZ_2M", SZ_2M },
	};
	char *buf, *buf1, *tmp;
	int i, j;
	uint32_t buf_size = CONFIG_VAL(SYS_MALLOC_LEN) >> 1;
	ulong start;
	ulong elapsed = 0;
	pr_err("buf len: 0x%x\n", buf_size);

	buf = malloc(buf_size);
	if (!buf) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}
	tmp = buf;
	srand((u32)get_timer(0));
	for (i = 0; i < buf_size; i += sizeof(ulong), tmp += sizeof(ulong))
		*(ulong *)tmp = rand();

	for (i = 0; i < ARRAY_SIZE(test); i++) {
		if (test[i].size >= (CONFIG_VAL(SYS_MALLOC_LEN) - buf_size))
			continue;

		buf1 = malloc(test[i].size);
		start = elapsed = 0;

		for (j = 0; j < 64; j++) {
			start = get_timer_us(0);
			memcpy(buf1, buf, test[i].size);
			elapsed += get_timer_us(start);
		}
		invalidate_dcache_range((ulong)buf1, (ulong)buf1 + test[i].size);
		flush_dcache_range((ulong)buf1, (ulong)buf1 + test[i].size);

		elapsed >>= 6;

		printf("memcpy %s: %lu us\n", test[i].name, elapsed);
		ulong total = test[i].size * 1000000 * 2;
		elapsed <<= 20;
		printf("       RW: [%lu.%-3lu] MB/s\n", total / elapsed, (total % elapsed) / 10000);

		free(buf1);
	}
	free(buf);
	printf("%s: pass\n", __func__);

	return 0;
}
LAGUNA_PERF_TEST(test_ddr_memcpy, UT_TESTF_CONSOLE_REC);
#endif