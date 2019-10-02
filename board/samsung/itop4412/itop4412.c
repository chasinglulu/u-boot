// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/cpu.h>
#include <asm/arch/mmc.h>
#include <asm/arch/periph.h>
#include <asm/arch/pinmux.h>
#include <usb.h>

#ifndef CONFIG_SPL_BUILD
u32 get_board_rev(void)
{
	return 0;
}

int exynos_init(void)
{
	return 0;
}

int board_usb_init(int index, enum usb_init_type init)
{
	return 0;
}

#ifdef CONFIG_BOARD_EARLY_INIT_F
int exynos_early_init_f(void)
{
	return 0;
}
#endif

#else /* !CONFIG_SPL_BUILD */
void exynos_led_set_on_early(unsigned int led_num)
{
#define GPL2CON (0x11000100)
#define GPL2DAT (0x11000104)
#define GPK1CON (0x11000060)
#define GPK1DAT (0x11000064)

        uint8_t val;

        /* LED2 */
        val = led_num & 0x01;
        if (val) {
                clrsetbits_le32(GPL2CON, 0xf << 0, 0x01 << 0);
                setbits_8(GPL2DAT, 1);
        }

        /* LED3 */
        val = led_num & 0x02;
        if (val) {
                clrsetbits_le32(GPK1CON, 0xf << 4, 0x01 << 4);
                setbits_8(GPK1DAT, 1 << 1);
        }
}
#endif
