/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef NOTIFY_H
#define NOTIFY_H

typedef void (*smp_call_func_t)(void *data);
struct smp_call_info_data {
	smp_call_func_t func;
	void *data;
};

struct acrn_vm;
void smp_call_function(uint64_t mask, smp_call_func_t func, void *data);

void setup_notification(void);
void handle_smp_call(void);
void setup_pi_notification(void);

#endif
