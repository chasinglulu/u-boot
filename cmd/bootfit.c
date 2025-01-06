// SPDX-License-Identifier: GPL-2.0+
/*
 *
 */

#include <common.h>
#include <command.h>
#include <env.h>
#include <fs.h>

static int do_bootfit(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	return 0;
}

U_BOOT_CMD(bootfit, 7, 1, do_bootfit,
	"command to get and boot from FIT or legacy image",
	"<interface> <dev[:part]> <ext2|fat|any> [addr] [filename]\n"
	"    - load and parse FIT/legacy image 'filename' from ext2, fat\n"
	"      or any filesystem on 'dev' on 'interface' to address 'addr'\n"
	"bootfit <interface> <dev[:part]> <none>\n"
	"    - directly load and parse FIT/legacy image from 'dev' on 'interface'\n"
	"      to address 'addr'"
);