/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Author: Yifan Liu <yifan1.liu@intel.com>
 *         Haicheng Li <haicheng.li@intel.com>
 */

#include <vcpu.h>
#include <vm.h>
#include <cpu.h>
#include <errno.h>
#include <types.h>
#include <schedule.h>

#include <asm/trap.h>
#include <asm/guest/vcpu_priv.h>

int32_t vcpu_virtual_inst_fault_handler(struct acrn_vcpu *vcpu) {
	/* TODO: to be implemented */
	pr_err("Unhandled virtual instruction fault");
	(void)vcpu;
	cpu_dead();
	return 0;
}

int32_t vcpu_exit_handler(struct acrn_vcpu *vcpu)
{
	uint64_t cause = vcpu->arch.regs.cause;
	int32_t ret;

	if ((cause & TRAP_CAUSE_INTERRUPT_BITMASK) != 0) {
		/*
		 * RISC-V traps does not acknowledge the interrupt (interrupt
		 * is still pending in sip). Call interrupt handler to clear
		 * the pending bit.
		 */

		dispatch_interrupt((const struct intr_excp_ctx *)&(vcpu->arch.regs));

		ret = 0;
		pr_dbg("vcpu intr, cause: 0x%x, ret: %d", cause, ret);

		/* Check if we have any other interrupts pending */
		local_irq_enable();
	} else {
		local_irq_enable();
		switch (cause) {
			case TRAP_CAUSE_EXC_VIRTUAL_INST_FAULT:
				ret = vcpu_virtual_inst_fault_handler(vcpu);
				break;
			default:
				ret = -EINVAL;
				pr_err("Unhandled trap. cause: 0x%x", cause);
				break;
		}

		pr_dbg("vcpu exc, cause: 0x%x, ret: %d", cause, ret);
	}

	return ret;
}

void dispatch_vcpu_trap(struct acrn_vcpu_arch *vcpu_arch)
{
	uint16_t pcpu_id = get_pcpu_id();
	struct acrn_vcpu *vcpu = container_of(vcpu_arch, struct acrn_vcpu, arch);
	struct cpu_regs *regs = &(vcpu->arch.regs);
	struct riscv_vcpu_host_ctx *hctx = &(vcpu->arch.hctx);
	int32_t ret = -EINVAL;

	regs->hstatus = cpu_csr_read(CSR_HSTATUS);
	hctx->htval = cpu_csr_read(CSR_HTVAL);
	hctx->htinst = cpu_csr_read(CSR_HTINST);

	ret = vcpu_exit_handler(vcpu);
	if (ret < 0) {
		pr_err("Failed to handle vcpu exit. ret: %d", ret);
	}
	local_irq_disable();

	if (need_reschedule(pcpu_id)) {
		schedule();
	}

	ret = riscv_process_vcpu_requests(vcpu);
	if (ret < 0) {
		pr_fatal("Failed to process vcpu requests, pausing the vcpu");
		get_vm_lock(vcpu->vm);
		zombie_vcpu(vcpu);
		put_vm_lock(vcpu->vm);
		schedule();
		/* we'll never be scheduled in */
	}

	cpu_csr_write(CSR_HSTATUS, regs->hstatus);

	/* next trap frame saved directly to vcpu struct */
	cpu_csr_write(CSR_SSCRATCH, (uint64_t)regs);
}
