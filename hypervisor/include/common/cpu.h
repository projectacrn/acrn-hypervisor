/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef COMMON_CPU_H
#define COMMON_CPU_H

#include <types.h>
#include <asm/cpu.h>

#define CPU_UP_TIMEOUT		100U /* millisecond */
#define CPU_DOWN_TIMEOUT	100U /* millisecond */

#define BSP_CPU_ID		0U /* Boot CPU ID */

/**
 * The invalid cpu_id (INVALID_CPU_ID) is error code for error handling,
 * this means that caller can't find a valid physical cpu or virtual cpu.
 */
#define INVALID_CPU_ID		0xffffU

/* hypervisor stack bottom magic('intl') */
#define SP_BOTTOM_MAGIC    0x696e746cUL

/* CPU states defined */
enum pcpu_boot_state {
        PCPU_STATE_RESET = 0U,
        PCPU_STATE_INITIALIZING,
        PCPU_STATE_RUNNING,
        PCPU_STATE_HALTED,
        PCPU_STATE_DEAD,
};

static inline uint16_t arch_get_pcpu_id(void);
static inline void arch_set_current_pcpu_id(uint16_t pcpu_id);
void arch_start_pcpu(uint16_t pcpu_id);
static inline void arch_asm_pause(void);
uint16_t arch_get_pcpu_num(void);
uint16_t get_pcpu_nums(void);
bool is_pcpu_active(uint16_t pcpu_id);
void set_pcpu_active(uint16_t pcpu_id);
void clear_pcpu_active(uint16_t pcpu_id);
bool check_pcpus_active(uint64_t mask);
bool check_pcpus_inactive(uint64_t mask);
uint64_t get_active_pcpu_bitmap(void);
void pcpu_set_current_state(uint16_t pcpu_id, enum pcpu_boot_state state);
bool start_pcpus(uint64_t mask);

#define ALL_CPUS_MASK		((1UL << get_pcpu_nums()) - 1UL)
#define AP_MASK			(ALL_CPUS_MASK & ~(1UL << BSP_CPU_ID))

static inline uint16_t get_pcpu_id(void)
{
	return arch_get_pcpu_id();
}

static inline void set_current_pcpu_id(uint16_t pcpu_id)
{
	arch_set_current_pcpu_id(pcpu_id);
}

static inline void asm_pause(void)
{
	arch_asm_pause();
}

#endif /* COMMON_CPU_H */
