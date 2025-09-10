/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef RISCV_CPU_H
#define RISCV_CPU_H

#include <types.h>
#include <lib/util.h>
#include <debug/logmsg.h>
#include <board_info.h>
#include <barrier.h>

#define cpu_relax()	cpu_memory_barrier() /* TODO: replace with yield instruction */
#define NR_CPUS		MAX_PCPU_NUM

#define LONG_BYTEORDER 3
#define BYTES_PER_LONG (1 << LONG_BYTEORDER)
#define BITS_PER_LONG (BYTES_PER_LONG << 3)
/* Define the interrupt enable bit mask */
#define SSTATUS_SIE 0x2

/* Define CPU stack alignment */
#define CPU_STACK_ALIGN	16UL

/* In ACRN, struct per_cpu_region is a critical data structure
 * containing key per-CPU data frequently accessed via get_cpu_var().
 * We use the tp register to store the current logical pCPU ID to
 * facilitate efficient per-CPU data access. This design mirrors
 * the x86 implementation, which uses the dedicated MSR_IA32_SYSENTER_CS
 * MSR (unused by the hypervisor) for the same purpose.
 */
static inline uint16_t arch_get_pcpu_id(void)
{
	uint16_t pcpu_id;

	asm volatile ("mv %0, tp" : "=r" (pcpu_id) : : );
	return pcpu_id;
}

static inline void arch_set_current_pcpu_id(uint16_t pcpu_id)
{
	asm volatile ("mv tp, %0" : : "r" (pcpu_id) : "tp");
}

static inline void arch_asm_pause(void)
{
	asm volatile ("pause" ::: "memory");
}

/* Write CSR */
#define cpu_csr_write(reg, csr_val)                                                                                    \
	({                                                                                                             \
		uint64_t val = (uint64_t)csr_val;                                                                      \
		asm volatile(" csrw " STRINGIFY(reg) ", %0 \n\t" ::"r"(val) : "memory");                               \
	})

/**
 * FIXME: to follow multi-arch design, refactor all of them into static inline functions with corresponding
 *        X86 implementation together.
 */
#define local_irq_disable() asm volatile("csrc sstatus, %0\n" ::"i"(SSTATUS_SIE) : "memory")
#define local_irq_enable() asm volatile("csrs sstatus, %0\n" ::"i"(SSTATUS_SIE) : "memory")
#define local_save_flags(x) ({ asm volatile("csrr %0, sstatus, 0\n" : "=r"(x)::"memory"); })
#define local_irq_restore(x) ({ asm volatile("csrs sstatus, %0\n" ::"rK"(x & SSTATUS_SIE) : "memory"); })
#define local_irq_save(x)                                                                                              \
	({                                                                                                             \
		uint32_t val = 0U;                                                                                     \
		asm volatile("csrrc %0, sstatus, 0\n" : "=r"(val) : "i"(SSTATUS_SIE) : "memory");                      \
		*(uint32_t *)(x) = val;                                                                                \
	})

#define CPU_INT_ALL_DISABLE(x) local_irq_save(x)
#define CPU_INT_ALL_RESTORE(x) local_irq_restore(x)

void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync);

#endif /* RISCV_CPU_H */
