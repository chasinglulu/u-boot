# SPDX-License-Identifier: GPL-2.0+

ifdef CONFIG_LUA_FIT

ifndef CONFIG_SPL_LOAD_MULTI_FIT
its-y += bl31.its
its-y += u-boot.its
its-y += tee.its
else
its-y += multi_spl.its
endif

its-y += kernel.its
its-y += extlinux.its
endif

targets += $(its-y)

PHONY += multi-its
multi-its: $(addprefix $(obj)/, $(its-y))
	@:

