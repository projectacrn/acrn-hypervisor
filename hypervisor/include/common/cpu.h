/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef COMMON_CPU_H
#define COMMON_CPU_H

#include <types.h>
#include <asm/cpu.h>

/* CPU states defined */
enum pcpu_boot_state {
        PCPU_STATE_RESET = 0U,
        PCPU_STATE_INITIALIZING,
        PCPU_STATE_RUNNING,
        PCPU_STATE_HALTED,
        PCPU_STATE_DEAD,
};
uint16_t arch_get_pcpu_num(void);
uint16_t get_pcpu_nums(void);
bool is_pcpu_active(uint16_t pcpu_id);
void set_pcpu_active(uint16_t pcpu_id);
void clear_pcpu_active(uint16_t pcpu_id);
bool check_pcpus_active(uint64_t mask);
bool check_pcpus_inactive(uint64_t mask);
uint64_t get_active_pcpu_bitmap(void);

#define ALL_CPUS_MASK		((1UL << get_pcpu_nums()) - 1UL)
#define AP_MASK			(ALL_CPUS_MASK & ~(1UL << BSP_CPU_ID))

#endif /* COMMON_CPU_H */
