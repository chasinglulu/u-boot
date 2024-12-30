#ifndef ASM_LAGUNA_BOOTPARAM_H_
#define ASM_LAGUNA_BOOTPARAM_H_

typedef struct boot_params {
	void *fdt_addr;
	unsigned short bootdevice;
} boot_params_t;


static inline boot_params_t *boot_params_get_base(void)
{
	/* TODO: detect base during runtime */
	return (boot_params_t *)CONFIG_LUA_IRAM_BASE;
}

enum {
	BOOTDEVICE_ONLY_EMMC = 0x1,
	/* Main OSPI interface */
	BOOTDEVICE_ONLY_NOR,
	BOOTDEVICE_ONLY_NAND,
	BOOTDEVICE_ONLY_HYPER,

	/* Safety QSPI + Main OSPI interface */
	BOOTDEIVCE_BOTH_NOR_NOR,
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
};

enum {
	BOOTMODE_DOWNLOAD,
	BOOTMODE_BOOTUP,
};

enum {
	DOWNLOAD_IF_UART,
	DOWNLOAD_IF_USB,
	DOWNLOAD_IF_CAN,
	DOWNLOAD_IF_SD,
};

extern const char *safe_part_id[];
extern const char *main_part_id[];
int get_safe_part_id_count(void);
int get_main_part_id_count(void);
int get_bootdevice(const char **name);
int set_bootdevice_env(int bootdev);

#include <blk.h>
#include <part.h>
#include <fdl.h>

struct blk_desc *get_blk_by_devnum(enum if_type if_type, int devnum);
int get_part_by_name(struct blk_desc *dev_desc, const char *name,
                      struct disk_partition *part);
int partitions_id_check(struct fdl_part_table *tab);

#endif /* ASM_LAGUNA_BOOTPARAM_H_ */