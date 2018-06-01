/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <hkdf.h>

#define TRUSTY_VERSION 1

struct trusty_mem {
	/* The first page of trusty memory is reserved for key_info and
	 * trusty_startup_param.
	 */
	union {
		struct {
			struct key_info key_info;
			struct trusty_startup_param startup_param;
		} data;
		uint8_t page[CPU_PAGE_SIZE];
	} first_page;

	/* The left memory is for trusty's code/data/heap/stack
	 */
	uint8_t left_mem[0];
};

static struct key_info g_key_info = {
	.size_of_this_struct = sizeof(g_key_info),
	.version = 0,
	.platform = 3,
	.num_seeds = 1
};

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

static void create_secure_world_ept(struct vm *vm, uint64_t gpa_orig,
		uint64_t size, uint64_t gpa_rebased)
{
	uint64_t nworld_pml4e = 0;
	uint64_t sworld_pml4e = 0;
	struct map_params map_params;
	uint64_t gpa = 0;
	uint64_t hpa = gpa2hpa(vm, gpa_orig);
	uint64_t table_present = (IA32E_EPT_R_BIT |
				IA32E_EPT_W_BIT |
				IA32E_EPT_X_BIT);
	void *sub_table_addr = NULL, *pml4_base = NULL;
	struct vm *vm0 = get_vm_from_vmid(0);

	if (vm0 == NULL) {
		pr_err("Parse vm0 context failed.");
		return;
	}

	if (!vm->sworld_control.sworld_enabled
			|| vm->arch_vm.sworld_eptp != 0) {
		pr_err("Sworld is not enabled or Sworld eptp is not NULL");
		return;
	}

	/* Check the physical address should be continuous */
	if (!check_continuous_hpa(vm, gpa_orig, size))	{
		ASSERT(false, "The physical addr is not continuous for Trusty");
		return;
	}

	map_params.page_table_type = PTT_EPT;
	map_params.pml4_inverted = HPA2HVA(vm->arch_vm.m2p);

	/* Unmap gpa_orig~gpa_orig+size from guest normal world ept mapping */
	map_params.pml4_base = HPA2HVA(vm->arch_vm.nworld_eptp);
	unmap_mem(&map_params, (void *)hpa, (void *)gpa_orig, size, 0);

	/* Copy PDPT entries from Normal world to Secure world
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 * The PML4/PDPT for Secure world are separated from
	 * Normal World.PD/PT are shared in both Secure world's EPT
	 * and Normal World's EPT
	 */
	pml4_base = alloc_paging_struct();
	vm->arch_vm.sworld_eptp = HVA2HPA(pml4_base);

	/* The trusty memory is remapped to guest physical address
	 * of gpa_rebased to gpa_rebased + size
	 */
	sub_table_addr = alloc_paging_struct();
	sworld_pml4e = HVA2HPA(sub_table_addr) | table_present;
	MEM_WRITE64(pml4_base, sworld_pml4e);


	nworld_pml4e = MEM_READ64(HPA2HVA(vm->arch_vm.nworld_eptp));
	memcpy_s(HPA2HVA(sworld_pml4e & IA32E_REF_MASK), CPU_PAGE_SIZE,
			HPA2HVA(nworld_pml4e & IA32E_REF_MASK), CPU_PAGE_SIZE);

	/* Map gpa_rebased~gpa_rebased+size
	 * to secure ept mapping
	 */
	map_params.pml4_base = pml4_base;
	map_mem(&map_params, (void *)hpa,
			(void *)gpa_rebased, size,
			(MMU_MEM_ATTR_READ |
			 MMU_MEM_ATTR_WRITE |
			 MMU_MEM_ATTR_EXECUTE |
			 MMU_MEM_ATTR_WB_CACHE));

	/* Unmap trusty memory space from sos ept mapping*/
	map_params.pml4_base = HPA2HVA(vm0->arch_vm.nworld_eptp);
	map_params.pml4_inverted = HPA2HVA(vm0->arch_vm.m2p);
	/* Get the gpa address in SOS */
	gpa = hpa2gpa(vm0, hpa);
	unmap_mem(&map_params, (void *)hpa, (void *)gpa, size, 0);

	/* Backup secure world info, will be used when
	 * destroy secure world */
	vm->sworld_control.sworld_memory.base_gpa = gpa;
	vm->sworld_control.sworld_memory.base_hpa = hpa;
	vm->sworld_control.sworld_memory.length = size;

	invept(vm->current_vcpu);
	invept(vm0->current_vcpu);

}

void  destroy_secure_world(struct vm *vm)
{
	struct map_params  map_params;
	struct vm *vm0 = get_vm_from_vmid(0);

	if (vm0 == NULL) {
		pr_err("Parse vm0 context failed.");
		return;
	}

	/* clear trusty memory space */
	memset(HPA2HVA(vm->sworld_control.sworld_memory.base_hpa),
			0, vm->sworld_control.sworld_memory.length);

	/* restore memory to SOS ept mapping */
	map_params.page_table_type = PTT_EPT;
	map_params.pml4_base = HPA2HVA(vm0->arch_vm.nworld_eptp);
	map_params.pml4_inverted = HPA2HVA(vm0->arch_vm.m2p);

	map_mem(&map_params, (void *)vm->sworld_control.sworld_memory.base_hpa,
			(void *)vm->sworld_control.sworld_memory.base_gpa,
			vm->sworld_control.sworld_memory.length,
			(MMU_MEM_ATTR_READ |
			 MMU_MEM_ATTR_WRITE |
			 MMU_MEM_ATTR_EXECUTE |
			 MMU_MEM_ATTR_WB_CACHE));

}

static void save_world_ctx(struct run_context *context)
{
	/* VMCS GUEST field */
	/* TSC_OFFSET, CR3, RIP, RSP, RFLAGS already saved on VMEXIT */
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
			vcpu->vm->arch_vm.nworld_eptp | (3<<3) | 6);
	} else {
		exec_vmwrite64(VMX_EPT_POINTER_FULL,
			vcpu->vm->arch_vm.sworld_eptp | (3<<3) | 6);
	}

	/* Update world index */
	arch_vcpu->cur_context = next_world;
}

/* Put key_info and trusty_startup_param in the first Page of Trusty
 * runtime memory
 */
static bool setup_trusty_info(struct vcpu *vcpu,
			uint32_t mem_size, uint64_t mem_base_hpa)
{
	uint32_t i;
	struct trusty_mem *mem;
	struct key_info *key_info;

	mem = (struct trusty_mem *)(HPA2HVA(mem_base_hpa));

	/* copy key_info to the first page of trusty memory */
	memcpy_s(&mem->first_page.data.key_info, sizeof(g_key_info),
			&g_key_info, sizeof(g_key_info));

	memset(mem->first_page.data.key_info.dseed_list, 0,
			sizeof(mem->first_page.data.key_info.dseed_list));
	/* Derive dvseed from dseed for Trusty */
	key_info = &mem->first_page.data.key_info;
	for (i = 0; i < g_key_info.num_seeds; i++) {
		if (!hkdf_sha256(key_info->dseed_list[i].seed,
				BUP_MKHI_BOOTLOADER_SEED_LEN,
				g_key_info.dseed_list[i].seed,
				BUP_MKHI_BOOTLOADER_SEED_LEN,
				NULL, 0,
				vcpu->vm->GUID, sizeof(vcpu->vm->GUID))) {
			memset(key_info, 0, sizeof(struct key_info));
			pr_err("%s: derive dvseed failed!", __func__);
			return false;
		}
	}

	/* Prepare trusty startup param */
	mem->first_page.data.startup_param.size_of_this_struct =
			sizeof(struct trusty_startup_param);
	mem->first_page.data.startup_param.mem_size = mem_size;
	mem->first_page.data.startup_param.tsc_per_ms = CYCLES_PER_MS;
	mem->first_page.data.startup_param.trusty_mem_base =
			TRUSTY_EPT_REBASE_GPA;

	/* According to trusty boot protocol, it will use RDI as the
	 * address(GPA) of startup_param on boot. Currently, the startup_param
	 * is put in the first page of trusty memory just followed by key_info.
	 */
	vcpu->arch_vcpu.contexts[SECURE_WORLD].guest_cpu_regs.regs.rdi
		= (uint64_t)TRUSTY_EPT_REBASE_GPA + sizeof(struct key_info);

	return true;
}

/* Secure World will reuse environment of UOS_Loder since they are
 * both booting from and running in 64bit mode, except GP registers.
 * RIP, RSP and RDI are specified below, other GP registers are leaved
 * as 0.
 */
static bool init_secure_world_env(struct vcpu *vcpu,
				uint64_t entry_gpa,
				uint64_t base_hpa,
				uint32_t size)
{
	vcpu->arch_vcpu.inst_len = 0;
	vcpu->arch_vcpu.contexts[SECURE_WORLD].rip = entry_gpa;
	vcpu->arch_vcpu.contexts[SECURE_WORLD].rsp =
		TRUSTY_EPT_REBASE_GPA + size;
	vcpu->arch_vcpu.contexts[SECURE_WORLD].tsc_offset = 0;

	vcpu->arch_vcpu.contexts[SECURE_WORLD].cr0 =
		vcpu->arch_vcpu.contexts[NORMAL_WORLD].cr0;
	vcpu->arch_vcpu.contexts[SECURE_WORLD].cr4 =
		vcpu->arch_vcpu.contexts[NORMAL_WORLD].cr4;

	exec_vmwrite(VMX_GUEST_RSP,
		TRUSTY_EPT_REBASE_GPA + size);
	exec_vmwrite(VMX_TSC_OFFSET_FULL,
		vcpu->arch_vcpu.contexts[SECURE_WORLD].tsc_offset);

	return setup_trusty_info(vcpu, size, base_hpa);
}

bool initialize_trusty(struct vcpu *vcpu, uint64_t param)
{
	uint64_t trusty_entry_gpa, trusty_base_gpa, trusty_base_hpa;
	struct vm *vm = vcpu->vm;
	struct trusty_boot_param *boot_param =
			(struct trusty_boot_param *)(gpa2hpa(vm, param));

	if (sizeof(struct trusty_boot_param) !=
			boot_param->size_of_this_struct) {
		pr_err("%s: sizeof(struct trusty_boot_param) mismatch!\n",
			__func__);
		return false;
	}

	if (boot_param->version != TRUSTY_VERSION) {
		pr_err("%s: version of(trusty_boot_param) mismatch!\n",
			__func__);
		return false;
	}

	if (!boot_param->entry_point) {
		pr_err("%s: Invalid entry point\n", __func__);
		return false;
	}

	if (!boot_param->base_addr) {
		pr_err("%s: Invalid memory base address\n", __func__);
		return false;
	}

	trusty_entry_gpa = (uint64_t)boot_param->entry_point;
	trusty_base_gpa = (uint64_t)boot_param->base_addr;

	create_secure_world_ept(vm, trusty_base_gpa, boot_param->mem_size,
						TRUSTY_EPT_REBASE_GPA);
	trusty_base_hpa = vm->sworld_control.sworld_memory.base_hpa;

	exec_vmwrite64(VMX_EPT_POINTER_FULL,
			vm->arch_vm.sworld_eptp | (3<<3) | 6);

	/* save Normal World context */
	save_world_ctx(&vcpu->arch_vcpu.contexts[NORMAL_WORLD]);

	/* init secure world environment */
	if (init_secure_world_env(vcpu,
		trusty_entry_gpa - trusty_base_gpa + TRUSTY_EPT_REBASE_GPA,
		trusty_base_hpa, boot_param->mem_size)) {

		/* switch to Secure World */
		vcpu->arch_vcpu.cur_context = SECURE_WORLD;
		return true;
	}

	return false;
}
