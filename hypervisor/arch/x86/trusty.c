/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <hkdf_wrap.h>

#define ACRN_DBG_TRUSTY 6U

#define TRUSTY_VERSION   1U
#define TRUSTY_VERSION_2 2U

struct trusty_mem {
	/* The first page of trusty memory is reserved for key_info and
	 * trusty_startup_param.
	 */
	union {
		struct {
			struct trusty_key_info key_info;
			struct trusty_startup_param startup_param;
		} data;
		uint8_t page[CPU_PAGE_SIZE];
	} first_page;

	/* The left memory is for trusty's code/data/heap/stack
	 */
	uint8_t left_mem[0];
};

static struct trusty_key_info g_key_info = {
	.size_of_this_struct = sizeof(g_key_info),
	.version = 0U,
	.platform = 3U,
	.num_seeds = 1U
};

/**
 * @defgroup trusty_apis Trusty APIs
 *
 * This is a special group that includes all APIs
 * related to Trusty
 *
 * @{
 */

/**
 * @brief Create Secure World EPT hierarchy
 *
 * Create Secure World EPT hierarchy, construct new PML4/PDPT, reuse PD/PT parse from
 * vm->arch_vm->ept
 *
 * @param vm pointer to a VM with 2 Worlds
 * @param gpa_orig original gpa allocated from vSBL
 * @param size LK size (16M by default)
 * @param gpa_rebased gpa rebased to offset xxx (511G_OFFSET)
 *
 */
static void create_secure_world_ept(struct vm *vm, uint64_t gpa_orig,
		uint64_t size, uint64_t gpa_rebased)
{
	uint64_t nworld_pml4e;
	uint64_t sworld_pml4e;
	uint64_t gpa;
	/* Check the HPA of parameter gpa_orig when invoking check_continuos_hpa */
	uint64_t hpa = gpa2hpa(vm, gpa_orig);
	uint64_t table_present = EPT_RWX;
	uint64_t pdpte, *dest_pdpte_p, *src_pdpte_p;
	void *sub_table_addr, *pml4_base;
	struct vm *vm0 = get_vm_from_vmid(0U);
	uint16_t i;

	if ((vm->sworld_control.flag.supported == 0UL)
			|| (vm->arch_vm.sworld_eptp != NULL)) {
		pr_err("Sworld is not supported or Sworld eptp is not NULL");
		return;
	}

	/**
	 * Check the HPA of parameter gpa_orig should exist
	 * Check the physical address should be continuous
	 */
	if (!check_continuous_hpa(vm, gpa_orig, size))	{
		ASSERT(false, "The physical addr is not continuous for Trusty");
		return;
	}

	/* Unmap gpa_orig~gpa_orig+size from guest normal world ept mapping */
	ept_mr_del(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
			gpa_orig, size);

	/* Copy PDPT entries from Normal world to Secure world
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 * The PML4/PDPT for Secure world are separated from
	 * Normal World.PD/PT are shared in both Secure world's EPT
	 * and Normal World's EPT
	 */
	pml4_base = vm->arch_vm.ept_mem_ops.info->ept.sworld_pgtable_base;
	(void)memset(pml4_base, 0U, CPU_PAGE_SIZE);
	vm->arch_vm.sworld_eptp = pml4_base;
	sanitize_pte((uint64_t *)vm->arch_vm.sworld_eptp);

	/* The trusty memory is remapped to guest physical address
	 * of gpa_rebased to gpa_rebased + size
	 */
	sub_table_addr = vm->arch_vm.ept_mem_ops.info->ept.sworld_pgtable_base + TRUSTY_PML4_PAGE_NUM(TRUSTY_EPT_REBASE_GPA);
	(void)memset(sub_table_addr, 0U, CPU_PAGE_SIZE);
	sworld_pml4e = hva2hpa(sub_table_addr) | table_present;
	set_pgentry((uint64_t *)pml4_base, sworld_pml4e);

	nworld_pml4e = get_pgentry((uint64_t *)vm->arch_vm.nworld_eptp);

	/*
	 * copy PTPDEs from normal world EPT to secure world EPT,
	 * and remove execute access attribute in these entries
	 */
	dest_pdpte_p = pml4e_page_vaddr(sworld_pml4e);
	src_pdpte_p = pml4e_page_vaddr(nworld_pml4e);
	for (i = 0U; i < PTRS_PER_PDPTE - 1; i++) {
		pdpte = get_pgentry(src_pdpte_p);
		if ((pdpte & table_present) != 0UL) {
			pdpte &= ~EPT_EXE;
			set_pgentry(dest_pdpte_p, pdpte);
		}
		src_pdpte_p++;
		dest_pdpte_p++;
	}

	/* Map [gpa_rebased, gpa_rebased + size) to secure ept mapping
	 */
	ept_mr_add(vm, (uint64_t *)vm->arch_vm.sworld_eptp,
			hpa, gpa_rebased, size, EPT_RWX | EPT_WB);

	/* Get the gpa address in SOS */
	gpa = vm0_hpa2gpa(hpa);

	/* Unmap trusty memory space from sos ept mapping*/
	ept_mr_del(vm0, (uint64_t *)vm0->arch_vm.nworld_eptp,
			gpa, size);

	/* Backup secure world info, will be used when
	 * destroy secure world and suspend UOS */
	vm->sworld_control.sworld_memory.base_gpa_in_sos = gpa;
	vm->sworld_control.sworld_memory.base_gpa_in_uos = gpa_orig;
	vm->sworld_control.sworld_memory.base_hpa = hpa;
	vm->sworld_control.sworld_memory.length = size;
}

void  destroy_secure_world(struct vm *vm, bool need_clr_mem)
{
	struct vm *vm0 = get_vm_from_vmid(0U);
	uint64_t hpa = vm->sworld_control.sworld_memory.base_hpa;
	uint64_t gpa_sos = vm->sworld_control.sworld_memory.base_gpa_in_sos;
	uint64_t gpa_uos = vm->sworld_control.sworld_memory.base_gpa_in_uos;
	uint64_t size = vm->sworld_control.sworld_memory.length;

	if (vm->arch_vm.sworld_eptp == NULL) {
		pr_err("sworld eptp is NULL, it's not created");
		return;
	}

	if (need_clr_mem) {
		/* clear trusty memory space */
		(void)memset(hpa2hva(hpa), 0U, size);
	}

	ept_mr_del(vm, vm->arch_vm.sworld_eptp, gpa_uos, size);
	/* sanitize trusty ept page-structures */
	sanitize_pte((uint64_t *)vm->arch_vm.sworld_eptp);
	vm->arch_vm.sworld_eptp = NULL;

	/* restore memory to SOS ept mapping */
	ept_mr_add(vm0, vm0->arch_vm.nworld_eptp,
			hpa, gpa_sos, size, EPT_RWX | EPT_WB);

	/* Restore memory to guest normal world */
	ept_mr_add(vm, vm->arch_vm.nworld_eptp,
			hpa, gpa_uos, size, EPT_RWX | EPT_WB);

}

static void save_world_ctx(struct acrn_vcpu *vcpu, struct ext_context *ext_ctx)
{
	/* cache on-demand run_context for efer/rflags/rsp/rip */
	(void)vcpu_get_efer(vcpu);
	(void)vcpu_get_rflags(vcpu);
	(void)vcpu_get_rsp(vcpu);
	(void)vcpu_get_rip(vcpu);

	/* VMCS GUEST field */
	ext_ctx->vmx_cr0 = exec_vmread(VMX_GUEST_CR0);
	ext_ctx->vmx_cr4 = exec_vmread(VMX_GUEST_CR4);
	ext_ctx->vmx_cr0_read_shadow = exec_vmread(VMX_CR0_READ_SHADOW);
	ext_ctx->vmx_cr4_read_shadow = exec_vmread(VMX_CR4_READ_SHADOW);
	ext_ctx->cr3 = exec_vmread(VMX_GUEST_CR3);
	ext_ctx->dr7 = exec_vmread(VMX_GUEST_DR7);
	ext_ctx->ia32_debugctl = exec_vmread64(VMX_GUEST_IA32_DEBUGCTL_FULL);

	/*
	 * Similar to CR0 and CR4, the actual value of guest's IA32_PAT MSR
	 * (represented by ext_ctx->vmx_ia32_pat) could be different from the
	 * value that guest reads (represented by ext_ctx->ia32_pat).
	 *
	 * the wrmsr handler keeps track of 'ia32_pat', and we only
	 * need to load 'vmx_ia32_pat' here.
	 */
	ext_ctx->vmx_ia32_pat = exec_vmread64(VMX_GUEST_IA32_PAT_FULL);
	ext_ctx->ia32_sysenter_esp = exec_vmread(VMX_GUEST_IA32_SYSENTER_ESP);
	ext_ctx->ia32_sysenter_eip = exec_vmread(VMX_GUEST_IA32_SYSENTER_EIP);
	ext_ctx->ia32_sysenter_cs = exec_vmread32(VMX_GUEST_IA32_SYSENTER_CS);
	save_segment(ext_ctx->cs, VMX_GUEST_CS);
	save_segment(ext_ctx->ss, VMX_GUEST_SS);
	save_segment(ext_ctx->ds, VMX_GUEST_DS);
	save_segment(ext_ctx->es, VMX_GUEST_ES);
	save_segment(ext_ctx->fs, VMX_GUEST_FS);
	save_segment(ext_ctx->gs, VMX_GUEST_GS);
	save_segment(ext_ctx->tr, VMX_GUEST_TR);
	save_segment(ext_ctx->ldtr, VMX_GUEST_LDTR);
	/* Only base and limit for IDTR and GDTR */
	ext_ctx->idtr.base = exec_vmread(VMX_GUEST_IDTR_BASE);
	ext_ctx->gdtr.base = exec_vmread(VMX_GUEST_GDTR_BASE);
	ext_ctx->idtr.limit = exec_vmread32(VMX_GUEST_IDTR_LIMIT);
	ext_ctx->gdtr.limit = exec_vmread32(VMX_GUEST_GDTR_LIMIT);

	/* MSRs which not in the VMCS */
	ext_ctx->ia32_star = msr_read(MSR_IA32_STAR);
	ext_ctx->ia32_lstar = msr_read(MSR_IA32_LSTAR);
	ext_ctx->ia32_fmask = msr_read(MSR_IA32_FMASK);
	ext_ctx->ia32_kernel_gs_base = msr_read(MSR_IA32_KERNEL_GS_BASE);

	/* FX area */
	asm volatile("fxsave (%0)"
			: : "r" (ext_ctx->fxstore_guest_area) : "memory");
}

static void load_world_ctx(struct acrn_vcpu *vcpu, const struct ext_context *ext_ctx)
{
	/* mark to update on-demand run_context for efer/rflags/rsp */
	bitmap_set_lock(CPU_REG_EFER, &vcpu->reg_updated);
	bitmap_set_lock(CPU_REG_RFLAGS, &vcpu->reg_updated);
	bitmap_set_lock(CPU_REG_RSP, &vcpu->reg_updated);
	bitmap_set_lock(CPU_REG_RIP, &vcpu->reg_updated);

	/* VMCS Execution field */
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, ext_ctx->tsc_offset);

	/* VMCS GUEST field */
	exec_vmwrite(VMX_GUEST_CR0, ext_ctx->vmx_cr0);
	exec_vmwrite(VMX_GUEST_CR4, ext_ctx->vmx_cr4);
	exec_vmwrite(VMX_CR0_READ_SHADOW, ext_ctx->vmx_cr0_read_shadow);
	exec_vmwrite(VMX_CR4_READ_SHADOW, ext_ctx->vmx_cr4_read_shadow);
	exec_vmwrite(VMX_GUEST_CR3, ext_ctx->cr3);
	exec_vmwrite(VMX_GUEST_DR7, ext_ctx->dr7);
	exec_vmwrite64(VMX_GUEST_IA32_DEBUGCTL_FULL, ext_ctx->ia32_debugctl);
	exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, ext_ctx->vmx_ia32_pat);
	exec_vmwrite32(VMX_GUEST_IA32_SYSENTER_CS, ext_ctx->ia32_sysenter_cs);
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_ESP, ext_ctx->ia32_sysenter_esp);
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_EIP, ext_ctx->ia32_sysenter_eip);
	load_segment(ext_ctx->cs, VMX_GUEST_CS);
	load_segment(ext_ctx->ss, VMX_GUEST_SS);
	load_segment(ext_ctx->ds, VMX_GUEST_DS);
	load_segment(ext_ctx->es, VMX_GUEST_ES);
	load_segment(ext_ctx->fs, VMX_GUEST_FS);
	load_segment(ext_ctx->gs, VMX_GUEST_GS);
	load_segment(ext_ctx->tr, VMX_GUEST_TR);
	load_segment(ext_ctx->ldtr, VMX_GUEST_LDTR);
	/* Only base and limit for IDTR and GDTR */
	exec_vmwrite(VMX_GUEST_IDTR_BASE, ext_ctx->idtr.base);
	exec_vmwrite(VMX_GUEST_GDTR_BASE, ext_ctx->gdtr.base);
	exec_vmwrite32(VMX_GUEST_IDTR_LIMIT, ext_ctx->idtr.limit);
	exec_vmwrite32(VMX_GUEST_GDTR_LIMIT, ext_ctx->gdtr.limit);

	/* MSRs which not in the VMCS */
	msr_write(MSR_IA32_STAR, ext_ctx->ia32_star);
	msr_write(MSR_IA32_LSTAR, ext_ctx->ia32_lstar);
	msr_write(MSR_IA32_FMASK, ext_ctx->ia32_fmask);
	msr_write(MSR_IA32_KERNEL_GS_BASE, ext_ctx->ia32_kernel_gs_base);

	/* FX area */
	asm volatile("fxrstor (%0)" : : "r" (ext_ctx->fxstore_guest_area));
}

static void copy_smc_param(const struct run_context *prev_ctx,
				struct run_context *next_ctx)
{
	next_ctx->guest_cpu_regs.regs.rdi = prev_ctx->guest_cpu_regs.regs.rdi;
	next_ctx->guest_cpu_regs.regs.rsi = prev_ctx->guest_cpu_regs.regs.rsi;
	next_ctx->guest_cpu_regs.regs.rdx = prev_ctx->guest_cpu_regs.regs.rdx;
	next_ctx->guest_cpu_regs.regs.rbx = prev_ctx->guest_cpu_regs.regs.rbx;
}

void switch_world(struct acrn_vcpu *vcpu, int next_world)
{
	struct vcpu_arch *arch_vcpu = &vcpu->arch_vcpu;

	/* save previous world context */
	save_world_ctx(vcpu, &arch_vcpu->contexts[!next_world].ext_ctx);

	/* load next world context */
	load_world_ctx(vcpu, &arch_vcpu->contexts[next_world].ext_ctx);

	/* Copy SMC parameters: RDI, RSI, RDX, RBX */
	copy_smc_param(&arch_vcpu->contexts[!next_world].run_ctx,
			&arch_vcpu->contexts[next_world].run_ctx);

	if (next_world == NORMAL_WORLD) {
		/* load EPTP for next world */
		exec_vmwrite64(VMX_EPT_POINTER_FULL,
			hva2hpa(vcpu->vm->arch_vm.nworld_eptp) |
			(3UL << 3U) | 0x6UL);

#ifndef CONFIG_L1D_FLUSH_VMENTRY_ENABLED
		cpu_l1d_flush();
#endif
	} else {
		exec_vmwrite64(VMX_EPT_POINTER_FULL,
			hva2hpa(vcpu->vm->arch_vm.sworld_eptp) |
			(3UL << 3U) | 0x6UL);
	}

	/* Update world index */
	arch_vcpu->cur_context = next_world;
}

/* Put key_info and trusty_startup_param in the first Page of Trusty
 * runtime memory
 */
static bool setup_trusty_info(struct acrn_vcpu *vcpu,
			uint32_t mem_size, uint64_t mem_base_hpa)
{
	uint32_t i;
	struct trusty_mem *mem;
	struct trusty_key_info *key_info;

	mem = (struct trusty_mem *)(hpa2hva(mem_base_hpa));

	/* copy key_info to the first page of trusty memory */
	(void)memcpy_s(&mem->first_page.data.key_info, sizeof(g_key_info),
			&g_key_info, sizeof(g_key_info));

	(void)memset(mem->first_page.data.key_info.dseed_list, 0U,
			sizeof(mem->first_page.data.key_info.dseed_list));
	/* Derive dvseed from dseed for Trusty */
	key_info = &mem->first_page.data.key_info;
	for (i = 0U; i < g_key_info.num_seeds; i++) {
		if (hkdf_sha256(key_info->dseed_list[i].seed,
				BUP_MKHI_BOOTLOADER_SEED_LEN,
				g_key_info.dseed_list[i].seed,
				BUP_MKHI_BOOTLOADER_SEED_LEN,
				NULL, 0U,
				vcpu->vm->GUID, sizeof(vcpu->vm->GUID)) == 0) {
			(void)memset(key_info, 0U, sizeof(struct trusty_key_info));
			pr_err("%s: derive dvseed failed!", __func__);
			return false;
		}
		key_info->dseed_list[i].cse_svn = g_key_info.dseed_list[i].cse_svn;
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
	vcpu->arch_vcpu.contexts[SECURE_WORLD].run_ctx.guest_cpu_regs.regs.rdi
		= (uint64_t)TRUSTY_EPT_REBASE_GPA + sizeof(struct trusty_key_info);

	return true;
}

/* Secure World will reuse environment of UOS_Loder since they are
 * both booting from and running in 64bit mode, except GP registers.
 * RIP, RSP and RDI are specified below, other GP registers are leaved
 * as 0.
 */
static bool init_secure_world_env(struct acrn_vcpu *vcpu,
				uint64_t entry_gpa,
				uint64_t base_hpa,
				uint32_t size)
{
	vcpu->arch_vcpu.inst_len = 0U;
	vcpu->arch_vcpu.contexts[SECURE_WORLD].run_ctx.rip = entry_gpa;
	vcpu->arch_vcpu.contexts[SECURE_WORLD].run_ctx.guest_cpu_regs.regs.rsp =
		TRUSTY_EPT_REBASE_GPA + size;

	vcpu->arch_vcpu.contexts[SECURE_WORLD].ext_ctx.tsc_offset = 0UL;
	vcpu->arch_vcpu.contexts[SECURE_WORLD].ext_ctx.ia32_pat =
		vcpu->arch_vcpu.contexts[NORMAL_WORLD].ext_ctx.ia32_pat;

	return setup_trusty_info(vcpu, size, base_hpa);
}

bool initialize_trusty(struct acrn_vcpu *vcpu, uint64_t param)
{
	uint64_t trusty_entry_gpa, trusty_base_gpa, trusty_base_hpa;
	uint32_t trusty_mem_size;
	struct vm *vm = vcpu->vm;
	struct trusty_boot_param boot_param;

	(void)memset(&boot_param, 0U, sizeof(boot_param));
	if (copy_from_gpa(vcpu->vm, &boot_param, param, sizeof(boot_param))
									!= 0) {
		pr_err("%s: Unable to copy trusty_boot_param\n", __func__);
		return false;
	}

	switch (boot_param.version) {
	case TRUSTY_VERSION_2:
		trusty_entry_gpa = ((uint64_t)boot_param.entry_point) |
			(((uint64_t)boot_param.entry_point_high) << 32U);
		trusty_base_gpa = ((uint64_t)boot_param.base_addr) |
			(((uint64_t)boot_param.base_addr_high) << 32U);

		/* copy rpmb_key from OSloader */
		(void)memcpy_s(&g_key_info.rpmb_key[0][0], 64U,
				&boot_param.rpmb_key[0], 64U);
		(void)memset(&boot_param.rpmb_key[0], 0U, 64U);
		break;
	case TRUSTY_VERSION:
		trusty_entry_gpa = (uint64_t)boot_param.entry_point;
		trusty_base_gpa = (uint64_t)boot_param.base_addr;
		break;
	default:
		dev_dbg(ACRN_DBG_TRUSTY, "%s: Version(%u) not supported!\n",
				__func__, boot_param.version);
		return false;
	}

	trusty_mem_size = boot_param.mem_size;

	create_secure_world_ept(vm, trusty_base_gpa, trusty_mem_size,
						TRUSTY_EPT_REBASE_GPA);
	trusty_base_hpa = vm->sworld_control.sworld_memory.base_hpa;

	exec_vmwrite64(VMX_EPT_POINTER_FULL,
			hva2hpa(vm->arch_vm.sworld_eptp) | (3UL << 3U) | 0x6UL);

	/* save Normal World context */
	save_world_ctx(vcpu, &vcpu->arch_vcpu.contexts[NORMAL_WORLD].ext_ctx);

	/* init secure world environment */
	if (init_secure_world_env(vcpu,
		(trusty_entry_gpa - trusty_base_gpa) + TRUSTY_EPT_REBASE_GPA,
		trusty_base_hpa, trusty_mem_size)) {

		/* switch to Secure World */
		vcpu->arch_vcpu.cur_context = SECURE_WORLD;
		return true;
	}

	return false;
}

void trusty_set_dseed(const void *dseed, uint8_t dseed_num)
{
	/* Use fake seed if input param is invalid */
	if ((dseed == NULL) || (dseed_num == 0U) ||
		(dseed_num > BOOTLOADER_SEED_MAX_ENTRIES)) {

		g_key_info.num_seeds = 1U;
		(void)memset(g_key_info.dseed_list[0].seed, 0xA5U,
			sizeof(g_key_info.dseed_list[0].seed));
		return;
	}

	g_key_info.num_seeds = dseed_num;
	(void)memcpy_s(&g_key_info.dseed_list,
			sizeof(struct seed_info) * dseed_num,
			dseed, sizeof(struct seed_info) * dseed_num);
}

void save_sworld_context(struct acrn_vcpu *vcpu)
{
	(void)memcpy_s(&vcpu->vm->sworld_snapshot,
			sizeof(struct cpu_context),
			&vcpu->arch_vcpu.contexts[SECURE_WORLD],
			sizeof(struct cpu_context));
}

void restore_sworld_context(struct acrn_vcpu *vcpu)
{
	struct secure_world_control *sworld_ctl =
		&vcpu->vm->sworld_control;

	create_secure_world_ept(vcpu->vm,
		sworld_ctl->sworld_memory.base_gpa_in_uos,
		sworld_ctl->sworld_memory.length,
		TRUSTY_EPT_REBASE_GPA);

	(void)memcpy_s(&vcpu->arch_vcpu.contexts[SECURE_WORLD],
			sizeof(struct cpu_context),
			&vcpu->vm->sworld_snapshot,
			sizeof(struct cpu_context));
}

/**
 * @}
 */
/* End of trusty_apis */
