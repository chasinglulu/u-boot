/* SPDX-License-Identifier: GPL-2.0+
 *
 * Common SoC-specific code
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
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
#include <u-boot/crc.h>

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
#if CONFIG_IS_ENABLED(LUA_BOOTPARAMS_VERIFY)
	int bootstate = get_bootstate();

	if (bootstate == BOOTSTATE_POWERUP && check_bootparams(false)) {
		pr_err("Invalid boot parameters\n");
		return -EINVAL;
	}
#endif

#if CONFIG_IS_ENABLED(CONSOLE_RECORD_OUT_REUSE)
	boot_params_t *bp = boot_params_get_base();

	/* Copy console output settings */
	memcpy((void *)&gd->console_out,
	        (void *)&bp->spl_console_out, sizeof(gd->console_out));
#endif

	return fdt_blob_setup_console((void *)blob);
}

void fdl_reset_misc(void)
{
	set_bootstrap(0x10);
}

void fdl_sd_complete(void)
{
	printf("\n\nFDL downloading via SDcard completed. Rebooting...\n\n");
	mdelay(50);
	fdl_reset_misc();
	reset_cpu();
}

int fdl_uart_get_baudrate(void)
{
	boot_params_t *bp = boot_params_get_base();

	if (bp->fdl_chg_baud)
		return bp->fdl_chg_baud;

	return gd->baudrate;
}

void reset_misc(void)
{
	int ret;

	ret = safety_abc_setup(AB_MARK_SUCCESSFUL, 0);
	if (ret < 0) {
		pr_err("Could not setup safety abc value\n");
		return;
	}
}

#if defined(CONFIG_LUA_BOOTPARAMS_VERIFY)
int check_bootparams(bool is_sbl)
{
	boot_params_t *bp = boot_params_get_base();
	uint32_t crc32_backup = 0;
	uint32_t calc_crc32;
	size_t size;

	size = is_sbl ? sizeof(bp->reserved) : sizeof(boot_params_t);
	crc32_backup = le32_to_cpu(bp->crc32);
	bp->crc32 = 0;

	calc_crc32 = crc32(0, (const unsigned char *)bp,
	                      size);

	debug("CRC: expected=%.8x, found=%.8x\n",
	         calc_crc32, le32_to_cpu(crc32_backup));
	if (calc_crc32 != le32_to_cpu(crc32_backup)) {
		pr_err("Invalid CRC-32 (expected %.8x, found %.8x)\n",
		       calc_crc32, le32_to_cpu(crc32_backup));
		return -EINVAL;
	}

	return 0;
}

void calc_bootparams_crc32(bool is_sbl)
{
	boot_params_t *bp = boot_params_get_base();
	uint32_t calc_crc32;
	size_t size;

	bp->crc32 = 0;

	size = is_sbl ? sizeof(bp->reserved) : sizeof(boot_params_t);

	calc_crc32 = crc32(0, (const unsigned char *)bp,
	              size);
	bp->crc32 = cpu_to_le32(calc_crc32);
	debug("%s: CRC32: %.8x\n", __func__, le32_to_cpu(bp->crc32));
}
#else
inline int check_bootparams(bool is_sbl) { return 0; }
inline void calc_bootparams_crc32(bool is_sbl) { }
#endif /* CONFIG_LUA_BOOTPARAMS_VERIFY */

static inline const char *get_blk_ifname(void)
{
	const char *varname;
	const char *ifname = NULL;

	varname = env_get_name(ENV_DEVTYPE);
	ifname = env_get(varname);
	if (!ifname) {
		debug("'%s' environment variable not found\n", varname);
		return NULL;
	}

	if (!strcmp(ifname, "mtd")) {
		mtd_probe_devices();
	}

	return ifname;
}


#ifndef CONFIG_SPL_BUILD
/*
 * Get the top of usable RAM
 *
 */
ulong board_get_usable_ram_top(ulong total_size)
{
	return gd->ram_base + gd->ram_size - 1;
}

#ifdef CONFIG_LUA_AB
static int scan_dev_for_extlinux_ab(void)
{
	boot_params_t *bp = NULL;
	struct blk_desc *dev_desc = NULL;
	struct disk_partition part_info = { 0 };
	char *devtype = NULL;
	ulong devnum;
	int slot, ret;

	devtype = env_get(env_get_name(ENV_DEVTYPE));
	if (unlikely(!devtype)) {
		debug("'devtype' environment variable not found\n");
		return -ENODATA;
	}

	devnum = env_get_ulong(env_get_name(ENV_DEVNUM), 10, ~0UL);
	if (devnum == ~0UL) {
		debug("devnum' environment variable not found\n");
		return -ENODATA;
	}

	dev_desc = blk_get_dev(devtype, devnum);
	if (IS_ERR_OR_NULL(dev_desc)) {
		debug("Failed to get '%s %lu' block device\n",
		      devtype, devnum);
		return PTR_ERR(dev_desc);
	}

	bp = boot_params_get_base();
	slot = bp->slot;
	if (slot < 0 || slot >= 2) {
		debug("Invalid slot '%d'\n", slot);
		return -EINVAL;
	}

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
		debug("Too many or few bootable partitions\n");
		return -EINVAL;
	}

	ret = part_get_info(dev_desc, part_map[slot], &part_info);
	if (ret < 0) {
		debug("Unable to get '%d' partition information on '%s' device\n",
		           part_map[slot], devtype);
		return ret;
	}
	ret = fs_set_blk_dev_with_part(dev_desc, part_map[slot]);
	if (ret < 0) {
		pr_err("No filesystem exist in '%s (%d)' partition on '%s' device\n",
			part_info.name, part_map[slot], devtype);
		return -ENOENT;
	}
	fs_close();

	printf("Using booting slot: %c\n", SLOT_NAME(slot));
	env_set_ulong("bootslot", slot);
	env_set_hex("distro_bootpart", part_map[slot]);
	env_set("prefix", "/");

	return 0;
}
#else
static inline int scan_dev_for_extlinux_ab(void) { return 0; }
#endif

#ifndef BUILD_DATE
#define BUILD_DATE NULL
#endif

#ifdef CONFIG_LUA_SDK_VERSION
static char axera_version[32];
static inline void create_axera_version(const char *build_date)
{
	if (build_date)
		snprintf(axera_version, sizeof(axera_version), "%s_%s",
		                CONFIG_VAL(LUA_SDK_VERSION), build_date);
	else
		snprintf(axera_version, sizeof(axera_version), "%s",
		                           CONFIG_VAL(LUA_SDK_VERSION));

	axera_version[strlen(axera_version)] = '\0';
}
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
	int ret;

	if (is_bootdev_env_ready())
		set_bootdev_env_ready(false);

	set_bootdevice_env(bootdev);
	switch (bootdev) {
	case BOOTDEVICE_ONLY_NAND:
	case BOOTDEVICE_BOTH_NOR_NAND:
		if (bootstate == BOOTSTATE_POWERUP)
			mtd_probe_devices();
		break;
	}

	if (bootstate == BOOTSTATE_POWERUP) {
		ret = scan_dev_for_extlinux_ab();
#if defined(CONFIG_LUA_STRICT_CHECK)
		if (ret < 0) {
			pr_err("Unable to find extlinux on device (ret = %d).\n", ret);
			return ret;
		}
#endif
	}

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

#ifdef CONFIG_LUA_SDK_VERSION
	create_axera_version(BUILD_DATE);
	env_set("sdk_version", axera_version);
#endif

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
	case DOWNLOAD_IF_SD:
		printf("Enter SD Download Mode...\n");
		ret = fdl_sd_download(-1, false);
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

#if defined(CONFIG_USE_BOOTARGS)
static void bootargs_merge_unique(char *bootargs, size_t size, const char *src)
{
	if (!src || !*src)
		return;

	const int max_args = 128;
	const char *args[max_args];
	char buf[CONFIG_SYS_CBSIZE] = { 0 };
	char *token = NULL;
	int lens[max_args];
	int count = 0;

	for (char *p = bootargs; *p && count < max_args;) {
		while (*p == ' ') p++;
		if (!*p) break;
		args[count] = p;
		int len = 0;
		while (p[len] && p[len] != ' ') len++;
		lens[count++] = len;
		p += len;
	}

	strncpy(buf, src, CONFIG_SYS_CBSIZE);
	for (token = strtok(buf, " "); token; token = strtok(NULL, " ")) {
		int tlen = strlen(token);
		int dup = 0;
		for (int i = 0; i < count; ++i) {
			if (lens[i] == tlen && !strncmp(token, args[i], tlen)) {
				dup = 1;
				break;
			}
		}
		if (!dup) {
			if (bootargs[0] != '\0')
				strncat(bootargs, " ", size - strlen(bootargs) - 1);
			strncat(bootargs, token, size - strlen(bootargs) - 1);
			if (count < max_args) {
				args[count] = bootargs + strlen(bootargs) - tlen;
				lens[count++] = tlen;
			}
		}
	}
}

char *board_fdt_chosen_bootargs(void)
{
	char bootargs[CONFIG_SYS_CBSIZE] = { 0 };
	const char *extra_args = NULL;
	const char *orig_args = NULL;
	const char *dtb_args = NULL;
	const char *dm_verity_args = NULL;
	char *fdt_addr = images.ft_addr;
	int nodeoffset;

	if (!fdt_addr) {
		pr_warn("No FDT found, unable to set bootargs.\n");
		return env_get("bootargs");
	}

	nodeoffset = fdt_path_offset(fdt_addr, "/chosen");
	if (nodeoffset < 0) {
		debug("No '/chosen' node found in FDT\n");
		return NULL;
	}

	orig_args = env_get("bootargs");
	dtb_args = fdt_getprop(fdt_addr, nodeoffset, "bootargs", NULL);
	extra_args = env_get("extra_args");

	bootargs_merge_unique(bootargs, sizeof(bootargs), dtb_args);
	bootargs_merge_unique(bootargs, sizeof(bootargs), orig_args);
	bootargs_merge_unique(bootargs, sizeof(bootargs), extra_args);

	dm_verity_args = env_get("dm_verity_args");
	if (dm_verity_args) {
		strncat(bootargs, " ", CONFIG_SYS_CBSIZE);
		strcat(bootargs, dm_verity_args);
	}

	env_set("bootargs", bootargs);

	return env_get("bootargs");
}
#endif

#ifdef CONFIG_LAST_STAGE_INIT
static int loglevel_env_set(void)
{
	const char *loglevel = env_get("loglevel");

	if (!loglevel) {
		debug("Setting default loglevel to '10'\n");
		env_set("loglevel", "10");
		return 0;
	}

	return 0;
}

static int rootfs_env_set(void)
{
	const char *varname = NULL;
	const char *devtype = NULL;
	const char *partname = NULL;
	struct blk_desc *dev_desc = NULL;
	struct disk_partition part_info = { 0 };
	char rootname[PART_NAME_LEN] = { 0 };
	char misc[PART_NAME_LEN] = { 0 };
	const char *rootfstype = NULL;
	int fstype = FS_TYPE_ANY;
	int part, bootdev, ret;

	ulong devnum;

	varname = env_get_name(ENV_DEVTYPE);
	devtype = env_get(varname);
	if (unlikely(!devtype)) {
		debug("'%s' environment variable not found\n", varname);
		return -ENODATA;
	}

	varname = env_get_name(ENV_DEVNUM);
	devnum = env_get_ulong(varname, 10, ~0UL);
	if (devnum == ~0UL) {
		debug("'%s' environment variable not found\n", varname);
		return -ENODATA;
	}

	dev_desc = blk_get_dev(devtype, devnum);
	if (IS_ERR_OR_NULL(dev_desc)) {
		debug("Unable to get '%s %lu' block device\n",
		               devtype, devnum);
		return -ENODEV;
	}

	partname = get_rootfs_part_name();
	if (unlikely(!partname)) {
		debug("Unable to get rootfs partition name\n");
		return -ENODATA;
	}
	part = part_get_info_by_name(dev_desc, partname, &part_info);
	if (part < 0) {
		debug("Unable to get '%s' partition information on '%s' device\n",
		           partname, devtype);
		return part;
	}
	debug("%s: part: %d\n", __func__, part);

	bootdev = get_bootdevice(NULL);
	debug("%s: bootdev: %d\n", __func__, bootdev);
	if (unlikely(bootdev < 0)) {
		debug("Unable to get boot device\n");
		return -ENODEV;
	}

	ret = fs_set_blk_dev_with_part(dev_desc, part);
	if (likely(ret < 0)) {
		debug("No filesystem probed in '%s (%d)' partition on '%s' device\n",
		               part_info.name, part, devtype);
	} else {
		fstype = fs_get_type();
	}

	switch (bootdev) {
	case BOOTDEVICE_ONLY_NAND:
	case BOOTDEVICE_BOTH_NOR_NAND:
	case BOOTDEVICE_ONLY_NOR:
	case BOOTDEVICE_BOTH_NOR_NOR:
	case BOOTDEVICE_ONLY_HYPER:
	case BOOTDEVICE_BOTH_NOR_HYPER:
		if (fstype == FS_TYPE_ANY) {
			env_set("rootfstype", "ubifs");
			env_set("root", "ubi0:rootfs");
			env_set("access", "rw");
			snprintf(misc, sizeof(misc), "ubi.mtd=%s", part_info.name);
			strncat(misc, " rootwait", 10);
			env_set("misc", misc);
		} else if (fstype == FS_TYPE_SQUASHFS) {
			env_set("access", "ro");
			env_set("rootfstype", "squashfs");
			snprintf(rootname, sizeof(rootname),
			               "/dev/mtdblock%d", part);
			env_set("root", rootname);
			env_set("misc", "rootwait");
		} else {
			pr_err("Unsupported filesystem type '%d' (bootdev = %d)\n",
			       fstype, bootdev);
			return -EINVAL;
		}
		return 0;
	case BOOTDEVICE_ONLY_EMMC:
	case BOOTDEVICE_BOTH_NOR_EMMC:
	case BOOTDEVICE_BOTH_NAND_EMMC:
	case BOOTDEVICE_BOTH_HYPER_EMMC:
		if (fstype == FS_TYPE_ANY) {
			pr_err("Unable to probe filesystem type (bootdev = %d)\n", bootdev);
			return -EINVAL;
		}
		break;
	default:
		pr_err("Unsupported boot device '%d'\n", bootdev);
		return -EINVAL;
	}

	switch (fstype) {
	case FS_TYPE_EXT:
	case FS_TYPE_SQUASHFS:
		rootfstype = fs_get_type_name();
		env_set("rootfstype", rootfstype);
		snprintf(rootname, sizeof(rootname), "/dev/mmcblk%lup%d",
		                            devnum, part);
		env_set("root", rootname);
		env_set("misc", "rootwait");
		if (fstype == FS_TYPE_SQUASHFS)
			env_set("access", "ro");
		else
#if defined(CONFIG_LUA_DM_VERITY)
			env_set("access", "ro");
#else
			env_set("access", "rw");
#endif
		break;
	default:
		pr_err("Unsupported file system type '%d'\n", fstype);
		return -EINVAL;
	}

	return 0;
}

int last_stage_init(void)
{
	const char * const vars[] = {
		"bootcmd",
		"distro_bootcmd",
		"bootcmd_blk_extlinux",
		"bootargs",
	};
	int ret;

	if (env_get_yesno("download") == 1)
		enter_download_mode();

	env_set_default_vars(ARRAY_SIZE(vars), (char * const *)vars, 0);
	loglevel_env_set();
	ret = rootfs_env_set();
	if (ret < 0)
		pr_warn("Unable to set rootfs releated environment variables (ret = %d)\n", ret);

#ifdef CONFIG_LUA_DM_VERITY
	int bootstate = get_bootstate();
	if (bootstate == BOOTSTATE_POWERUP) {
		ret = verify_rootfs_dm_verity();
		if (ret < 0) {
			pr_err("Unable to verify rootfs verity (ret = %d)\n", ret);
			return ret;
		}
	}
#endif

	fs_close();
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
	uint64_t size = console_record_avail() + 22;
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
	int32_t safety_abort, main_abort;
	const char *safety_abort_str = NULL;
	const char *main_abort_str = NULL;
	ulong slot;

	parent = fdt_path_offset(blob, "/hw_info_display");
	if (parent < 0) {
		pr_err("failed to find '/hw_info_display' node\n");
		return -EINVAL;
	}

	boardid = cpu_to_fdt32(env_get_ulong("boardid", 10, -1));
	if (boardid == -1U)
		boardid = 0;

#ifdef CONFIG_LUA_SDK_VERSION
	fdt_setprop_string(blob, parent, "axera,version",
	                                      axera_version);
#endif

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

	safety_abort = get_abort(true, &safety_abort_str);
	main_abort = get_abort(false, &main_abort_str);
	if (safety_abort > 0) {
		safety_abort = cpu_to_fdt32(safety_abort);
		fdt_setprop(blob, parent, "axera,safety-abort",
		         &safety_abort, sizeof(safety_abort));
		if (safety_abort_str)
			fdt_setprop_string(blob, parent, "axera,safety-abort-name", safety_abort_str);
	}
	if (main_abort > 0) {
		main_abort = cpu_to_fdt32(main_abort);
		fdt_setprop(blob, parent, "axera,main-abort",
		         &main_abort, sizeof(main_abort));
		if (main_abort_str)
			fdt_setprop_string(blob, parent, "axera,main-abort-name", main_abort_str);
	}

	slot = env_get_ulong("bootslot", 10, ~0UL);
	if (slot != ~0UL) {
		fdt_setprop_u32(blob, parent, "axera,boot-slot", slot);
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

#if !defined(CONFIG_ARCH_FIXUP_FDT)
int arch_fixup_fdt(void *blob)
{
	return 0;
}
#endif

int board_init(void)
{
	int bootdev = get_bootdevice(NULL);

	probe_all_spi_nor_devs();

	env_set_default(NULL, 0);
	set_bootdevice_env(bootdev);

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

void board_cleanup_before_linux(void)
{
	const char *var_name;
	ulong bootdev;

#if defined(CONFIG_LUA_AB)
	/*
	 * FIXME:
	 * Mark Slot A/B successful
	 *
	 * This call is temporary. It should be moved to the rootfs init script
	 * to mark the slot as successful after the kernel boots.
	 */
	abc_mark_bootable(true);
#endif

	var_name = env_get_name(ENV_BOOTDEVICE);
	bootdev = env_get_ulong(var_name, 10, ~0ULL);
	if (bootdev == ~0ULL) {
		debug("Not found '%s' environment variable\n", var_name);
		return;
	}
	remove_mtd_device(bootdev);
}

#if defined(CONFIG_ENV_IS_IN_BLK)
const char *env_blk_get_intf(void)
{
	return get_blk_ifname();
}

const char *env_blk_get_dev_part(int copy, bool only_dev)
{
	static char dev_part_str[32];
	const char *varname;

	varname = env_get_name(ENV_DEVNUM);
	if (only_dev) {
		return env_get(varname);
	}

	snprintf(dev_part_str, sizeof(dev_part_str), "%s#%s",
				env_get(varname), copy == 1 ? "ubootenv_bak" : "ubootenv");

	debug("%s: dev_part_str = %s\n", __func__, dev_part_str);
	return dev_part_str;
}
#endif /* CONFIG_ENV_IS_IN_BLK */

#endif /* !CONFIG_SPI_BUILD */

const char *bootloader_message_ab_get_blk_ifname(void)
{
	return get_blk_ifname();
}

const char *bootloader_message_ab_get_blk_dev_part(int copy)
{
	const char *varname;
	static char dev_part_str[32];

	varname = env_get_name(ENV_DEVNUM);
	snprintf(dev_part_str, sizeof(dev_part_str), "%s#%s",
	               env_get(varname), copy == 1 ? "misc_bak" : "misc");

	debug("%s: dev_part_str = %s\n", __func__, dev_part_str);
	return dev_part_str;
}

int abc_mark_bootable(bool okay)
{
	ulong slot;
	int ret;

	slot = env_get_ulong("bootslot", 10, ~0ULL);
	if (slot == ~0ULL) {
		debug("Not found 'bootslot' environment variable\n");
		return -EINVAL;
	}
	debug("boot slot: %lu\n", slot);

	if (okay)
		ret = ab_control_mark_successful(!!slot);
	else
		ret = ab_control_mark_unbootable(!!slot);
	if (ret < 0) {
		debug("Failed to mark slot %c %s\n",
		     SLOT_NAME(!!slot), okay ? "successful" : "unbootable");
		return ret;
	}
	printf("Mark slot %c %s\n",
	         SLOT_NAME(!!slot), okay ? "successful" : "unbootable");

	return 0;
}