/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <vcpu.h>
#include <logmsg.h>
#include <asm/irq.h>

/**
 * @brief Inject a trap to VS-mode.
 *
 * This function handles the injection of a trap to VS-mode. The trap
 * can be originated in the context of VS-mode or VU-mode.
 *
 * @param vcpu Pointer to vCPU.
 * @param trap Pointer to the structure containing trap information
 * (tval, epc, cause).
 */
void vcpu_set_trap(struct acrn_vcpu *vcpu,
	struct riscv_vcpu_trap_info *trap)
{
	uint64_t vsstatus;

	/* Get VS-mode SSTATUS CSR */
	vsstatus = cpu_csr_read(CSR_VSSTATUS);

	/* Update SPP for VS-mode */
	vsstatus &= ~SSTATUS_SPP;
	if (vcpu->arch.regs.status & SSTATUS_SPP)
		vsstatus |= SSTATUS_SPP;

	/* Update SPIE for VS-mode */
	vsstatus &= ~SSTATUS_SPIE;
	if (vsstatus & SSTATUS_SIE)
		vsstatus |= SSTATUS_SPIE;

	/* Clear SIE for VS-mode */
	vsstatus &= ~SSTATUS_SIE;

	/* Update VS-mode SSTATUS CSR */
	cpu_csr_write(CSR_VSSTATUS, vsstatus);

	/* Update VS-mode exception info */
	cpu_csr_write(CSR_VSTVAL, trap->tval);
	cpu_csr_write(CSR_VSEPC, trap->epc);
	cpu_csr_write(CSR_VSCAUSE, trap->cause);

	/* Set SEPC to VS-mode exception vector base */
	vcpu->arch.regs.epc = cpu_csr_read(CSR_VSTVEC) & ~0x3UL;

	/* Set SPP to VS-mode */
	vcpu->arch.regs.status |= SSTATUS_SPP;
}

/**
 * @brief Queue an exception for a vCPU.
 *
 * Queues an exception for the specified vCPU. If an exception is already
 * pending, it will be overwritten and a warning will be logged. Nested
 * exceptions and exception prioritization are not supported.
 *
 * @param vcpu Pointer to the target acrn_vcpu structure.
 * @param trap Pointer to the riscv_vcpu_trap_info structure with exception
 * details.
 *
 * @note No action is taken if @p trap is NULL.
 * @note This function is NOT thread-safe and is only expected to be called
 * in the vcpu_thread/vcpu_trap_handler specified by vcpu.
 */
void vcpu_queue_exception(struct acrn_vcpu *vcpu,
	struct riscv_vcpu_trap_info *trap)
{
	struct acrn_vcpu_arch *arch = &vcpu->arch;

	if (trap) {
		/*
		 * Nested exceptions and exception priority are not supported.
		 * If an exception is pending injection, it will be overwritten
		 * by any new exception.
		 */
		if (arch->trap.cause != EXCEPTION_INVALID) {
			pr_err("nested exception happened, prev excp = 0x%lx",
				arch->trap.cause);
		}

		arch->trap = *trap;
		vcpu_make_request(vcpu, RISCV_VCPU_REQUEST_EXCEPTION);
	}
}