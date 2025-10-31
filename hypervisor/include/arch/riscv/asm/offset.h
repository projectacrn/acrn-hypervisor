/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_OFFSET_H
#define RISCV_OFFSET_H

/* The following symbols must remain consistent:
 * - CPU_REGS_OFFSET_* macros in `include/arch/riscv/asm/offset.h`
 * - struct cpu_regs
 * - cpu_ctx_save/cpu_ctx_restore macros used in assembly
 */
/* General purpose registers. */
#define CPU_REGS_OFFSET_ZERO			0x0
#define CPU_REGS_OFFSET_RA			0x8
#define CPU_REGS_OFFSET_SP			0x10
#define CPU_REGS_OFFSET_GP			0x18
#define CPU_REGS_OFFSET_TP			0x20
#define CPU_REGS_OFFSET_T0			0x28
#define CPU_REGS_OFFSET_T1			0x30
#define CPU_REGS_OFFSET_T2			0x38
#define CPU_REGS_OFFSET_S0			0x40
#define CPU_REGS_OFFSET_S1			0x48
#define CPU_REGS_OFFSET_A0			0x50
#define CPU_REGS_OFFSET_A1			0x58
#define CPU_REGS_OFFSET_A2			0x60
#define CPU_REGS_OFFSET_A3			0x68
#define CPU_REGS_OFFSET_A4			0x70
#define CPU_REGS_OFFSET_A5			0x78
#define CPU_REGS_OFFSET_A6			0x80
#define CPU_REGS_OFFSET_A7			0x88
#define CPU_REGS_OFFSET_S2			0x90
#define CPU_REGS_OFFSET_S3			0x98
#define CPU_REGS_OFFSET_S4			0xA0
#define CPU_REGS_OFFSET_S5			0xA8
#define CPU_REGS_OFFSET_S6			0xB0
#define CPU_REGS_OFFSET_S7			0xB8
#define CPU_REGS_OFFSET_S8			0xC0
#define CPU_REGS_OFFSET_S9			0xC8
#define CPU_REGS_OFFSET_S10			0xD0
#define CPU_REGS_OFFSET_S11			0xD8
#define CPU_REGS_OFFSET_T3			0xE0
#define CPU_REGS_OFFSET_T4			0xE8
#define CPU_REGS_OFFSET_T5			0xF0
#define CPU_REGS_OFFSET_T6			0xF8

/* Control and Status Registers (CSRs). */
#define CPU_REGS_OFFSET_EPC			0x100
#define CPU_REGS_OFFSET_STATUS			0x108
#define CPU_REGS_OFFSET_CAUSE			0x110
#define CPU_REGS_OFFSET_TVAL			0x118
#define CPU_REGS_OFFSET_SCRATCH			0x120
#define CPU_REGS_OFFSET_HSTATUS			0x128
#define CPU_REGS_OFFSET_HOST_TP			0x130
#define CPU_REGS_OFFSET_HOST_GP			0x138
#define CPU_REGS_OFFSET_EXC_SP			0x140

/* Total context area size (struct cpu_regs). */
#define CPU_REGS_OFFSET_LAST 			(CPU_REGS_OFFSET_EXC_SP + 8)

#endif /* RISCV_OFFSET_H */
