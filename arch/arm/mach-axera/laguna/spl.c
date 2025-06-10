/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (c) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <debug_uart.h>
#include <asm/system.h>
#include <semihosting.h>
#include <android_ab.h>
#include <asm/io.h>
#include <hang.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <dm.h>
#include <spl.h>
#include <cpu_func.h>
#include <malloc.h>
#include <test/test.h>
#include <test/ut.h>
#include <asm/arch/bootparams.h>
#include <dm/device-internal.h>
#include <linux/delay.h>

u32 spl_boot_device(void)
{
	boot_params_t *bp = boot_params_get_base();
	u32 dev_id = BOOT_DEVICE_RAM;

	switch (bp->bootdevice) {
	case BOOTDEVICE_ONLY_EMMC:
	case BOOTDEVICE_BOTH_NOR_EMMC:
	case BOOTDEVICE_BOTH_NAND_EMMC:
	case BOOTDEVICE_BOTH_HYPER_EMMC:
		dev_id = BOOT_DEVICE_MMC1;
		break;
	case BOOTDEVICE_ONLY_NOR:
	case BOOTDEVICE_BOTH_NOR_NOR:
	case BOOTDEVICE_BOTH_NAND_NOR:
	case BOOTDEVICE_BOTH_HYPER_NOR:
		dev_id = BOOT_DEVICE_NOR;
		break;
	case BOOTDEVICE_ONLY_NAND:
	case BOOTDEVICE_BOTH_NOR_NAND:
	case BOOTDEVICE_BOTH_NAND_NAND:
	case BOOTDEVICE_BOTH_HYPER_NAND:
		dev_id = BOOT_DEVICE_NAND;
		break;
	case BOOTDEVICE_ONLY_HYPER:
	case BOOTDEVICE_BOTH_NOR_HYPER:
	case BOOTDEVICE_BOTH_NAND_HYPER:
	case BOOTDEVICE_BOTH_HYPER_HYPER:
		dev_id = BOOT_DEVICE_SPI;
		break;
	case BOOTDEVICE_UART:
		dev_id = BOOT_DEVICE_UART;
		break;
	case BOOTDEVICE_USB:
		dev_id = BOOT_DEVICE_USB;
		break;
	case BOOTDEVICE_SD:
		dev_id = BOOT_DEVICE_MMC2;
		break;
	case BOOTDEVICE_OCM:
		dev_id = BOOT_DEVICE_BOARD;
		break;
	default:
		pr_err("Unknown boot device\n");
	}

	if (semihosting_enabled())
		return BOOT_DEVICE_SMH;

	return dev_id;
}

void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = spl_boot_device();
	if (spl_boot_list[0] == BOOT_DEVICE_SMH)
		spl_boot_list[1] = BOOT_DEVICE_RAM;
}

struct image_header *spl_get_load_buffer(ssize_t offset, size_t size)
{
	return (struct image_header *)(CONFIG_SYS_LOAD_ADDR);
}

void spl_board_prepare_for_boot(void)
{
	/* Nothing to do */
}

void spl_board_prepare(void)
{
	boot_params_t *bp = boot_params_get_base();

#if defined(CONFIG_CONSOLE_RECORD) && defined(CONFIG_CONSOLE_RECORD_FIXED)

	/* Copy console output settings */
	memcpy((void *)&bp->spl_console_out,
	          (void *)&gd->console_out, sizeof(gd->console_out));
#endif

	remove_mtd_device(bp->bootdevice);

	calc_bootparams_crc32(false);
}

int spl_parse_board_header(struct spl_image_info *spl_image,
				  const struct spl_boot_device *bootdev,
				  const void *image_header, size_t size)
{
#if IS_ENABLED(CONFIG_LUA_ATF_SUPPORT)
	if (IS_ENABLED(CONFIG_SPL_ATF) &&
		    bootdev->boot_device == BOOT_DEVICE_RAM
		    && current_el() == 3) {
		spl_image->entry_point = CONFIG_LUA_SPL_ATF_LOAD_ADDR;
		spl_image->load_addr = CONFIG_LUA_SPL_ATF_LOAD_ADDR;

		return 0;
	}
#endif

	return -EINVAL;
}

#if !(CONFIG_IS_ENABLED(SYS_ICACHE_OFF) && CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
static inline void setup_caches(void)
{
	gd->arch.tlb_size = PGTABLE_SIZE;
	/* Align tlb_addr to 16KiB */
	gd->arch.tlb_addr = (uint64_t)memalign(SZ_16K, gd->arch.tlb_size);
	enable_caches();
}
#else
static inline void setup_caches(void) { }
#endif

#if CONFIG_IS_ENABLED(SOC_INIT)
void spl_soc_init(void)
{
#if defined (CONFIG_LUA_CUSTOM_BOOTSTRAP)
	/* unlock bootstrap register write permission */
	writel(BIT(0), CONFIG_LUA_BOOTSTRAP_REGADDR);

	/* Customize bootstrap register value */
	writel(CONFIG_LUA_BOOTSTRAP_REGVAL, CONFIG_LUA_BOOTSTRAP_REGADDR + 0xC);
#endif

#if defined (CONFIG_LUA_CUSTOM_DOWNLOAD_IF)
	/* Customize download interface register value */
	writel(CONFIG_LUA_DOWNLOAD_IF_REGVAL, CONFIG_LUA_DOWNLOAD_IF_REGADDR);
#endif

#if CONFIG_IS_ENABLED(LUA_CPU_CACHE)
	setup_caches();
#endif
}
#endif

#ifdef CONFIG_SPL_LUA_TEST
#ifdef CONFIG_SPL_LUA_OCM_TEST
static void spl_test_npu_ocm(void)
{
	struct unit_test *tests = UNIT_TEST_SUITE_START(laguna_spl_test);
	const int n_ents = UNIT_TEST_SUITE_COUNT(laguna_spl_test);

	ut_run_list("ocm-noncacheable", "test_spl_", tests, n_ents, "npu_ocm_noncacheable");

	setup_caches();
	ut_run_list("ocm-cacheable", "test_spl_", tests, n_ents, "npu_ocm_cacheable");
}
#endif

#ifdef CONFIG_SPL_LUA_IRAM_TEST
static void spl_test_safety_iram(void)
{
	struct unit_test *tests = UNIT_TEST_SUITE_START(laguna_spl_test);
	const int n_ents = UNIT_TEST_SUITE_COUNT(laguna_spl_test);

	ut_run_list("safety-iram", "test_spl_", tests, n_ents, "safety_iram");
}
#endif

static void spl_lua_test(void)
{
#ifdef CONFIG_SPL_LUA_OCM_TEST
	spl_test_npu_ocm();
#endif

#ifdef CONFIG_SPL_LUA_IRAM_TEST
	spl_test_safety_iram();
#endif
}
#else
static inline void spl_lua_test(void) { }
#endif

#ifdef CONFIG_SPL_BOARD_INIT
static inline void wait_for_uboot(void)
{
#if defined(CONFIG_LUA_WAIT_UBOOT_LOAD)
	boot_params_t *bp = boot_params_get_base();
	uint32_t magic = bp->magic;
	ulong timeout = 0;
	ulong elapsed = 0, start;

	/* Wait for U-Boot to load */
	do {
		if (elapsed >= timeout) {
			printf("Waiting for U-Boot image to load into DDR...\n");
			timeout += 5000;
			magic = bp->magic;
			debug("magic = 0x%x\n", magic);
		}

		start = get_timer(0);
		udelay(1000);
		invalidate_dcache_range((ulong)bp, sizeof(boot_params_t));
		magic = bp->magic;
		elapsed += get_timer(start);
	} while (magic != M57H_SOC_MAGIC);
#endif
}

void spl_board_init(void)
{
	boot_params_t *bp = boot_params_get_base();
	const char *name = NULL;
	int bootdev, bootstate;

	bootstate = get_bootstate();
	if (bootstate > 0) {
		const char *state = bootstate == BOOTSTATE_POWERUP ?
		                           "Normal Boot" : "Download";
		printf("Boot State:    %s (0x%x)\n", state, bootstate);
	} else {
		printf("Boot State:    Unknown\n");
	}

	if (bootstate == BOOTSTATE_POWERUP && check_bootparams(true)) {
		pr_err("Boot params check failed, handup...\n");
		hang();
	}
	memset(&bp->fdt_addr, 0, sizeof(*bp) - offsetof(boot_params_t, fdt_addr));

	bootdev = get_bootdevice(&name);
	if (bootdev > 0) {
		printf("Boot Device:   %s [ID = %d]\n", name, bootdev);
		bp->bootdevice = bootdev;
	} else {
		printf("Boot Device:   Unknown\n");
	}

	if (bootstate == BOOTSTATE_DOWNLOAD)
		bp->bootdevice = BOOTDEVICE_OCM;

#if CONFIG_IS_ENABLED(ENV_SUPPORT)
	env_relocate();
	set_bootdevice_env(bp->bootdevice);

	if (is_secure_boot())
		set_secureboot_env(true);
#endif

	spl_lua_test();

	wait_for_uboot();
}
#endif

#ifdef CONFIG_SPL_DISPLAY_PRINT
static inline unsigned long read_midr(void)
{
	unsigned long val;

	asm volatile("mrs %0, midr_el1" : "=r" (val));

	return val;
}

#define MPIDR_HWID_BITMASK	UL(0xff00ffffff)
void spl_display_print(void)
{
	unsigned long mpidr = read_mpidr() & MPIDR_HWID_BITMASK;

	printf("Boot SPL on physical CPU%lx [0x%08lx]\n",
	                   mpidr, read_midr());
	printf("EL level:      EL%x\n", current_el());

#ifdef CONFIG_LUA_GICV2_LOWLEVEL_INIT
	printf("GICv2:         enabled\n");
#endif

#ifdef CONFIG_LUA_SECURE_BOOT
	printf("Secure Boot:   %s\n", is_secure_boot() ?
	                        "enabled" : "disabled");
#endif
}
#endif

void spl_perform_fixups(struct spl_image_info *spl_image)
{
	if (spl_image->flags & SPL_KIMAGE_FOUND)
		return;

	if (IS_ENABLED(CONFIG_SPL_ATF) &&
		    spl_image->boot_device == BOOT_DEVICE_RAM
		    && !(spl_image->flags & SPL_FIT_FOUND)
		    && (current_el() == 3)) {
		spl_image->os = IH_OS_ARM_TRUSTED_FIRMWARE;
		spl_image->name = "ARM Trusted Firmware";
	}

#if CONFIG_IS_ENABLED(LOAD_FIT) || CONFIG_IS_ENABLED(LOAD_FIT_FULL)
	boot_params_t *bp = boot_params_get_base();

	if (spl_image->fdt_addr)
		bp->fdt_addr = spl_image->fdt_addr;
#endif
}

#if defined(CONFIG_SPL_MULTI_MMC) || defined(CONFIG_SPL_MULTI_MTD)      \
     || defined(CONFIG_SPL_MULTI_BLK) || defined(CONFIG_SPL_MULTI_FAT)  \
     || defined(CONFIG_SPL_MULTI_UART)
int spl_board_perform_legacy_fixups(struct spl_image_info *spl_image)
{
	if (env_get_yesno("secureboot") == 1)
		return -EPERM;

	switch (spl_image->os) {
#if IS_ENABLED(CONFIG_LUA_ATF_SUPPORT)
	case IH_OS_ARM_TRUSTED_FIRMWARE:
		spl_image->load_addr = CONFIG_LUA_SPL_ATF_LOAD_ADDR;
		spl_image->entry_point = CONFIG_LUA_SPL_ATF_LOAD_ADDR;
		break;
#endif
#if IS_ENABLED(CONFIG_LUA_OPTEE_SUPPORT)
	case IH_OS_TEE:
		spl_image->load_addr = CONFIG_LUA_SPL_OPTEE_LOAD_ADDR;
		break;
#endif
	case IH_OS_U_BOOT:
		spl_image->load_addr = CONFIG_SYS_TEXT_BASE;
		spl_image->entry_point = CONFIG_SYS_TEXT_BASE;
	}

	/* offset load addr in order to reduce one memmove */
	spl_image->load_addr -= sizeof(image_header_t);

	return 0;
}

int spl_board_get_devnum(uint bootdev_type)
{
	ulong devnum;

	switch (bootdev_type) {
	case BOOT_DEVICE_MMC1:
		devnum = env_get_ulong(env_get_name(ENV_MMC_DEV), 10, ~0ULL);
		break;
	case BOOT_DEVICE_NOR:
		devnum = env_get_ulong(env_get_name(ENV_SAFE_MTD), 10, ~0ULL);
		break;
	case BOOT_DEVICE_NAND:
		devnum = env_get_ulong(env_get_name(ENV_MAIN_MTD), 10, ~0ULL);
		break;
	default:
		return -1;
	}

	if (unlikely(devnum == ~0ULL))
		return -1;

	return devnum;
}

#if defined(CONFIG_LUA_AB)
int spl_multi_mmc_ab_select(struct blk_desc *dev_desc)
{
	struct disk_partition info;
	int ret;

	if (!dev_desc)
		return -ENODEV;

	ret = part_get_info_by_name(dev_desc, "misc", &info);
	if (ret < 0) {
		debug("misc part not exist\n");
		return ret;
	}

	ret = ab_select_slot(dev_desc, &info);
	if (ret < 0)
		pr_err("failed to select ab slot\n");

	return ret;
}
#endif /* CONFIG_LUA_AB */
#endif /* CONFIG_SPL_MULTI_MMC */
