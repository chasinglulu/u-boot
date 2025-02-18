#ifndef ASM_LAGUNA_BOOTPARAM_H_
#define ASM_LAGUNA_BOOTPARAM_H_

#include <membuff.h>
typedef struct boot_params {
	void *fdt_addr;
	unsigned short bootdevice;
	struct membuff spl_console_out;
} boot_params_t;

static inline boot_params_t *boot_params_get_base(void)
{
	/* TODO: detect base during runtime */
	return (boot_params_t *)CONFIG_LUA_IRAM_BASE;
}

enum {
	/* Main SDHCI interface */
	BOOTDEVICE_ONLY_EMMC = 1,
	/* Main OSPI interface */
	BOOTDEVICE_ONLY_NOR,
	BOOTDEVICE_ONLY_NAND,
	BOOTDEVICE_ONLY_HYPER,

	/* Safety QSPI + Main OSPI interface */
	BOOTDEVICE_BOTH_NOR_NOR,
	BOOTDEVICE_BOTH_NOR_NAND,
	BOOTDEVICE_BOTH_NOR_HYPER,
	BOOTDEVICE_BOTH_NAND_NOR,
	BOOTDEVICE_BOTH_NAND_NAND,
	BOOTDEVICE_BOTH_NAND_HYPER,
	BOOTDEVICE_BOTH_HYPER_NOR,
	BOOTDEVICE_BOTH_HYPER_NAND,
	BOOTDEVICE_BOTH_HYPER_HYPER,

	/* Safety QSPI + Main SDHCI interface */
	BOOTDEVICE_BOTH_NOR_EMMC,
	BOOTDEVICE_BOTH_NAND_EMMC,
	BOOTDEVICE_BOTH_HYPER_EMMC,

	/* Special interface */
	BOOTDEVICE_UART,
	BOOTDEVICE_USB,
	BOOTDEVICE_SD,

	/* for FDL2 */
	BOOTDEVICE_OCM,
};

enum {
	DOWNLOAD_IF_UNKNOWN,
	DOWNLOAD_IF_CAN,
	DOWNLOAD_IF_UART,
	DOWNLOAD_IF_SD,
	DOWNLOAD_IF_USB,
};

/* boot state after poweron */
enum {
	BOOTSTATE_POWERUP = 1,
	BOOTSTATE_DOWNLOAD,
};

/* ASIC bootstrap PINs */
enum {
	BOOTSTRAP_SAFETY_NOR = 0,
	BOOTSTRAP_SAFETY_NAND_2K,
	BOOTSTRAP_SAFETY_NAND_4K,
	BOOTSTRAP_SAFETY_NAND_2KMP,
	BOOTSTRAP_SAFETY_NAND_4KMP,
	BOOTSTRAP_SAFETY_COUNT,

	BOOTSTRAP_MAIN_EMMC_DUAL_BOOT = 7,
	BOOTSTRAP_MAIN_NOR,
	BOOTSTRAP_MAIN_NAND_2K,
	BOOTSTRAP_MAIN_NAND_4K,
	BOOTSTRAP_MAIN_NAND_2KMP,
	BOOTSTRAP_MAIN_NAND_4KMP,
	BOOTSTRAP_MAIN_EMMC_BOOT,
	BOOTSTRAP_MAIN_EMMC,
	BOOTSTRAP_MAIN_HYPER,
	BOOTSTRAP_DOWNLOAD,
	BOOTSTRAP_COUNT,
};

/* Main domain bootmode selector */
enum {
	BOOTMODE_MAIN_NOR = 0,
	BOOTMODE_MAIN_NAND,
	BOOTMODE_MAIN_HYPER,
	BOOTMODE_MAIN_EMMC,
	BOOTMODE_MAIN_COUNT,
};

extern const char *safe_part_id[];
extern const char *main_part_id[];
int get_safe_part_id_count(void);
int get_main_part_id_count(void);
int get_bootdevice(const char **name);
int set_bootdevice_env(int bootdev);
int get_bootstate(void);
int get_downif(void);
bool is_secure_boot(void);
void set_secureboot_env(bool secure);

#include <blk.h>
#include <part.h>
#include <fdl.h>

struct blk_desc *get_blk_by_devnum(enum if_type if_type, int devnum);
int get_part_by_name(struct blk_desc *dev_desc, const char *name,
                      struct disk_partition *part);
int partitions_id_check(struct fdl_part_table *tab);

#endif /* ASM_LAGUNA_BOOTPARAM_H_ */