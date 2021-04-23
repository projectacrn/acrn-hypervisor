/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
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
bool is_notification_nmi(const struct acrn_vm *vm);

void setup_notification(void);
void setup_pi_notification(void);

#endif
