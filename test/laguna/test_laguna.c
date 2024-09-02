// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 * Unit tests for Laguna functions
 */

#include <common.h>
#include <command.h>
#include <test/laguna.h>
#include <test/suites.h>
#include <test/ut.h>

int do_ut_laguna(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct unit_test *tests = UNIT_TEST_SUITE_START(laguna_test);
	const int n_ents = UNIT_TEST_SUITE_COUNT(laguna_test);

	return cmd_ut_category("laguna", "laguna_test_", tests, n_ents, argc,
			       argv);
}

int do_ut_laguna_stress(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct unit_test *tests = UNIT_TEST_SUITE_START(laguna_stress_test);
	const int n_ents = UNIT_TEST_SUITE_COUNT(laguna_stress_test);

	return cmd_ut_category("laguna_stress", "laguna_stress_test_", tests, n_ents, argc,
			       argv);
}

int do_ut_ras_errors_injection(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct unit_test *tests = UNIT_TEST_SUITE_START(laguna_ras);
	const int n_ents = UNIT_TEST_SUITE_COUNT(laguna_ras);

	return cmd_ut_category("ras", "laguna_ras_", tests, n_ents, argc,
			       argv);
}