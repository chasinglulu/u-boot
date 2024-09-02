// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 * Unit tests for A55 CPU and DUS RAS functions
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

#define CONFIG_CORTEX_A55

/* ERR<n>PFGCTL */
#define ERRPFGCTLR_CDNEN      BIT(31)
#define ERRPFGCTLR_R          BIT(30)
#define ERRPFGCTLR_CE         BIT(6)
#define ERRPFGCTLR_DE         BIT(5)
#define ERRPFGCTLR_UER        BIT(3)
#define ERRPFGCTLR_UC         BIT(1)

/* ERR<n>PFGF */
#define ERRPFGF_PFG           BIT(31)
#define ERRPFGF_R             BIT(30)
#define ERRPFGF_CE            BIT(6)
#define ERRPFGF_DE            BIT(5)
#define ERRPFGF_UEO           BIT(4)
#define ERRPFGF_UER           BIT(3)
#define ERRPFGF_UEU           BIT(2)
#define ERRPFGF_UC            BIT(1)

/* ERR<n>CTLR */
#define ERRCTLR_CFI           BIT(8)
#define ERRCTLR_FI            BIT(3)
#define ERRCTLR_UI            BIT(2)
#define ERRCTLR_ED            BIT(0)

static inline uint16_t read_erridr_el1(void)
{
	uint64_t num;
#if GCC_VERSION >= 100301
	asm volatile("mrs %0, erridr_el1" : "=r" (num));
#else
	asm volatile("mrs %0, s3_0_c5_c3_0" : "=r" (num));
#endif

	return num & GENMASK(15, 0);
}

static inline uint16_t read_errselr_el1(void)
{
	uint64_t idx;
#if GCC_VERSION >= 100301
	asm volatile("mrs %0, errselr_el1" : "=r" (idx));
#else
	asm volatile("mrs %0, s3_0_c5_c3_1" : "=r" (idx));
#endif

	return idx & BIT(0);
}

static inline void write_errselr_el1(uint64_t val)
{
	uint16_t num = read_erridr_el1();
	uint64_t idx = val & BIT(0);

	assert(idx < num);

#if GCC_VERSION >= 100301
	asm volatile("msr errselr_el1, %0" : : "r" (idx) : "memory");
#else
	asm volatile("msr s3_0_c5_c3_1, %0" : : "r" (idx) : "memory");
#endif
}

static inline uint64_t read_erxctlr_el1(void)
{
	uint64_t ctl;

#if GCC_VERSION >= 100301
	asm volatile("mrs %0, erxctlr_el1" : "=r" (ctl));
#else
	asm volatile("mrs %0, s3_0_c5_c4_1" : "=r" (ctl));
#endif

	return ctl;
}

static inline void write_erxctlr_el1(uint64_t val)
{
#if GCC_VERSION >= 100301
	asm volatile("msr erxctlr_el1, %0" : : "r" (val) : "memory");
#else
	asm volatile("msr s3_0_c5_c4_1, %0" : : "r" (val) : "memory");
#endif
}

static inline void write_erxstatus_el1(uint64_t val)
{
#if GCC_VERSION >= 100301
	asm volatile("msr erxstatus_el1, %0" : : "r" (val) : "memory");
#else
	asm volatile("msr s3_0_c5_c4_2, %0" : : "r" (val) : "memory");
#endif
}


static inline uint64_t read_erxpfgcdn_el1(void)
{
	uint64_t feat;
#ifdef CONFIG_CORTEX_A55
	asm volatile("mrs %0, s3_0_c15_c2_2" : "=r" (feat));
#else
#if GCC_VERSION >= 100301
	asm volatile("mrs %0, erxpfgcdn_el1" : "=r" (feat));
#else
	asm volatile("mrs %0, s3_0_c5_c4_6" : "=r" (feat));
#endif
#endif

	return feat;
}

static inline void write_erxpfgcdn_el1(uint64_t val)
{
#ifdef CONFIG_CORTEX_A55
	asm volatile("msr s3_0_c15_c2_2, %0" : : "r" (val) : "memory");
#else
#if GCC_VERSION >= 100301
	asm volatile("msr erxpfgcdn_el1, %0" : : "r" (val) : "memory");
#else
	asm volatile("msr s3_0_c5_c4_6, %0" : : "r" (val) : "memory");
#endif
#endif
}

static inline uint64_t read_erxpfgctl_el1(void)
{
	uint64_t feat;
#ifdef CONFIG_CORTEX_A55
	asm volatile("mrs %0, s3_0_c15_c2_1" : "=r" (feat));
#else
#if GCC_VERSION >= 100301
	asm volatile("mrs %0, erxpfgctl_el1" : "=r" (feat));
#else
	asm volatile("mrs %0, s3_0_c5_c4_5" : "=r" (feat));
#endif
#endif

	return feat;
}

static inline void write_erxpfgctl_el1(uint64_t val)
{
#ifdef CONFIG_CORTEX_A55
	asm volatile("msr s3_0_c15_c2_1, %0" : : "r" (val) : "memory");
#else
#if GCC_VERSION >= 100301
	asm volatile("msr erxpfgctl_el1, %0" : : "r" (val) : "memory");
#else
	asm volatile("msr s3_0_c5_c4_5, %0" : : "r" (val) : "memory");
#endif
#endif
}

static inline uint64_t read_erxpfgf_el1(void)
{
	uint64_t feat;
#ifdef CONFIG_CORTEX_A55
	asm volatile("mrs %0, s3_0_c15_c2_0" : "=r" (feat));
#else
#if GCC_VERSION >= 100301
	asm volatile("mrs %0, erxpfgf_el1" : "=r" (feat));
#else
	asm volatile("mrs %0, s3_0_c5_c4_4" : "=r" (feat));
#endif
#endif

	return feat;
}

enum error_type {
	/* Corrected errors */
	ERROR_CE,
	/* Deferred errors */
	ERROR_DE,
	/* Uncontainable errors */
	ERROR_UC,
	/* Latent error */
	ERROR_UEO,
};

static void update_erxctlr_el1(int type)
{
	uint64_t val;

	val = read_erxctlr_el1();
	val &= 0ULL;
	switch (type) {
	case ERROR_CE:
		val |= ERRCTLR_CFI;
		break;
	case ERROR_DE:
		val |= ERRCTLR_FI;
		break;
	case ERROR_UC:
		val |= ERRCTLR_UI;
		break;
	}
	val |= ERRCTLR_ED;
	write_erxctlr_el1(val);
}

static void update_erxpfgctl_el1(int type)
{
	uint64_t val;

	val = read_erxpfgctl_el1();
	val &= 0ULL;
	switch (type) {
	case ERROR_CE:
		val |= ERRPFGCTLR_CE;
		break;
	case ERROR_DE:
		val |= ERRPFGCTLR_DE;
		break;
	case ERROR_UC:
		val |= ERRPFGCTLR_UC;
		break;
	}
	val |= (ERRPFGCTLR_CDNEN | ERRPFGCTLR_R);
	write_erxpfgctl_el1(val);
}

static void clear_injected_error(int idx)
{
	write_errselr_el1(idx);
	write_erxctlr_el1(0);
	write_erxpfgctl_el1(0);
	write_erxstatus_el1(0xFFF00000);
}

static int inject_single_error(int type, int idx, uint64_t cdn)
{
	uint64_t func;

	assert(idx < read_erridr_el1());

	write_errselr_el1(idx);
	func = read_erxpfgf_el1();
	if (!(func & ERRPFGF_PFG))
		return -EINVAL;

	/* Clear the last injection */
	clear_injected_error(idx);

	update_erxctlr_el1(type);
	write_erxpfgcdn_el1(cdn);
	update_erxpfgctl_el1(type);

	return 0;
}

static int cpu_ras_inject_errs(struct unit_test_state *uts)
{
	void *addr;
	uint64_t len;
	int i;

	ut_assertok(inject_single_error(ERROR_CE, 0, 0x500));
	printf("The output is used to trigger L1/L2 cache corrected errors.\n");
	len = SZ_128K;
	addr = malloc(len);
	memset(addr, '\xff', len);
	flush_dcache_range((ulong)addr, (ulong)addr + len);
	for (i = 0; i < len / 4; i++)
		writel(i, addr + i * 4);
	flush_dcache_range((ulong)addr, (ulong)addr + len);
	free(addr);
	printf("Inject CPU corrected errors successfully\n");

	mdelay(5000);

	ut_assertok(inject_single_error(ERROR_DE, 0, 0x200));
	printf("The output is used to trigger L1/L2 cache deferred errors.\n");
	len = SZ_1K;
	addr = malloc(len);
	memset(addr, '\xff', len);
	flush_dcache_range((ulong)addr, (ulong)addr + len);
	free(addr);
	printf("Inject CPU deferred errors successfully\n");

	mdelay(5000);

	ut_assertok(inject_single_error(ERROR_UC, 0, 0x1));
	printf("The output is used to trigger L1/L2 cache uncontainable errors.\n");
	printf("Inject CPU uncontainable errors successfully\n");

	/* CLear injected errors after injection test */
	clear_injected_error(0);

	return 0;
}
LAGUNA_RAS_TEST(cpu_ras_inject_errs, UT_TESTF_CONSOLE_REC);

static int dsu_ras_inject_errs(struct unit_test_state *uts)
{
	void *addr;
	uint64_t len;
	int i;

	ut_assertok(inject_single_error(ERROR_CE, 1, 0x10));
	printf("The output is used to trigger L3 cache corrected errors.\n");
	len = SZ_128K;
	addr = malloc(len);
	memset(addr, '\xff', len);
	flush_dcache_range((ulong)addr, (ulong)addr + len);
	for (i = 0; i < len / 4; i++)
		writel(i, addr + i * 4);
	flush_dcache_range((ulong)addr, (ulong)addr + len);
	free(addr);
	printf("Inject DSU corrected errors successfully\n");

	mdelay(5000);

	// ut_assertok(inject_single_error(ERROR_DE, 1, 0x200));
	// printf("The output is used to trigger L3 cache deferred errors.\n");
	// len = SZ_1K;
	// addr = malloc(len);
	// memset(addr, '\xff', len);
	// flush_dcache_range((ulong)addr, (ulong)addr + len);
	// free(addr);
	// printf("Inject DSU deferred errors successfully\n");

	// mdelay(5000);

	ut_assertok(inject_single_error(ERROR_UC, 1, 0x50));
	printf("The output is used to trigger L3 cache uncontainable errors.\n");
	printf("Inject DSU uncontainable errors successfully\n");

	/* CLear injected errors after injection test */
	clear_injected_error(1);

	return 0;
}
LAGUNA_RAS_TEST(dsu_ras_inject_errs, UT_TESTF_CONSOLE_REC);
