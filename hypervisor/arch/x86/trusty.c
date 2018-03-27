/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <acrn_hv_defs.h>
#include <hv_debug.h>

_Static_assert(NR_WORLD == 2, "Only 2 Worlds supported!");

/* Trusty EPT rebase gpa: 511G */
#define TRUSTY_EPT_REBASE_GPA (511ULL*1024ULL*1024ULL*1024ULL)

#define save_segment(seg, SEG_NAME) \
{ \
	seg.selector = exec_vmread(VMX_GUEST_##SEG_NAME##_SEL); \
	seg.base = exec_vmread(VMX_GUEST_##SEG_NAME##_BASE); \
	seg.limit = exec_vmread(VMX_GUEST_##SEG_NAME##_LIMIT); \
	seg.attr = exec_vmread(VMX_GUEST_##SEG_NAME##_ATTR); \
}

#define load_segment(seg, SEG_NAME) \
{ \
	exec_vmwrite(VMX_GUEST_##SEG_NAME##_SEL, seg.selector); \
	exec_vmwrite(VMX_GUEST_##SEG_NAME##_BASE, seg.base); \
	exec_vmwrite(VMX_GUEST_##SEG_NAME##_LIMIT, seg.limit); \
	exec_vmwrite(VMX_GUEST_##SEG_NAME##_ATTR, seg.attr); \
}

void create_secure_world_ept(struct vm *vm, uint64_t gpa,
		uint64_t size, uint64_t rebased_gpa)
{
	int i = 0;
	uint64_t pml4e = 0;
	uint64_t entry = 0;
	struct map_params map_params;
	uint64_t hpa = gpa2hpa(vm, gpa);
	struct vm *vm0 = get_vm_from_vmid(0);

	/* Create secure world eptp */
	if (vm->sworld_control.sworld_enabled && !vm->arch_vm.sworld_eptp)
		vm->arch_vm.sworld_eptp = alloc_paging_struct();

	map_params.page_table_type = PTT_EPT;
	map_params.pml4_inverted = vm->arch_vm.m2p;

	/* unmap gpa~gpa+size from guest ept mapping */
	map_params.pml4_base = vm->arch_vm.nworld_eptp;
	unmap_mem(&map_params, (void *)hpa, (void *)gpa, size, 0);

	/* Copy PDPT entries from Normal world to Secure world
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 * The PML4/PDPT for Secure world are separated from Normal World.
	 * PD/PT are shared in both Secure world's EPT and Normal World's EPT
	 */
	for (i = 0; i < IA32E_NUM_ENTRIES; i++) {
		pml4e = MEM_READ64(vm->arch_vm.nworld_eptp);
		entry = MEM_READ64((pml4e & IA32E_REF_MASK)
				+ (i * IA32E_COMM_ENTRY_SIZE));
		pml4e = MEM_READ64(vm->arch_vm.sworld_eptp);
		MEM_WRITE64((pml4e & IA32E_REF_MASK)
				+ (i * IA32E_COMM_ENTRY_SIZE),
				entry);
	}

	/* Map rebased_gpa~rebased_gpa+size
	 * to secure ept mapping
	 */
	map_params.pml4_base = vm->arch_vm.sworld_eptp;
	map_mem(&map_params, (void *)hpa,
			(void *)rebased_gpa, size,
			(MMU_MEM_ATTR_READ |
			 MMU_MEM_ATTR_WRITE |
			 MMU_MEM_ATTR_EXECUTE |
			 MMU_MEM_ATTR_WB_CACHE));

	/* Unap gap~gpa+size from sos ept mapping*/
	map_params.pml4_base = vm0->arch_vm.nworld_eptp;
	/* Get the gpa address in SOS */
	gpa = hpa2gpa(vm0, hpa);
	unmap_mem(&map_params, (void *)hpa, (void *)gpa, size, 0);

	/* Backup secure world info, will be used when
	 * destroy secure world */
	vm0->sworld_control.sworld_memory.base_gpa = gpa;
	vm0->sworld_control.sworld_memory.base_hpa = hpa;
	vm0->sworld_control.sworld_memory.length = size;

	mmu_invept(vm->current_vcpu);
	mmu_invept(vm0->current_vcpu);

}

static void save_world_ctx(struct run_context *context)
{
	/* VMCS Execution field */
	context->tsc_offset = exec_vmread64(VMX_TSC_OFFSET_FULL);

	/* VMCS GUEST field */
	/* CR3, RIP, RSP, RFLAGS already saved on VMEXIT */
	context->cr0 = exec_vmread(VMX_GUEST_CR0);
	context->cr4 = exec_vmread(VMX_GUEST_CR4);
	context->dr7 = exec_vmread(VMX_GUEST_DR7);
	context->ia32_debugctl = exec_vmread64(VMX_GUEST_IA32_DEBUGCTL_FULL);
	context->ia32_pat = exec_vmread64(VMX_GUEST_IA32_PAT_FULL);
	context->ia32_efer = exec_vmread64(VMX_GUEST_IA32_EFER_FULL);
	context->ia32_sysenter_cs = exec_vmread(VMX_GUEST_IA32_SYSENTER_CS);
	context->ia32_sysenter_esp = exec_vmread(VMX_GUEST_IA32_SYSENTER_ESP);
	context->ia32_sysenter_eip = exec_vmread(VMX_GUEST_IA32_SYSENTER_EIP);
	save_segment(context->cs, CS);
	save_segment(context->ss, SS);
	save_segment(context->ds, DS);
	save_segment(context->es, ES);
	save_segment(context->fs, FS);
	save_segment(context->gs, GS);
	save_segment(context->tr, TR);
	save_segment(context->ldtr, LDTR);
	/* Only base and limit for IDTR and GDTR */
	context->idtr.base = exec_vmread(VMX_GUEST_IDTR_BASE);
	context->idtr.limit = exec_vmread(VMX_GUEST_IDTR_LIMIT);
	context->gdtr.base = exec_vmread(VMX_GUEST_GDTR_BASE);
	context->gdtr.limit = exec_vmread(VMX_GUEST_GDTR_LIMIT);

	/* MSRs which not in the VMCS */
	context->ia32_star = msr_read(MSR_IA32_STAR);
	context->ia32_lstar = msr_read(MSR_IA32_LSTAR);
	context->ia32_fmask = msr_read(MSR_IA32_FMASK);
	context->ia32_kernel_gs_base = msr_read(MSR_IA32_KERNEL_GS_BASE);

	/* FX area */
	asm volatile("fxsave (%0)"
			: : "r" (context->fxstore_guest_area) : "memory");
}

static void load_world_ctx(struct run_context *context)
{
	/* VMCS Execution field */
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, context->tsc_offset);

	/* VMCS GUEST field */
	exec_vmwrite(VMX_GUEST_CR0, context->cr0);
	exec_vmwrite(VMX_GUEST_CR3, context->cr3);
	exec_vmwrite(VMX_GUEST_CR4, context->cr4);
	exec_vmwrite(VMX_GUEST_RIP, context->rip);
	exec_vmwrite(VMX_GUEST_RSP, context->rsp);
	exec_vmwrite(VMX_GUEST_RFLAGS, context->rflags);
	exec_vmwrite(VMX_GUEST_DR7, context->dr7);
	exec_vmwrite64(VMX_GUEST_IA32_DEBUGCTL_FULL, context->ia32_debugctl);
	exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, context->ia32_pat);
	exec_vmwrite64(VMX_GUEST_IA32_EFER_FULL, context->ia32_efer);
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_CS, context->ia32_sysenter_cs);
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_ESP, context->ia32_sysenter_esp);
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_EIP, context->ia32_sysenter_eip);
	load_segment(context->cs, CS);
	load_segment(context->ss, SS);
	load_segment(context->ds, DS);
	load_segment(context->es, ES);
	load_segment(context->fs, FS);
	load_segment(context->gs, GS);
	load_segment(context->tr, TR);
	load_segment(context->ldtr, LDTR);
	/* Only base and limit for IDTR and GDTR */
	exec_vmwrite(VMX_GUEST_IDTR_BASE, context->idtr.base);
	exec_vmwrite(VMX_GUEST_IDTR_LIMIT, context->idtr.limit);
	exec_vmwrite(VMX_GUEST_GDTR_BASE, context->gdtr.base);
	exec_vmwrite(VMX_GUEST_GDTR_LIMIT, context->gdtr.limit);

	/* MSRs which not in the VMCS */
	msr_write(MSR_IA32_STAR, context->ia32_star);
	msr_write(MSR_IA32_LSTAR, context->ia32_lstar);
	msr_write(MSR_IA32_FMASK, context->ia32_fmask);
	msr_write(MSR_IA32_KERNEL_GS_BASE, context->ia32_kernel_gs_base);

	/* FX area */
	asm volatile("fxrstor (%0)" : : "r" (context->fxstore_guest_area));
}

static void copy_smc_param(struct run_context *prev_ctx,
				struct run_context *next_ctx)
{
	next_ctx->guest_cpu_regs.regs.rdi = prev_ctx->guest_cpu_regs.regs.rdi;
	next_ctx->guest_cpu_regs.regs.rsi = prev_ctx->guest_cpu_regs.regs.rsi;
	next_ctx->guest_cpu_regs.regs.rdx = prev_ctx->guest_cpu_regs.regs.rdx;
	next_ctx->guest_cpu_regs.regs.rbx = prev_ctx->guest_cpu_regs.regs.rbx;
}

void switch_world(struct vcpu *vcpu, int next_world)
{
	struct vcpu_arch *arch_vcpu = &vcpu->arch_vcpu;

	/* save previous world context */
	save_world_ctx(&arch_vcpu->contexts[!next_world]);

	/* load next world context */
	load_world_ctx(&arch_vcpu->contexts[next_world]);

	/* Copy SMC parameters: RDI, RSI, RDX, RBX */
	copy_smc_param(&arch_vcpu->contexts[!next_world],
			&arch_vcpu->contexts[next_world]);

	/* load EPTP for next world */
	if (next_world == NORMAL_WORLD) {
		exec_vmwrite64(VMX_EPT_POINTER_FULL,
			((uint64_t)vcpu->vm->arch_vm.nworld_eptp) | (3<<3) | 6);
	} else {
		exec_vmwrite64(VMX_EPT_POINTER_FULL,
			((uint64_t)vcpu->vm->arch_vm.sworld_eptp) | (3<<3) | 6);
	}

	/* Update world index */
	arch_vcpu->cur_context = next_world;
}
