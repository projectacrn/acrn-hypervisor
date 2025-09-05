/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef X86_SMP_H
#define X86_SMP_H

#include <types.h>

void arch_init_smp_call(void);
void arch_smp_call_kick_pcpu(uint16_t pcpu_id);

#endif /* X86_SMP_H */
