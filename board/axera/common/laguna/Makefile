# SPDX-License-Identifier: GPL-2.0+

# obj- += dummy.o

obj-y += common.o mtdparts.o

ifndef CONFIG_SPL_BUILD
obj-y += fdl_blk.o
obj-$(CONFIG_LUA_DM_VERITY) += dm_verity.o
endif
