#ifndef ASM_LAGUNA_BOOTPARAM_H_
#define ASM_LAGUNA_BOOTPARAM_H_

#include <linux/types.h>
#include <membuff.h>

typedef struct boot_params {
	union {
		uint8_t reserved[3072];
		uint32_t crc32;

		struct {
			union {
				struct {
					uint32_t magic;
					uint32_t board_id;
					/**
					* @brief The mcu slot information passed from MCU to Acore
					*        BIT 0: 0 for slot A, 1 for slot B.
					*        BIT 1~31: Reserved for future use.
					*/
					uint32_t mcu_slot;
					/**
					* @brief baudrate passed from MCU to Acore.
					*        During UART download, AXDL dynamically switches the baudrate.
					*        This variable stores the baudrate after the switch.
					*        This field is valid when fdl_chg_baud != 0.
					*/
					uint32_t fdl_chg_baud;
					/**
					* @brief The safety abort alarm passed from MCU to Acore
					*        This field corresponds to the "safety_rst_alarm" register of
					*        the safety_island_glb_aon
					*/
					uint32_t safety_abort_alarm;
					/**
					* @brief The top sys abort alarm passed from MCU to Acore
					*        This field corresponds to the "abort_alarm" register of the top_glb
					*/
					uint32_t top_abort_alarm;
				};

				uint8_t reserved0[512];
			};
			uint8_t ddr_param[512];
			/* MTD partition table */
			uint8_t mtdparts[896];
			/* Anti-rollback data */
			uint8_t antirollback[512];
			/* Debug policy */
			uint8_t debugpolicy[512];
		};
	};

	void *fdt_addr;
	int32_t slot;
	unsigned short bootdevice;
	struct membuff spl_console_out;
} boot_params_t;

_Static_assert(offsetof(boot_params_t, fdt_addr) == 3072,
	      "fdt_addr offset in boot_params_t is not 3072 bytes");

/* Magic number indicating that the U-Boot image has been loaded into memory */
#define M57H_SOC_MAGIC     0x5a5a5a5aU

static inline boot_params_t *boot_params_get_base(void)
{
	/* TODO: detect base during runtime */
	return (boot_params_t *)CONFIG_LUA_OCM_BASE;
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

enum {
	ENV_BOOTDEVICE = 0,
	ENV_BOOTSTATE,
	ENV_MAIN_MTD,
	ENV_SAFE_MTD,
	ENV_MMC_DEV,
	ENV_DEVTYPE,
	ENV_DEVNUM,
	ENV_SECUREBOOT,
	ENV_DOWNIF,
};

enum {
	SAFETY_ABORT_UNKNOWN = 0x00,
	SAFETY_ABORT_SWRST_ALARM,
	SAFETY_ABORT_FOSU_ALARM,
	SAFETY_ABORT_FCU_ALARM,
	SAFETY_ABORT_WTD4_ALARM,
	SAFETY_ABORT_WTD5_ALARM,
	SAFETY_ABORT_THM_ALARM,
	SAFETY_ABORT_LBIST_ALARM,

	SAFETY_ABORT_COUNT,
};

enum {
	ABORT_UNKNOWN = 0x00,
	ABORT_WDT0_ALARM,
	ABORT_WDT1_ALARM,
	ABORT_WDT2_ALARM,
	ABORT_WDT3_ALARM,
	ABORT_SAFETY_HW_ALARM,
	ABORT_SAFETY_SW_ALARM,
	ABORT_MAIN_SW_ALARM,
	ABORT_ALARM_REQ,

	ABORT_COUNT,
};

extern const char *safe_part_id[];
extern const char *main_part_id[];
int get_safe_part_id_count(void);
int get_main_part_id_count(void);
int get_bootdevice(const char **name);
int set_bootdevice_env(int bootdev);
int get_bootstate(void);
int set_bootstrap(uint32_t bootstrap);
int get_downif(void);
void set_downif_env(int downif);
bool is_secure_boot(void);
void set_secureboot_env(bool secure);
void remove_mtd_device(int bootdev);
const char *env_get_name(int index);
int soc_init_f(void);
bool is_bootdev_env_ready(void);
void set_bootdev_env_ready(bool okay);
int get_abort(bool safety, const char **name);
int check_bootparams(bool is_sbl);
void calc_bootparams_crc32(bool is_sbl);
void probe_all_spi_nor_devs(void);

/* AB Control operations */
int safety_abc_setup(int mark_type, int slot);
int safety_abc_get_slot(void);
int abc_mark_bootable(bool okay);

#include <blk.h>
#include <part.h>
#include <fdl.h>

struct blk_desc *get_blk_by_devnum(enum if_type if_type, int devnum);
int get_part_by_name(struct blk_desc *dev_desc, const char *name,
                      struct disk_partition *part);
int partitions_id_check(struct fdl_part_table *tab);

#endif /* ASM_LAGUNA_BOOTPARAM_H_ */