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
	uint64_t magic;
};

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

/* Clear CSR */
#define cpu_csr_clear(reg, csr_mask)                                                                                   \
	({                                                                                                             \
		uint64_t mask = (uint64_t)csr_mask;                                                                    \
		asm volatile (" csrc " STRINGIFY(reg) ", %0 \n\t" :: "r"(mask): "memory");                             \
	})

static inline void arch_local_irq_enable(void)
{
	asm volatile ("csrs sstatus, %0 \n"
			:: "i"(SSTATUS_SIE)
			: "memory");
}

static inline void arch_local_irq_disable(void)
{
	asm volatile ("csrc sstatus, %0 \n"
			:: "i"(SSTATUS_SIE)
			: "memory");
}

static inline void arch_local_irq_save(uint64_t *flags_ptr)
{
	uint64_t val = 0UL;

	/* read and clear the SSTATUS_SIE bit (disable interrupts) */
	asm volatile("csrrc %0, sstatus, %1 \n"
			: "=r"(val)
			: "r"(SSTATUS_SIE)
			: "memory");
	*flags_ptr = val;
}

static inline void arch_local_irq_restore(uint64_t flags)
{
	asm volatile("csrs sstatus, %0 \n" ::"rK"(flags & SSTATUS_SIE) : "memory");
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

#else /* ASSEMBLER */
#include <asm/offset.h>

/* The following symbols must remain consistent:
 * - CPU_REGS_OFFSET_* macros in `include/arch/riscv/asm/offset.h`
 * - struct cpu_regs
 * - cpu_ctx_save/cpu_ctx_restore macros used in assembly
 */
.macro cpu_ctx_save
	addi sp, sp, -CPU_REGS_OFFSET_LAST

	/* General purpose registers. */
	/* Save sp first to avoid corrupting the stack frame */
	sd sp, CPU_REGS_OFFSET_SP(sp)
	sd ra, CPU_REGS_OFFSET_RA(sp)
	sd gp, CPU_REGS_OFFSET_GP(sp)
	sd tp, CPU_REGS_OFFSET_TP(sp)
	sd t0, CPU_REGS_OFFSET_T0(sp)
	sd t1, CPU_REGS_OFFSET_T1(sp)
	sd t2, CPU_REGS_OFFSET_T2(sp)
	sd s0, CPU_REGS_OFFSET_S0(sp)
	sd s1, CPU_REGS_OFFSET_S1(sp)
	sd a0, CPU_REGS_OFFSET_A0(sp)
	sd a1, CPU_REGS_OFFSET_A1(sp)
	sd a2, CPU_REGS_OFFSET_A2(sp)
	sd a3, CPU_REGS_OFFSET_A3(sp)
	sd a4, CPU_REGS_OFFSET_A4(sp)
	sd a5, CPU_REGS_OFFSET_A5(sp)
	sd a6, CPU_REGS_OFFSET_A6(sp)
	sd a7, CPU_REGS_OFFSET_A7(sp)
	sd s2, CPU_REGS_OFFSET_S2(sp)
	sd s3, CPU_REGS_OFFSET_S3(sp)
	sd s4, CPU_REGS_OFFSET_S4(sp)
	sd s5, CPU_REGS_OFFSET_S5(sp)
	sd s6, CPU_REGS_OFFSET_S6(sp)
	sd s7, CPU_REGS_OFFSET_S7(sp)
	sd s8, CPU_REGS_OFFSET_S8(sp)
	sd s9, CPU_REGS_OFFSET_S9(sp)
	sd s10, CPU_REGS_OFFSET_S10(sp)
	sd s11, CPU_REGS_OFFSET_S11(sp)
	sd t3, CPU_REGS_OFFSET_T3(sp)
	sd t4, CPU_REGS_OFFSET_T4(sp)
	sd t5, CPU_REGS_OFFSET_T5(sp)
	sd t6, CPU_REGS_OFFSET_T6(sp)

	/* Control and Status Registers (CSRs). */
	csrr t0, sepc
	sd t0, CPU_REGS_OFFSET_EPC(sp)
	csrr t1, sstatus
	sd t1, CPU_REGS_OFFSET_STATUS(sp)
	csrr t2, scause
	sd t2, CPU_REGS_OFFSET_CAUSE(sp)
	csrr t3, stval
	sd t3, CPU_REGS_OFFSET_TVAL(sp)
	csrr t4, sscratch
	sd t4, CPU_REGS_OFFSET_SCRATCH(sp)
.endm

.macro cpu_ctx_restore
	/* Control and Status Registers (CSRs). */
	ld t0, CPU_REGS_OFFSET_EPC(sp)
	csrw sepc, t0
	ld t1, CPU_REGS_OFFSET_STATUS(sp)
	csrw sstatus, t1
	/* Restoring scause/stval is unnecessary and will be skipped. */
	ld t4, CPU_REGS_OFFSET_SCRATCH(sp)
	csrw sscratch, t4

	/* General purpose registers. */
	ld ra, CPU_REGS_OFFSET_RA(sp)
	ld gp, CPU_REGS_OFFSET_GP(sp)
	ld tp, CPU_REGS_OFFSET_TP(sp)
	ld t0, CPU_REGS_OFFSET_T0(sp)
	ld t1, CPU_REGS_OFFSET_T1(sp)
	ld t2, CPU_REGS_OFFSET_T2(sp)
	ld s0, CPU_REGS_OFFSET_S0(sp)
	ld s1, CPU_REGS_OFFSET_S1(sp)
	ld a0, CPU_REGS_OFFSET_A0(sp)
	ld a1, CPU_REGS_OFFSET_A1(sp)
	ld a2, CPU_REGS_OFFSET_A2(sp)
	ld a3, CPU_REGS_OFFSET_A3(sp)
	ld a4, CPU_REGS_OFFSET_A4(sp)
	ld a5, CPU_REGS_OFFSET_A5(sp)
	ld a6, CPU_REGS_OFFSET_A6(sp)
	ld a7, CPU_REGS_OFFSET_A7(sp)
	ld s2, CPU_REGS_OFFSET_S2(sp)
	ld s3, CPU_REGS_OFFSET_S3(sp)
	ld s4, CPU_REGS_OFFSET_S4(sp)
	ld s5, CPU_REGS_OFFSET_S5(sp)
	ld s6, CPU_REGS_OFFSET_S6(sp)
	ld s7, CPU_REGS_OFFSET_S7(sp)
	ld s8, CPU_REGS_OFFSET_S8(sp)
	ld s9, CPU_REGS_OFFSET_S9(sp)
	ld s10, CPU_REGS_OFFSET_S10(sp)
	ld s11, CPU_REGS_OFFSET_S11(sp)
	ld t3, CPU_REGS_OFFSET_T3(sp)
	ld t4, CPU_REGS_OFFSET_T4(sp)
	ld t5, CPU_REGS_OFFSET_T5(sp)
	ld t6, CPU_REGS_OFFSET_T6(sp)
	/* Restore sp last to avoid corrupting the stack frame */
	ld sp, CPU_REGS_OFFSET_SP(sp)

	addi sp, sp, CPU_REGS_OFFSET_LAST
.endm

#endif /* ASSEMBLER */

#endif /* RISCV_CPU_H */
