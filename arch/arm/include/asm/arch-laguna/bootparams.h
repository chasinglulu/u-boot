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
	BOOTDEVICE_GPIO_EMMC = 0x01,
	BOOTDEVICE_GPIO_NOR,
	BOOTDEVICE_GPIO_NAND,
	BOOTDEVICE_GPIO_UART,
};

#endif /* ASM_LAGUNA_BOOTPARAM_H_ */