/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Author: Yifan Liu <yifan1.liu@intel.com>
 *         Haicheng Li <haicheng.li@intel.com>
 */

#include <vcpu.h>
#include <irq.h>
#include <bits.h>
#include <event.h>
#include <vm.h>
#include <fdt.h>

#include <asm/csr.h>
#include <asm/trap.h>
#include <asm/guest/vcpu_priv.h>
#include <asm/guest/vsbi.h>
#include <asm/guest/virq.h>

void vcpu_set_epc(struct acrn_vcpu *vcpu, uint64_t val)
{
	struct cpu_regs *r = &(vcpu->arch.regs);
	/* vs mode pc will be set to sepc after sret */
	r->epc = val;
}

void load_vcpu(struct acrn_vcpu *vcpu)
{
	struct riscv_vcpu_guest_ctx *gctx = &(vcpu->arch.gctx);
	struct riscv_vcpu_host_ctx *hctx = &(vcpu->arch.hctx);

	/* load host state */
	cpu_csr_write(CSR_HCOUNTEREN, hctx->hcounteren);
	cpu_csr_write(CSR_HENVCFG,    hctx->henvcfg);
	cpu_csr_write(CSR_HEDELEG,    hctx->hedeleg);
	cpu_csr_write(CSR_HIDELEG,    hctx->hideleg);
	cpu_csr_write(CSR_HGATP,      hctx->hgatp);
	/* htinst is written by hardware, no need to restore */
	/* htval is written by hardware, no need to restore */
	cpu_csr_write(CSR_HIE,        hctx->hie);
	/* hip is written by hardware, no need to restore */

	/* load guest state */
	cpu_csr_write(CSR_VSSTATUS,   gctx->vsstatus);
	cpu_csr_write(CSR_VSIE,       gctx->vsie);
	cpu_csr_write(CSR_VSTVEC,     gctx->vstvec);
	cpu_csr_write(CSR_VSSCRATCH,  gctx->vsscratch);
	cpu_csr_write(CSR_VSEPC,      gctx->vsepc);
	cpu_csr_write(CSR_VSCAUSE,    gctx->vscause);
	cpu_csr_write(CSR_VSTVAL,     gctx->vstval);
	cpu_csr_write(CSR_VSTIMECMP,  gctx->vstimecmp);
	cpu_csr_write(CSR_HVIP,       gctx->hvip);
	cpu_csr_write(CSR_VSATP,      gctx->vsatp);
	cpu_csr_write(CSR_SCOUNTEREN, gctx->scounteren);
	cpu_csr_write(CSR_SENVCFG,    gctx->senvcfg);

	/* TODO: save and restore FP registers */
}

void unload_vcpu(struct acrn_vcpu *vcpu)
{
	struct riscv_vcpu_guest_ctx *gctx = &(vcpu->arch.gctx);
	struct riscv_vcpu_host_ctx *hctx = &(vcpu->arch.hctx);

	/* save host state */
	hctx->hcounteren = cpu_csr_read(CSR_HCOUNTEREN);
	hctx->henvcfg    = cpu_csr_read(CSR_HENVCFG);
	hctx->hedeleg    = cpu_csr_read(CSR_HEDELEG);
	hctx->hideleg    = cpu_csr_read(CSR_HIDELEG);
	hctx->hgatp      = cpu_csr_read(CSR_HGATP);
	hctx->htinst     = cpu_csr_read(CSR_HTINST);
	hctx->htval      = cpu_csr_read(CSR_HTVAL);
	hctx->hie        = cpu_csr_read(CSR_HIE);
	hctx->hip        = cpu_csr_read(CSR_HIP);

	/* save guest state */
	gctx->vsstatus   = cpu_csr_read(CSR_VSSTATUS);
	gctx->vsie       = cpu_csr_read(CSR_VSIE);
	gctx->vstvec     = cpu_csr_read(CSR_VSTVEC);
	gctx->vsscratch  = cpu_csr_read(CSR_VSSCRATCH);
	gctx->vsepc      = cpu_csr_read(CSR_VSEPC);
	gctx->vscause    = cpu_csr_read(CSR_VSCAUSE);
	gctx->vstval     = cpu_csr_read(CSR_VSTVAL);
	gctx->vstimecmp  = cpu_csr_read(CSR_VSTIMECMP);
	gctx->hvip       = cpu_csr_read(CSR_HVIP);
	gctx->vsatp      = cpu_csr_read(CSR_VSATP);
	gctx->scounteren = cpu_csr_read(CSR_SCOUNTEREN);
	gctx->senvcfg    = cpu_csr_read(CSR_SENVCFG);

	/* TODO: save and restore FP registers */
}

int32_t riscv_process_vcpu_requests(struct acrn_vcpu *vcpu)
{
	int32_t ret = 0;

	if (vcpu_has_pending_request(vcpu)) {
		if (vcpu_take_request(vcpu, RISCV_VCPU_REQUEST_EXCEPTION)) {
			vcpu_set_trap(vcpu, &vcpu->arch.trap);
			memset(&vcpu->arch.trap, 0, sizeof(struct riscv_vcpu_trap_info));
			vcpu->arch.trap.cause = EXCEPTION_INVALID;
		}

		vcpu_inject_pending_intr(vcpu);
	}

	return ret;
}

int32_t arch_init_vcpu(struct acrn_vcpu *vcpu)
{
	struct cpu_regs *regs = &(vcpu->arch.regs);
	struct riscv_vcpu_host_ctx *hctx = &(vcpu->arch.hctx);

	reset_vcpu(vcpu);

	/* Enable CY, TM, IR */
	hctx->hcounteren = SCOUNTEREN_CY | SCOUNTEREN_TM | SCOUNTEREN_IR;

	/* Enable guest SSTC */
	hctx->henvcfg = ENVCFG_STCE;

	/* Delegate traps to VS mode */
	hctx->hedeleg = HEDELEG_DEFAULT;

	/* Delegate VS interrupts */
	hctx->hideleg = HIDELEG_DEFAULT;

	/*
	 * SPVP & SPV: sret to vs mode
	 */
	regs->hstatus = HSTATUS_SPVP | HSTATUS_SPV;

	/* SPP: kept same with hstatus.SPVP
	 * SPIE: enable interrupt after entering guest
	 */
	regs->status = cpu_csr_read(CSR_SSTATUS) | SSTATUS_SPP | SSTATUS_SPIE;

	return 0;
}

void arch_deinit_vcpu(struct acrn_vcpu *vcpu)
{
	(void)vcpu;
}

void arch_vcpu_thread(struct thread_object *obj)
{
	struct acrn_vcpu *vcpu = container_of(obj, struct acrn_vcpu, thread_obj);
	struct cpu_regs *regs = &(vcpu->arch.regs);

	cpu_csr_write(CSR_HSTATUS, regs->hstatus);

	/* load general context from vcpu struct */
	cpu_csr_write(CSR_SSCRATCH, (uint64_t)regs);

	/*
	 * Enter guest and never return.
	 * Next vCPU exit goes through trap gate.
	 */
	vcpu_exit_return();

	/* should not reach here */
	panic("Failed to enter guest");
}

void arch_reset_vcpu(struct acrn_vcpu *vcpu)
{
	struct cpu_regs *regs = &(vcpu->arch.regs);
	struct riscv_vcpu_host_ctx *hctx = &(vcpu->arch.hctx);
	struct riscv_vcpu_guest_ctx *gctx = &(vcpu->arch.gctx);
	struct riscv_vcpu_trap_info *trap = &(vcpu->arch.trap);

	memset(regs, 0, sizeof(struct cpu_regs));
	memset(hctx, 0, sizeof(struct riscv_vcpu_host_ctx));
	memset(gctx, 0, sizeof(struct riscv_vcpu_guest_ctx));
	memset(trap, 0, sizeof(struct riscv_vcpu_trap_info));
	trap->cause = EXCEPTION_INVALID;
	vcpu->arch.irqs_pending = 0UL;
	vcpu->arch.irqs_pending_mask = 0UL;
}

void arch_context_switch_out(struct thread_object *prev)
{
	struct acrn_vcpu *vcpu = container_of(prev, struct acrn_vcpu, thread_obj);
	unload_vcpu(vcpu);
}

void arch_context_switch_in(struct thread_object *next)
{
	struct acrn_vcpu *vcpu = container_of(next, struct acrn_vcpu, thread_obj);
	load_vcpu(vcpu);
}

uint64_t arch_build_stack_frame(struct acrn_vcpu *vcpu)
{
	uint64_t stacktop = (uint64_t)&vcpu->stack[CONFIG_STACK_SIZE];
	struct stack_frame *frame;
	uint64_t *ret;

	frame = (struct stack_frame *)stacktop;
	frame -= 1;

	memset(frame, 0, sizeof(struct stack_frame));

	frame->magic = SP_BOTTOM_MAGIC;
	frame->ra = (uint64_t)vcpu->thread_obj.thread_entry; /*return address*/
	frame->a0 = (uint64_t)&vcpu->thread_obj;

	ret = &frame->ra;

	return (uint64_t) ret;
}
