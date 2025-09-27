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
#include <asm/csr.h>
#include <types.h>

static void init_interrupt_arch(__unused uint16_t pcpu_id)
{
	uint64_t addr = (uint64_t)&strap_handler;

	/*
	 * According to RISC-V Privileged Architecture
	 * 12.1.2. Supervisor Trap Vector Base Address (stvec) Register:
	 * The BASE field in stvec is a field that can hold any valid virtual
	 * or physical address, subject to the following alignment constraints:
	 * the address must be ``4-byte aligned``, and MODE settings other than
	 * Direct might impose additional alignment constraints on the
	 * value in the BASE field.
	 */
	cpu_csr_write(CSR_STVEC, (addr | TRAP_VECTOR_MODE_DIRECT));
	cpu_csr_write(CSR_SIE, (IP_IE_SSI | IP_IE_STI | IP_IE_SEI));
}

/*
 * TODO:
 * This is the first step toward aligning with the common IRQ framework.
 * For simplicity in this patchset, which focuses only on initialization,
 * init_interrupt() is defined directly in arch/riscv/irq.c.
 *
 * Because interrupt handler registration via request_irq() is not yet
 * implemented for RISC-V, fully aligning with the framework would require
 * adding a few empty arch-specific functions as placeholders.
 *
 * Once request_irq() support is introduced, we can complete the integration
 * with the common IRQ framework.
 */
void init_interrupt(uint16_t pcpu_id)
{
	init_interrupt_arch(pcpu_id);

	local_irq_enable();
}
