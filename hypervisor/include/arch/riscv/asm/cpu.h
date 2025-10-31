/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef RISCV_CPU_H
#define RISCV_CPU_H

#ifndef ASSEMBLER
#include <types.h>
#include <lib/util.h>
#include <logmsg.h>
#include <board_info.h>
#include <barrier.h>

#include <asm/csr.h>

/* The following symbols must remain consistent:
 * - CPU_REGS_OFFSET_* macros in `include/arch/riscv/asm/offset.h`
 * - struct cpu_regs
 * - cpu_ctx_save/cpu_ctx_restore macros used in assembly
 */
struct cpu_regs {
	/* General purpose registers. */
	uint64_t zero;
	uint64_t ra;
	uint64_t sp;
	uint64_t gp;
	uint64_t tp;
	uint64_t t0;
	uint64_t t1;
	uint64_t t2;
	uint64_t s0;
	uint64_t s1;
	uint64_t a0;
	uint64_t a1;
	uint64_t a2;
	uint64_t a3;
	uint64_t a4;
	uint64_t a5;
	uint64_t a6;
	uint64_t a7;
	uint64_t s2;
	uint64_t s3;
	uint64_t s4;
	uint64_t s5;
	uint64_t s6;
	uint64_t s7;
	uint64_t s8;
	uint64_t s9;
	uint64_t s10;
	uint64_t s11;
	uint64_t t3;
	uint64_t t4;
	uint64_t t5;
	uint64_t t6;

	/* Control and Status Registers (CSRs). */
	uint64_t epc;
	uint64_t status;
	uint64_t cause;
	uint64_t tval;
	uint64_t scratch;
	uint64_t hstatus;

	/* Host context. Used only in v-mode traps. */
	uint64_t host_tp;
	uint64_t host_gp;
	uint64_t exc_sp;
};

/* stack_frame is linked with the sequence of stack operation in arch_switch_to() */
#define STACK_FRAME_OFFSET_RA        0x0
#define STACK_FRAME_OFFSET_S0        0x8
#define STACK_FRAME_OFFSET_S1        0x10
#define STACK_FRAME_OFFSET_S2        0x18
#define STACK_FRAME_OFFSET_S3        0x20
#define STACK_FRAME_OFFSET_S4        0x28
#define STACK_FRAME_OFFSET_S5        0x30
#define STACK_FRAME_OFFSET_S6        0x38
#define STACK_FRAME_OFFSET_S7        0x40
#define STACK_FRAME_OFFSET_S8        0x48
#define STACK_FRAME_OFFSET_S9        0x50
#define STACK_FRAME_OFFSET_S10       0x58
#define STACK_FRAME_OFFSET_S11       0x60
#define STACK_FRAME_OFFSET_A0        0x68
#define STACK_FRAME_OFFSET_STATUS    0x70
struct stack_frame {
	uint64_t ra;
	uint64_t s0;
	uint64_t s1;
	uint64_t s2;
	uint64_t s3;
	uint64_t s4;
	uint64_t s5;
	uint64_t s6;
	uint64_t s7;
	uint64_t s8;
	uint64_t s9;
	uint64_t s10;
	uint64_t s11;
	uint64_t a0; /* thread_object parameter */
	uint64_t status;
	uint64_t magic;
};

#define cpu_relax()	cpu_memory_barrier() /* TODO: replace with yield instruction */
#define NR_CPUS		MAX_PCPU_NUM

#define LONG_BYTEORDER 3
#define BYTES_PER_LONG (1 << LONG_BYTEORDER)
#define BITS_PER_LONG (BYTES_PER_LONG << 3)

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
	register uint16_t pcpu_id asm ("tp");
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

static inline void arch_local_irq_enable(void)
{
	cpu_csr_set(CSR_SSTATUS, SSTATUS_SIE);
}

static inline void arch_local_irq_disable(void)
{
	cpu_csr_clear(CSR_SSTATUS, SSTATUS_SIE);
}

static inline void arch_local_irq_save(uint64_t *flags_ptr)
{
	*flags_ptr = cpu_csr_read_clear(CSR_SSTATUS, SSTATUS_SIE);
}

static inline void arch_local_irq_restore(uint64_t flags)
{
	cpu_csr_set(CSR_SSTATUS, (flags & SSTATUS_SIE));
}

static inline void arch_pre_user_access(void)
{
	cpu_csr_set(CSR_SSTATUS, SSTATUS_SUM);
}

static inline void arch_post_user_access(void)
{
	cpu_csr_clear(CSR_SSTATUS, SSTATUS_SUM);
}

void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync);
void init_percpu_hart_id(uint32_t bsp_hart_id);
uint16_t get_pcpu_id_from_hart_id(uint32_t hart_id);

/* FIXME: riscv dummy function */
static inline bool need_offline(uint16_t pcpu_id)
{
	(void)pcpu_id;
	return false;
}

/* FIXME: If RISC-V support sbuf, it should implement SMAP enable/disable APIs */
static inline void stac(void) {}
static inline void clac(void) {}

#else /* ASSEMBLER */
#include <asm/offset.h>

#endif /* ASSEMBLER */

#endif /* RISCV_CPU_H */
