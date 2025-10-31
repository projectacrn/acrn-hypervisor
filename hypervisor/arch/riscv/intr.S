/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <asm/trap.h>
#include <cpu.h>

.macro cpu_ctx_save
	/* General purpose registers. */
	sd sp, CPU_REGS_OFFSET_SP(t0)
	sd ra, CPU_REGS_OFFSET_RA(t0)
	sd gp, CPU_REGS_OFFSET_GP(t0)
	sd tp, CPU_REGS_OFFSET_TP(t0)
	sd t1, CPU_REGS_OFFSET_T1(t0)
	sd t2, CPU_REGS_OFFSET_T2(t0)
	sd s0, CPU_REGS_OFFSET_S0(t0)
	sd s1, CPU_REGS_OFFSET_S1(t0)
	sd a0, CPU_REGS_OFFSET_A0(t0)
	sd a1, CPU_REGS_OFFSET_A1(t0)
	sd a2, CPU_REGS_OFFSET_A2(t0)
	sd a3, CPU_REGS_OFFSET_A3(t0)
	sd a4, CPU_REGS_OFFSET_A4(t0)
	sd a5, CPU_REGS_OFFSET_A5(t0)
	sd a6, CPU_REGS_OFFSET_A6(t0)
	sd a7, CPU_REGS_OFFSET_A7(t0)
	sd s2, CPU_REGS_OFFSET_S2(t0)
	sd s3, CPU_REGS_OFFSET_S3(t0)
	sd s4, CPU_REGS_OFFSET_S4(t0)
	sd s5, CPU_REGS_OFFSET_S5(t0)
	sd s6, CPU_REGS_OFFSET_S6(t0)
	sd s7, CPU_REGS_OFFSET_S7(t0)
	sd s8, CPU_REGS_OFFSET_S8(t0)
	sd s9, CPU_REGS_OFFSET_S9(t0)
	sd s10, CPU_REGS_OFFSET_S10(t0)
	sd s11, CPU_REGS_OFFSET_S11(t0)
	sd t3, CPU_REGS_OFFSET_T3(t0)
	sd t4, CPU_REGS_OFFSET_T4(t0)
	sd t5, CPU_REGS_OFFSET_T5(t0)
	sd t6, CPU_REGS_OFFSET_T6(t0)

	/* Control and Status Registers (CSRs). */
	csrr t1, sstatus
	sd t1, CPU_REGS_OFFSET_STATUS(t0)
	csrr t2, scause
	sd t2, CPU_REGS_OFFSET_CAUSE(t0)
	csrr t3, stval
	sd t3, CPU_REGS_OFFSET_TVAL(t0)
	csrr t4, sscratch
	sd t4, CPU_REGS_OFFSET_T0(t0)
	csrr t5, sepc
	sd t5, CPU_REGS_OFFSET_EPC(t0)
.endm

.macro cpu_ctx_restore
	/* Control and Status Registers (CSRs). */
	ld t1, CPU_REGS_OFFSET_EPC(t0)
	csrw sepc, t1
	ld t2, CPU_REGS_OFFSET_STATUS(t0)
	csrw sstatus, t2

	/* General purpose registers. */
	ld sp, CPU_REGS_OFFSET_SP(t0)
	ld ra, CPU_REGS_OFFSET_RA(t0)
	ld gp, CPU_REGS_OFFSET_GP(t0)
	ld tp, CPU_REGS_OFFSET_TP(t0)
	ld t1, CPU_REGS_OFFSET_T1(t0)
	ld t2, CPU_REGS_OFFSET_T2(t0)
	ld s0, CPU_REGS_OFFSET_S0(t0)
	ld s1, CPU_REGS_OFFSET_S1(t0)
	ld a0, CPU_REGS_OFFSET_A0(t0)
	ld a1, CPU_REGS_OFFSET_A1(t0)
	ld a2, CPU_REGS_OFFSET_A2(t0)
	ld a3, CPU_REGS_OFFSET_A3(t0)
	ld a4, CPU_REGS_OFFSET_A4(t0)
	ld a5, CPU_REGS_OFFSET_A5(t0)
	ld a6, CPU_REGS_OFFSET_A6(t0)
	ld a7, CPU_REGS_OFFSET_A7(t0)
	ld s2, CPU_REGS_OFFSET_S2(t0)
	ld s3, CPU_REGS_OFFSET_S3(t0)
	ld s4, CPU_REGS_OFFSET_S4(t0)
	ld s5, CPU_REGS_OFFSET_S5(t0)
	ld s6, CPU_REGS_OFFSET_S6(t0)
	ld s7, CPU_REGS_OFFSET_S7(t0)
	ld s8, CPU_REGS_OFFSET_S8(t0)
	ld s9, CPU_REGS_OFFSET_S9(t0)
	ld s10, CPU_REGS_OFFSET_S10(t0)
	ld s11, CPU_REGS_OFFSET_S11(t0)
	ld t3, CPU_REGS_OFFSET_T3(t0)
	ld t4, CPU_REGS_OFFSET_T4(t0)
	ld t5, CPU_REGS_OFFSET_T5(t0)
	ld t6, CPU_REGS_OFFSET_T6(t0)
	ld t0, CPU_REGS_OFFSET_T0(t0)
.endm

.macro cpu_host_state_load
	ld sp, CPU_REGS_OFFSET_EXC_SP(t0)
	ld tp, CPU_REGS_OFFSET_HOST_TP(t0)
	ld gp, CPU_REGS_OFFSET_HOST_GP(t0)
.endm

.macro cpu_host_state_save
	sd sp, CPU_REGS_OFFSET_EXC_SP(t0)
	sd tp, CPU_REGS_OFFSET_HOST_TP(t0)
	sd gp, CPU_REGS_OFFSET_HOST_GP(t0)
.endm

	.text

	.balign 4
	.globl strap_handler
strap_handler:
	/*
	 * sscratch == 0: trap comes from hs-mode
	 * sscratch != 0: trap comes from v-mode, and sscratch points to struct acrn_vcpu_arch
	 */
	csrrw t0, sscratch, t0
	bnez t0, vcpu_exit_trap

	addi sp, sp, -CPU_REGS_OFFSET_LAST
	mv t0, sp
	cpu_ctx_save
	csrw sscratch, zero

	/** Pass t0 in a0, as the argument to dispatch_trap(). */
	mv	a0, t0
	call	dispatch_trap

	mv t0, sp
	cpu_ctx_restore
	addi sp, sp, CPU_REGS_OFFSET_LAST
	sret

vcpu_exit_trap:
	cpu_ctx_save
	cpu_host_state_load
	csrw sscratch, zero

	/* t0 points to struct acrn_vcpu_arch */
	mv	a0, t0
	call dispatch_vcpu_trap

	.globl vcpu_exit_return
vcpu_exit_return:
	csrr t0, sscratch
	cpu_host_state_save
	cpu_ctx_restore
	sret
