/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Horizon Robotics Co,. Ltd
 */

#ifndef _ASM_ARCH_HARDWARE_H
#define _ASM_ARCH_HARDWARE_H

#define HB_CLRSETBITS(clr, set)		((((clr) | (set)) << 16) | (set))
#define HB_SETBITS(set)			HB_CLRSETBITS(0, set)
#define HB_CLRBITS(clr)			HB_CLRSETBITS(clr, 0)

#define hb_clrsetreg(addr, clr, set)	\
				writel(((clr) | (set)) << 16 | (set), addr)
#define hb_clrreg(addr, clr)		writel((clr) << 16, addr)
#define hb_setreg(addr, set)		writel((set) << 16 | (set), addr)

#endif