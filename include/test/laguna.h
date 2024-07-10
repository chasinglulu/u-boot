/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef __TEST_LAGUNA_H__
#define __TEST_LAGUNA_H__

#include <test/test.h>

/* Declare a new laguna function test */
#define LAGUNA_TEST(_name, _flags) UNIT_TEST(_name, _flags, laguna_test)
#define LAGUNA_STRESS_TEST(_name, _flags) UNIT_TEST(_name, _flags, laguna_stress_test)

#endif /* __TEST_LAGUNA_H__ */