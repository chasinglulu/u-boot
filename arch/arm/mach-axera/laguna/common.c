/* SPDX-License-Identifier: GPL-2.0+
 *
 * Common board-specific code
 *
 * Copyright (c) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <asm/io.h>
#include <fdt_support.h>
#include <asm/sections.h>
#include <linux/sizes.h>
#include <cpu_func.h>
#include <dm/uclass.h>
#include <exports.h>
#include <env.h>
#include <init.h>
#include <log.h>
#include <stdio_dev.h>
#if IS_ENABLED(CONFIG_MTD) || IS_ENABLED(CONFIG_DM_MTD)
#include <mtd.h>
#endif
#if IS_ENABLED(CONFIG_CMD_NAND) && IS_ENABLED(CONFIG_MTD_RAW_NAND)
#include <nand.h>
#endif
#include <fs.h>
#include <android_ab.h>
#include <linux/ctype.h>
#include <hang.h>
#include <image.h>

#include <asm/arch/bootparams.h>
#include <asm/arch/cpu.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_OF_BOARD
void *board_fdt_blob_setup(int *err)
{
	void *fdt_addr;

	/* FDT is at end of image */
	fdt_addr = (ulong *)&_end;
	if (!fdt_check_header(fdt_addr)) {
		*err = 0;
		memmove(__bss_end, fdt_addr, fdt_totalsize(fdt_addr));
		memset(__bss_start, 0, __bss_end - __bss_start);
		return __bss_end;
	}

#ifndef CONFIG_SPL_BUILD
	boot_params_t *bp = boot_params_get_base();
	if (bp->fdt_addr && !fdt_check_header(bp->fdt_addr)) {
		*err = 0;
		return bp->fdt_addr;
	}

	/* QEMU loads a generated DTB for us at the start of DRAM. */
	fdt_addr = (void *)CONFIG_LUA_DRAM_BASE;
	if (!fdt_check_header(fdt_addr)) {
		*err = 0;
		return fdt_addr;
	}
#endif

	*err = -FDT_ERR_NOTFOUND;
	return NULL;
}
#endif

#ifdef CONFIG_LUA_SELECT_UART_FOR_CONSOLE
#if CONFIG_LUA_SAFETY_UART0
#define CONFIG_LUA_UART_INDEX    0
#elif CONFIG_LUA_MAIN_UART1
#define CONFIG_LUA_UART_INDEX    1
#elif CONFIG_LUA_MAIN_UART2
#define CONFIG_LUA_UART_INDEX    2
#endif

#if defined(CONFIG_ENV_IMPORT_FDT) && !defined(CONFIG_SPL_BUILD)
static int fdt_blob_setup_stdio(void *fdt, const char *name)
{
	int envoffset, i, offset;
	const char *namep, *data;
	int ret;

	envoffset = fdt_path_offset(fdt, CONFIG_ENV_FDT_PATH);
	if (envoffset < 0) {
		printf("failed to find '%s' node\n", CONFIG_ENV_FDT_PATH);
		return -EINVAL;
	}

	for (i = 0; i < MAX_FILES; i++) {
		for (offset = fdt_first_property_offset(fdt, envoffset);
			offset > 0;
			offset = fdt_next_property_offset(fdt, offset)) {
				data = fdt_getprop_by_offset(fdt, offset,
					&namep, NULL);

				if (!memcmp(stdio_names[i], namep, strlen(stdio_names[i])))
					break;
		}
		ret = fdt_setprop_string(fdt, envoffset, stdio_names[i], name);
		if (ret)
			return ret;
	}

	return 0;
}
#else
static inline int fdt_blob_setup_stdio(void *fdt, const char *name) { return 0; }
#endif

static int fdt_blob_setup_console(void *fdt)
{
	int aliasoffset, offset, node = -1;
	const char *str, *p, *name;
	char data[32];
	int namelen, len;
	int idx, ret;

	str = fdtdec_get_chosen_prop(fdt, "stdout-path");
	if (!str) {
		printf("failed to find 'stdout-path' property\n");
		return -EINVAL;
	}

	p = strchr(str, ':');
	namelen = p ? p - str : strlen(str);

	node = fdt_path_offset_namelen(fdt, str, namelen);
	if (node < 0) {
		printf("invaild path '%s'\n", str);
		return -EINVAL;
	}

	if (*str != '/') {
		/*
		 * Deal with things like
		 * stdout-path = "serial0:115200n8";
		 *
		 * We need to look up the alias and then follow it to
		 * the correct node.
		 */
		idx = str[namelen - 1] - '0';
		if (idx == CONFIG_LUA_UART_INDEX)
			return 0;

		assert(namelen < 32);
		memcpy(data, str, namelen);
		data[namelen - 1]= '0' + CONFIG_LUA_UART_INDEX;
		data[namelen] = '\0';
	} else {
		/*
		 * Deal with things like
		 * stdout-path = "/soc/uart@0E403000:115200n8";
		 * stdout-path = &serial1;
		 *
		 * We need to look up the path and then follow it to
		 * the correct node.
		 */
		aliasoffset = fdt_path_offset(fdt, "/aliases");
		if (aliasoffset < 0) {
			printf("failed to find '/aliases' node\n");
			return -EINVAL;
		}
		for (offset = fdt_first_property_offset(fdt, aliasoffset);
			offset > 0;
			offset = fdt_next_property_offset(fdt, offset)) {
				name = fdt_getprop_by_offset(fdt, offset,
					&p, NULL);

				if (!memcmp(name, str, namelen))
					break;
		}
		if (offset < 0) {
			printf("failed to find '%s' in aliases properties\n", str);
			return -ENODATA;
		}

		len = strlen(p);
		idx = p[len - 1] - '0';
		if (idx == CONFIG_LUA_UART_INDEX)
			return 0;

		memcpy(data, p, len);
		data[len - 1] = '0' + CONFIG_LUA_UART_INDEX;
		data[len] = '\0';
	}

	/* validate new uart name */
	node = fdt_path_offset_namelen(fdt, data, strlen(data));
	if (node < 0) {
		printf("Invaild '%s' node\n", data);
		return -ENODATA;
	}

#ifndef CONFIG_SPL_BUILD
	const char *nodename;
	nodename = fdt_get_alias_namelen(fdt, data, strlen(data));
	p = strrchr(nodename, '/');
	if (p)
		fdt_blob_setup_stdio(fdt, p + 1);
#endif

	/* update property data */
	node = fdt_path_offset(fdt, "/chosen");
	ret = fdt_setprop_string(fdt, node, "stdout-path", data);
	if (ret)
		return ret;

	return 0;
}
#else
static inline int fdt_blob_setup_console(void *fdt) { return 0; }
#endif

int fdtdec_board_setup(const void *blob)
{
#if CONFIG_IS_ENABLED(CONSOLE_RECORD_OUT_REUSE)
	boot_params_t *bp = boot_params_get_base();

	/* Copy console output settings */
	memcpy((void *)&gd->console_out,
	        (void *)&bp->spl_console_out, sizeof(gd->console_out));
#endif

	return fdt_blob_setup_console((void *)blob);
}

void reset_cpu(void)
{
	return;
}

#ifndef CONFIG_SPL_BUILD
/*
 * Get the top of usable RAM
 *
 */
ulong board_get_usable_ram_top(ulong total_size)
{
	/* TODO: detect size dynamically */
	return CONFIG_LUA_DRAM_BASE + SZ_4G - 1;
}

#ifdef CONFIG_LUA_AB
static int scan_dev_for_extlinux_ab(void)
{
	struct blk_desc *dev_desc;
	struct disk_partition part_info;
	char *devtype = NULL;
	char *devnum = NULL;
	char dev_part_str[32];
	bool fallback = false;
	int slot, ret;

	devtype = env_get("devtype");
	if (unlikely(!devtype)) {
		debug("Not Found 'devtype' environment variable\n");
		return -ENODATA;
	}

	devnum = env_get("devnum");
	if (unlikely(!devnum)) {
		debug("Not Found 'devnum' environment variable\n");
		return -ENODATA;
	}

retry:
	if (fallback) {
		/* Lookup the "misc_bak" partition */
		snprintf(dev_part_str, 32, "%s#misc_bak", devnum);
		debug("Using backup 'misc' partition\n");
	} else {
		/* Lookup the "misc" partition */
		snprintf(dev_part_str, 32, "%s#misc", devnum);
	}
	ret = part_get_info_by_dev_and_name_or_num(devtype, dev_part_str,
	                    &dev_desc, &part_info,
	                    false);
	if (ret < 0) {
		if (fallback) {
			pr_err("Not Found 'misc_bak' partition on '%s' device\n", devtype);
			return -ENODEV;
		} else {
			fallback = true;
			goto retry;
		}
	}

	ret = ab_select_slot(dev_desc, &part_info);
	if (ret < 0) {
		if (fallback) {
			pr_err("Invaild AB control in 'misc_bak' partition on '%s' device\n", devtype);
			return -EINVAL;
		} else {
			fallback = true;
			goto retry;
		}
	}
	slot = ret;

	/* Lookup the "bootable falg" partition */
	int part_count = 0;
	int part_map[MAX_SEARCH_PARTITIONS] = { 0 };
	for (int p = 1; p <= MAX_SEARCH_PARTITIONS; p++) {
		int r = part_get_info(dev_desc, p, &part_info);

		if (r != 0)
			continue;

		if (!part_info.bootable)
			continue;

		part_map[part_count++] = p;
	}
	if (part_count != 2) {
		if (fallback) {
			debug("Too many or few bootable partitions on '%s' device\n", devtype);
			return -EINVAL;
		} else {
			fallback = true;
			goto retry;
		}
	}

	ret = fs_set_blk_dev_with_part(dev_desc, part_map[slot]);
	if (ret < 0) {
		if (fallback) {
			part_get_info(dev_desc, part_map[slot], &part_info);
			pr_err("No filesystem exist in '%s (%d)' partition on '%s' device\n",
				part_info.name, part_map[slot], devtype);
			return -EBADSLT;
		} else {
			fallback = true;
			goto retry;
		}
	}
	fs_close();

	printf("Using booting slot: %c\n", toupper(BOOT_SLOT_NAME(slot)));
	env_set_hex("distro_bootpart", part_map[slot]);
	env_set("prefix", "/");

	return 0;
}
#else
static inline int scan_dev_for_extlinux_ab(void) { return 0; }
#endif

#ifdef CONFIG_BOARD_LATE_INIT
#ifdef CONFIG_LUA_R5F_DEMO_CV
#include "demo_cv.h"
void load_r5f_demo_into_ocm(void)
{
	printf("Loading R5F Demo into safety OCM...\n");
	writel(0, CONFIG_LUA_R5F_CPU_RELEASE_ADDR);
	memcpy((void *)MMAP_SAFETY_OCM_BASE, demo_cv_bin, demo_cv_bin_len);
	flush_dcache_all();
	printf("Release R5F CPU to run...\n");
	writel(MMAP_SAFETY_OCM_BASE, CONFIG_LUA_R5F_CPU_RELEASE_ADDR);
}
#else
void inline load_r5f_demo_into_ocm(void) {}
#endif
int board_late_init(void)
{
	int bootstate = get_bootstate();
	int bootdev = get_bootdevice(NULL);

	set_bootdevice_env(bootdev);
	switch (bootdev) {
	case BOOTDEVICE_ONLY_NAND:
	case BOOTDEVICE_BOTH_NOR_NAND:
		if (bootstate == BOOTSTATE_POWERUP)
			mtd_probe_devices();
		break;
	}

	if (bootstate == BOOTSTATE_POWERUP)
		scan_dev_for_extlinux_ab();

	if (bootstate == BOOTSTATE_DOWNLOAD)
		env_set("download", "yes");

	if (is_secure_boot()) {
		env_set("fdtcontroladdr", NULL);
		env_set("fdt_addr", NULL);
		env_set_hex("kernel_addr_r",
		         CONFIG_VAL(LUA_KERNEL_LOAD_ADDR) - SZ_32M);
		env_set("boot_syslinux_conf", "extlinux_sign.conf");

		set_secureboot_env(true);
		env_set("verify", "Yes");
	}

	load_r5f_demo_into_ocm();
	return 0;
}
#endif

#if defined(CONFIG_FDL)
static void enter_download_mode(void)
{
	int downif = get_downif();
	int ret;

	debug("%s: download interface: %d\n", __func__, downif);

	switch (downif) {
	case DOWNLOAD_IF_UART:
		printf("Enter UART Download Mode...\n");
		ret = fdl_uart_download(2, false);
		break;
	case DOWNLOAD_IF_USB:
		printf("Enter USB Download Mode...\n");
		ret = fdl_usb_download(0, false);
		break;
	default:
		debug("Not supported download interface '%d'\n", downif);
		ret = -EINVAL;
	}

	if (ret < 0) {
		pr_err("Unable to enter download mode, hang up...\n");
		hang();
	}
}
#else
static inline void enter_download_mode(void) { }
#endif

#ifdef CONFIG_LAST_STAGE_INIT
int last_stage_init(void)
{
	if (env_get_yesno("download") == 1)
		enter_download_mode();

	return 0;
}
#endif

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_SYSTEM_SETUP)

static int ft_system_reserve_memory(void *blob)
{
	struct fdt_memory carveout[] = {
#if IS_ENABLED(CONFIG_LUA_ATF_SUPPORT)
		{
			.start = CONFIG_LUA_SPL_ATF_LOAD_ADDR,
			.end = CONFIG_LUA_SPL_ATF_LOAD_ADDR + CONFIG_LUA_SPL_ATF_LOAD_SIZE - 1,
		},
#endif
#if IS_ENABLED(CONFIG_LUA_OPTEE_SUPPORT)
		{
			.start = CONFIG_LUA_SPL_OPTEE_LOAD_ADDR,
			.end = CONFIG_LUA_SPL_OPTEE_LOAD_ADDR + CONFIG_LUA_SPL_OPTEE_LOAD_SIZE - 1,
		},
#endif
#if IS_ENABLED(CONFIG_CONSOLE_RECORD_FIXED)
		{
			.start = CONFIG_CONSOLE_RECORD_FIXED_ADDR,
			.end = CONFIG_CONSOLE_RECORD_FIXED_ADDR + CONFIG_CONSOLE_RECORD_FIXED_SIZE - 1,
		},
#endif
	};
	const char *nodename[] = {
#if IS_ENABLED(CONFIG_LUA_ATF_SUPPORT)
		"atf_reserved",
#endif
#if IS_ENABLED(CONFIG_LUA_OPTEE_SUPPORT)
		"optee_reserved",
#endif
#if IS_ENABLED(CONFIG_CONSOLE_RECORD_FIXED)
		"bootlog_reserved",
#endif
	};
	unsigned long flags = FDTDEC_RESERVED_MEMORY_NO_MAP;
	int ret, i;

	assert(ARRAY_SIZE(carveout) == ARRAY_SIZE(nodename));

	for (i = 0; i < ARRAY_SIZE(carveout); i++) {
		ret = fdtdec_add_reserved_memory(blob,
			nodename[i], &carveout[i],
			NULL, 0, NULL, flags);
		if (ret < 0) {
			pr_err("failed to add %s memory: %d\n", nodename[i], ret);
			return ret;
		}
		pr_info("Add %s memory: 0x%llx-0x%llx\n",
			nodename[i], carveout[i].start, carveout[i].end);
	}

	return 0;
}

static int ft_system_hwinfo_bootlog(void *blob, int nodeoffset)
{
#if IS_ENABLED(CONFIG_CONSOLE_RECORD_FIXED)
	uint64_t addr = CONFIG_CONSOLE_RECORD_FIXED_ADDR;
	uint64_t size = CONFIG_CONSOLE_RECORD_FIXED_SIZE;
	fdt32_t cells[2] = {}, *ptr = cells;

	*ptr++ = cpu_to_fdt32(upper_32_bits(addr));
	*ptr++ = cpu_to_fdt32(lower_32_bits(addr));
	fdt_setprop(blob, nodeoffset, "axera,bootlog-record-addr", cells, 2 * sizeof(cells));

	ptr = cells;
	*ptr++ = cpu_to_fdt32(upper_32_bits(size));
	*ptr++ = cpu_to_fdt32(lower_32_bits(size));
	fdt_setprop(blob, nodeoffset, "axera,bootlog-record-len", cells, 2 * sizeof(cells));
#endif

	return 0;
}

static int ft_system_hwinfo(void *blob)
{
	int parent;
	fdt32_t boardid, bootdev;
	const char *chipname;
	const char *bootdev_name = NULL;

	parent = fdt_path_offset(blob, "/hw_info_display");
	if (parent < 0) {
		pr_err("failed to find '/hw_info_display' node\n");
		return -EINVAL;
	}

	boardid = cpu_to_fdt32(env_get_ulong("boardid", 10, -1));
	if (boardid == -1U)
		boardid = 0;

	chipname = env_get("chipname");
	if (!chipname)
		chipname = "Laguna";

	fdt_setprop(blob, parent, "axera,board-id", &boardid, sizeof(boardid));
	fdt_setprop_string(blob, parent, "axera,chip-name", chipname);

	ft_system_hwinfo_bootlog(blob, parent);

	bootdev = cpu_to_fdt32(get_bootdevice(&bootdev_name));
	if (bootdev_name) {
		fdt_setprop_string(blob, parent, "axera,boot-device", bootdev_name);
		fdt_setprop(blob, parent, "axera,boot-device-id", &bootdev, sizeof(bootdev));
	}

	return 0;
}

int ft_system_setup(void *blob, struct bd_info *bd)
{
	ft_system_reserve_memory(blob);
	ft_system_hwinfo(blob);

	return 0;
}
#endif

int dram_init(void)
{
	if (fdtdec_setup_mem_size_base() != 0)
		return -EINVAL;

	return 0;
}

int dram_init_banksize(void)
{
	fdtdec_setup_memory_banksize();

	return 0;
}

int board_init(void)
{
#if IS_ENABLED(CONFIG_DM_SPI_FLASH) && IS_ENABLED(CONFIG_SPI_FLASH_MTD)
	struct udevice *dev;

	/* probe all SPI NOR Flash device */
	uclass_foreach_dev_probe(UCLASS_SPI_FLASH, dev)
		;
#endif

	return 0;
}

#if IS_ENABLED(CONFIG_CMD_NAND) && IS_ENABLED(CONFIG_MTD_RAW_NAND)
void board_nand_init(void)
{
	struct udevice *dev;
	struct mtd_info *mtd;
	char mtd_name[20];
	int i;

	/* probe all SPI NAND Flash devices */
	uclass_foreach_dev_probe(UCLASS_MTD, dev)
		debug("SPI NAND Flash device = %s\n", dev->name);

	for (i = 0; i < CONFIG_SYS_MAX_NAND_DEVICE; i++) {
		snprintf(mtd_name, sizeof(mtd_name), "spi-nand%d", i);
		mtd = get_mtd_device_nm(mtd_name);
		if (IS_ERR_OR_NULL(mtd)) {
			debug("MTD device %s not found, ret %ld\n", mtd_name,
				PTR_ERR(mtd));
			return;
		}

		nand_register(0, mtd);
	}
}
#endif
#endif /* !CONFIG_SPI_BUILD */

void board_prep_linux(bootm_headers_t *images)
{
	ulong bootdev = env_get_ulong(env_get_name(ENV_BOOTDEVICE), 10, ~0ULL);

	if (bootdev == ~0ULL) {
		debug("Not found 'bootdevice' environment variable\n");
		return;
	}

	remove_mtd_device(bootdev);
}
