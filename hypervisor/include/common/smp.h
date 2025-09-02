/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef COMMON_SMP_H
#define COMMON_SMP_H

#include <types.h>

typedef void (*smp_call_func_t)(void *data);

struct smp_call_info_data {
	smp_call_func_t func;
	void *data;
};

void init_smp_call(void);
void handle_smp_call(void);
void smp_call_function(uint64_t mask, smp_call_func_t func, void *data);
void kick_notification(__unused uint32_t irq, __unused void *data);

#endif /* COMMON_SMP_H */
