/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * this file contains vmcs operations which is vcpu related
 */

#include <types.h>
#include <x86/guest/vmcs.h>
#include <x86/guest/vcpu.h>
#include <x86/guest/vm.h>
#include <x86/vmx.h>
#include <x86/gdt.h>
#include <x86/pgtable.h>
#include <x86/per_cpu.h>
#include <x86/cpu_caps.h>
#include <x86/cpufeatures.h>
#include <x86/guest/vmexit.h>
#include <logmsg.h>

/* rip, rsp, ia32_efer and rflags are written to VMCS in start_vcpu */
static void init_guest_vmx(struct acrn_vcpu *vcpu, uint64_t cr0, uint64_t cr3,
	uint64_t cr4)
{
	struct guest_cpu_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context];
	struct ext_context *ectx = &ctx->ext_ctx;

	pr_dbg("%s,cr0:0x%lx, cr4:0x%lx.", __func__, cr0, cr4);

	vcpu_set_cr4(vcpu, cr4);
	vcpu_set_cr0(vcpu, cr0);
	exec_vmwrite(VMX_GUEST_CR3, cr3);

	exec_vmwrite(VMX_GUEST_GDTR_BASE, ectx->gdtr.base);
	pr_dbg("VMX_GUEST_GDTR_BASE: 0x%016lx", ectx->gdtr.base);
	exec_vmwrite32(VMX_GUEST_GDTR_LIMIT, ectx->gdtr.limit);
	pr_dbg("VMX_GUEST_GDTR_LIMIT: 0x%016lx", ectx->gdtr.limit);

	exec_vmwrite(VMX_GUEST_IDTR_BASE, ectx->idtr.base);
	pr_dbg("VMX_GUEST_IDTR_BASE: 0x%016lx", ectx->idtr.base);
	exec_vmwrite32(VMX_GUEST_IDTR_LIMIT, ectx->idtr.limit);
	pr_dbg("VMX_GUEST_IDTR_LIMIT: 0x%016lx", ectx->idtr.limit);

	/* init segment selectors: es, cs, ss, ds, fs, gs, ldtr, tr */
	load_segment(ectx->cs, VMX_GUEST_CS);
	load_segment(ectx->ss, VMX_GUEST_SS);
	load_segment(ectx->ds, VMX_GUEST_DS);
	load_segment(ectx->es, VMX_GUEST_ES);
	load_segment(ectx->fs, VMX_GUEST_FS);
	load_segment(ectx->gs, VMX_GUEST_GS);
	load_segment(ectx->tr, VMX_GUEST_TR);
	load_segment(ectx->ldtr, VMX_GUEST_LDTR);

	/* init guest ia32_misc_enable value for guest read */
	vcpu_set_guest_msr(vcpu, MSR_IA32_MISC_ENABLE, msr_read(MSR_IA32_MISC_ENABLE));

	/* fixed values */
	exec_vmwrite32(VMX_GUEST_IA32_SYSENTER_CS, 0U);
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_ESP, 0UL);
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_EIP, 0UL);
	exec_vmwrite(VMX_GUEST_PENDING_DEBUG_EXCEPT, 0UL);
	exec_vmwrite(VMX_GUEST_IA32_DEBUGCTL_FULL, 0UL);
	exec_vmwrite32(VMX_GUEST_INTERRUPTIBILITY_INFO, 0U);
	exec_vmwrite32(VMX_GUEST_ACTIVITY_STATE, 0U);
	exec_vmwrite32(VMX_GUEST_SMBASE, 0U);
	vcpu_set_guest_msr(vcpu, MSR_IA32_PAT, PAT_POWER_ON_VALUE);
	exec_vmwrite(VMX_GUEST_IA32_PAT_FULL, PAT_POWER_ON_VALUE);
	exec_vmwrite(VMX_GUEST_DR7, DR7_INIT_VALUE);
}

static void init_guest_state(struct acrn_vcpu *vcpu)
{
	struct guest_cpu_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context];

	pr_dbg("%s, cr0:0x%lx, cr4:0x%lx.\n", __func__,
	ctx->run_ctx.cr0, ctx->run_ctx.cr4);

	init_guest_vmx(vcpu, ctx->run_ctx.cr0, ctx->ext_ctx.cr3,
			ctx->run_ctx.cr4 & ~(CR4_VMXE | CR4_SMXE | CR4_MCE));
}

static void init_host_state(void)
{
	uint16_t value16;
	uint64_t value64;
	uint64_t value;
	uint64_t tss_addr;
	uint64_t gdt_base;
	uint64_t idt_base;

	pr_dbg("Initialize host state");

	/***************************************************
	 * 16 - Bit fields
	 * Move the current ES, CS, SS, DS, FS, GS, TR, LDTR * values to the
	 * corresponding 16-bit host * segment selection (ES, CS, SS, DS, FS,
	 * GS), * Task Register (TR), * Local Descriptor Table Register (LDTR)
	 *
	 ***************************************************/
	CPU_SEG_READ(es, &value16);
	exec_vmwrite16(VMX_HOST_ES_SEL, value16);
	pr_dbg("VMX_HOST_ES_SEL: 0x%hx ", value16);

	CPU_SEG_READ(cs, &value16);
	exec_vmwrite16(VMX_HOST_CS_SEL, value16);
	pr_dbg("VMX_HOST_CS_SEL: 0x%hx ", value16);

	CPU_SEG_READ(ss, &value16);
	exec_vmwrite16(VMX_HOST_SS_SEL, value16);
	pr_dbg("VMX_HOST_SS_SEL: 0x%hx ", value16);

	CPU_SEG_READ(ds, &value16);
	exec_vmwrite16(VMX_HOST_DS_SEL, value16);
	pr_dbg("VMX_HOST_DS_SEL: 0x%hx ", value16);

	CPU_SEG_READ(fs, &value16);
	exec_vmwrite16(VMX_HOST_FS_SEL, value16);
	pr_dbg("VMX_HOST_FS_SEL: 0x%hx ", value16);

	CPU_SEG_READ(gs, &value16);
	exec_vmwrite16(VMX_HOST_GS_SEL, value16);
	pr_dbg("VMX_HOST_GS_SEL: 0x%hx ", value16);

	exec_vmwrite16(VMX_HOST_TR_SEL, HOST_GDT_RING0_CPU_TSS_SEL);
	pr_dbg("VMX_HOST_TR_SEL: 0x%hx ", HOST_GDT_RING0_CPU_TSS_SEL);

	/******************************************************
	 * 32-bit fields
	 * Set up the 32 bit host state fields - pg 3418 B.3.3 * Set limit for
	 * ES, CS, DD, DS, FS, GS, LDTR, Guest TR, * GDTR, and IDTR
	 ******************************************************/

	/* TODO: Should guest GDTB point to host GDTB ? */
	/* Obtain the current global descriptor table base */
	gdt_base = sgdt();

	if (((gdt_base >> 47U) & 0x1UL) != 0UL) {
	        gdt_base |= 0xffff000000000000UL;
	}

	/* Set up the guest and host GDTB base fields with current GDTB base */
	exec_vmwrite(VMX_HOST_GDTR_BASE, gdt_base);
	pr_dbg("VMX_HOST_GDTR_BASE: 0x%x ", gdt_base);

	tss_addr = hva2hpa((void *)&get_cpu_var(tss));
	/* Set up host TR base fields */
	exec_vmwrite(VMX_HOST_TR_BASE, tss_addr);
	pr_dbg("VMX_HOST_TR_BASE: 0x%016lx ", tss_addr);

	/* Obtain the current interrupt descriptor table base */
	idt_base = sidt();
	/* base */
	if (((idt_base >> 47U) & 0x1UL) != 0UL) {
		idt_base |= 0xffff000000000000UL;
	}

	exec_vmwrite(VMX_HOST_IDTR_BASE, idt_base);
	pr_dbg("VMX_HOST_IDTR_BASE: 0x%x ", idt_base);

	/**************************************************/
	/* 64-bit fields */
	pr_dbg("64-bit********");

	value64 = msr_read(MSR_IA32_PAT);
	exec_vmwrite64(VMX_HOST_IA32_PAT_FULL, value64);
	pr_dbg("VMX_HOST_IA32_PAT: 0x%016lx ", value64);

	value64 = msr_read(MSR_IA32_EFER);
	exec_vmwrite64(VMX_HOST_IA32_EFER_FULL, value64);
	pr_dbg("VMX_HOST_IA32_EFER: 0x%016lx ",
			value64);

	/**************************************************/
	/* Natural width fields */
	pr_dbg("Natural-width********");
	/* Set up host CR0 field */
	CPU_CR_READ(cr0, &value);
	exec_vmwrite(VMX_HOST_CR0, value);
	pr_dbg("VMX_HOST_CR0: 0x%016lx ", value);

	/* Set up host CR3 field */
	CPU_CR_READ(cr3, &value);
	exec_vmwrite(VMX_HOST_CR3, value);
	pr_dbg("VMX_HOST_CR3: 0x%016lx ", value);

	/* Set up host CR4 field */
	CPU_CR_READ(cr4, &value);
	exec_vmwrite(VMX_HOST_CR4, value);
	pr_dbg("VMX_HOST_CR4: 0x%016lx ", value);

	/* Set up host and guest FS base address */
	value = msr_read(MSR_IA32_FS_BASE);
	exec_vmwrite(VMX_HOST_FS_BASE, value);
	pr_dbg("VMX_HOST_FS_BASE: 0x%016lx ", value);
	value = msr_read(MSR_IA32_GS_BASE);
	exec_vmwrite(VMX_HOST_GS_BASE, value);
	pr_dbg("VMX_HOST_GS_BASE: 0x%016lx ", value);

	/* Set up host instruction pointer on VM Exit */
	value64 = (uint64_t)&vm_exit;
	pr_dbg("HOST RIP on VMExit %016lx ", value64);
	exec_vmwrite(VMX_HOST_RIP, value64);
	pr_dbg("vm exit return address = %016lx ", value64);

	/* As a type I hypervisor, just init sysenter fields to 0 */
	exec_vmwrite32(VMX_HOST_IA32_SYSENTER_CS, 0U);
	exec_vmwrite(VMX_HOST_IA32_SYSENTER_ESP, 0UL);
	exec_vmwrite(VMX_HOST_IA32_SYSENTER_EIP, 0UL);
}

static uint32_t check_vmx_ctrl(uint32_t msr, uint32_t ctrl_req)
{
	uint64_t vmx_msr;
	uint32_t vmx_msr_low, vmx_msr_high;
	uint32_t ctrl = ctrl_req;

	vmx_msr = msr_read(msr);
	vmx_msr_low  = (uint32_t)vmx_msr;
	vmx_msr_high = (uint32_t)(vmx_msr >> 32U);
	pr_dbg("VMX_PIN_VM_EXEC_CONTROLS:low=0x%x, high=0x%x\n",
			vmx_msr_low, vmx_msr_high);

	/* high 32b: must 0 setting
	 * low 32b:  must 1 setting
	 */
	ctrl &= vmx_msr_high;
	ctrl |= vmx_msr_low;

	if ((ctrl_req & ~ctrl) != 0U) {
		pr_err("VMX ctrl 0x%x not fully enabled: "
			"request 0x%x but get 0x%x\n",
			msr, ctrl_req, ctrl);
	}

	return ctrl;

}

static uint32_t check_vmx_ctrl_64(uint32_t msr, uint64_t ctrl_req)
{
	uint64_t vmx_msr;
	uint32_t ctrl = ctrl_req;

	vmx_msr = msr_read(msr);

	/* 64 bits are allowed 1-setting */
	ctrl &= vmx_msr;

	if ((ctrl_req & ~ctrl) != 0U) {
		pr_err("VMX ctrl 0x%x not fully enabled: "
			"request 0x%llx but get 0x%llx\n",
			msr, ctrl_req, ctrl);
	}

	return ctrl;
}

static void init_exec_ctrl(struct acrn_vcpu *vcpu)
{
	uint32_t value32;
	uint64_t value64;
	struct acrn_vm *vm = vcpu->vm;

	/* Log messages to show initializing VMX execution controls */
	pr_dbg("Initialize execution control ");

	/* Set up VM Execution control to enable Set VM-exits on external
	 * interrupts preemption timer - pg 2899 24.6.1
	 */
	/* enable external interrupt VM Exit */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_PINBASED_CTLS, VMX_PINBASED_CTLS_IRQ_EXIT);

	if (is_apicv_advanced_feature_supported()) {
		value32 |= VMX_PINBASED_CTLS_POST_IRQ;
	}

	exec_vmwrite32(VMX_PIN_VM_EXEC_CONTROLS, value32);
	pr_dbg("VMX_PIN_VM_EXEC_CONTROLS: 0x%x ", value32);

	/* Set up primary processor based VM execution controls - pg 2900
	 * 24.6.2. Set up for:
	 * Enable TSC offsetting
	 * Enable TSC exiting
	 * guest access to IO bit-mapped ports causes VM exit
	 * guest access to MSR causes VM exit
	 * Activate secondary controls
	 * Activate tertiary controls
	 */
	/* These are bits 1,4-6,8,13-16, and 26, the corresponding bits of
	 * the IA32_VMX_PROCBASED_CTRLS MSR are always read as 1 --- A.3.2
	 */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_PROCBASED_CTLS,
			 VMX_PROCBASED_CTLS_TSC_OFF | VMX_PROCBASED_CTLS_TPR_SHADOW |
			 VMX_PROCBASED_CTLS_IO_BITMAP | VMX_PROCBASED_CTLS_MSR_BITMAP |
			 VMX_PROCBASED_CTLS_HLT | VMX_PROCBASED_CTLS_SECONDARY | VMX_PROCBASED_CTLS_TERTIARY);

	/*Disable VM_EXIT for CR3 access*/
	value32 &= ~(VMX_PROCBASED_CTLS_CR3_LOAD | VMX_PROCBASED_CTLS_CR3_STORE);
	value32 &= ~(VMX_PROCBASED_CTLS_CR8_LOAD | VMX_PROCBASED_CTLS_CR8_STORE);

	/*
	 * Disable VM_EXIT for invlpg execution.
	 */
	value32 &= ~VMX_PROCBASED_CTLS_INVLPG;

	/*
	 * Enable VM_EXIT for rdpmc execution.
	 */
	value32 |= VMX_PROCBASED_CTLS_RDPMC;
	vcpu->arch.proc_vm_exec_ctrls = value32;
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);
	pr_dbg("VMX_PROC_VM_EXEC_CONTROLS: 0x%x ", value32);

	/* Set up secondary processor based VM execution controls - pg 2901
	 * 24.6.2. Set up for: * Enable EPT * Eable VPID * Enable RDTSCP *
	 * Enable Unrestricted guest (optional)
	 */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_PROCBASED_CTLS2,
			VMX_PROCBASED_CTLS2_VAPIC | VMX_PROCBASED_CTLS2_EPT |VMX_PROCBASED_CTLS2_VPID |
			VMX_PROCBASED_CTLS2_RDTSCP | VMX_PROCBASED_CTLS2_UNRESTRICT |
			VMX_PROCBASED_CTLS2_PAUSE_LOOP);

	/* SDM Vol3, 25.3,  setting "enable INVPCID" VM-execution to 1 with "INVLPG exiting" disabled,
	 * passes-through INVPCID instruction to guest if the instruction is supported.
	 */
	if (pcpu_has_cap(X86_FEATURE_INVPCID)) {
		value32 |= VMX_PROCBASED_CTLS2_INVPCID;
	} else {
		value32 &= ~VMX_PROCBASED_CTLS2_INVPCID;
	}

	if (is_apicv_advanced_feature_supported()) {
		value32 |= VMX_PROCBASED_CTLS2_VIRQ;
		value32 |= VMX_PROCBASED_CTLS2_VAPIC_REGS;
	} else {
		/*
		 * This field exists only on processors that support
		 * the 1-setting  of the "use TPR shadow"
		 * VM-execution control.
		 *
		 * Set up TPR threshold for virtual interrupt delivery
		 * - pg 2904 24.6.8
		 */
		exec_vmwrite32(VMX_TPR_THRESHOLD, 0U);
	}

	if (pcpu_has_cap(X86_FEATURE_OSXSAVE)) {
		exec_vmwrite64(VMX_XSS_EXITING_BITMAP_FULL, 0UL);
		value32 |= VMX_PROCBASED_CTLS2_XSVE_XRSTR;
	}

	value32 |= VMX_PROCBASED_CTLS2_WBINVD;

	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);
	pr_dbg("VMX_PROC_VM_EXEC_CONTROLS2: 0x%x ", value32);

	/* Set up tertiary processor based VM execution controls */
	if ((exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS) & VMX_PROCBASED_CTLS_TERTIARY) != 0U) {
		/* Enable KeyLocker if support */
		value64 = check_vmx_ctrl_64(MSR_IA32_VMX_PROCBASED_CTLS3, VMX_PROCBASED_CTLS3_LOADIWKEY);

		exec_vmwrite64(VMX_PROC_VM_EXEC_CONTROLS3_FULL, value64);
		pr_dbg("VMX_PROC_VM_EXEC_CONTROLS3: 0x%llx ", value64);
	}

	/*APIC-v, config APIC-access address*/
	value64 = vlapic_apicv_get_apic_access_addr();
	exec_vmwrite64(VMX_APIC_ACCESS_ADDR_FULL, value64);

	/*APIC-v, config APIC virtualized page address*/
	value64 = vlapic_apicv_get_apic_page_addr(vcpu_vlapic(vcpu));
	exec_vmwrite64(VMX_VIRTUAL_APIC_PAGE_ADDR_FULL, value64);

	if (is_apicv_advanced_feature_supported()) {
		/* Disable all EOI VMEXIT by default and
		 * clear RVI and SVI.
		 */
		exec_vmwrite64(VMX_EOI_EXIT0_FULL, 0UL);
		exec_vmwrite64(VMX_EOI_EXIT1_FULL, 0UL);
		exec_vmwrite64(VMX_EOI_EXIT2_FULL, 0UL);
		exec_vmwrite64(VMX_EOI_EXIT3_FULL, 0UL);

		exec_vmwrite16(VMX_GUEST_INTR_STATUS, 0U);
		exec_vmwrite16(VMX_POSTED_INTR_VECTOR, (uint16_t)vcpu->arch.pid.control.bits.nv);
		exec_vmwrite64(VMX_PIR_DESC_ADDR_FULL, hva2hpa(get_pi_desc(vcpu)));
	}

	/* Load EPTP execution control
	 * TODO: introduce API to make this data driven based
	 * on VMX_EPT_VPID_CAP
	 */
	value64 = hva2hpa(vm->arch_vm.nworld_eptp) | (3UL << 3U) | 6UL;
	exec_vmwrite64(VMX_EPT_POINTER_FULL, value64);
	pr_dbg("VMX_EPT_POINTER: 0x%016lx ", value64);

	/* Set up guest exception mask bitmap setting a bit * causes a VM exit
	 * on corresponding guest * exception - pg 2902 24.6.3
	 * enable VM exit on MC always
	 * enable AC for split-lock emulation when split-lock detection is enabled on physical platform.
	 */
	value32 = (1U << IDT_MC);
	if (is_ac_enabled()) {
		value32 = (value32 | (1U << IDT_AC));
	}
	exec_vmwrite32(VMX_EXCEPTION_BITMAP, value32);

	/* Set up page fault error code mask - second paragraph * pg 2902
	 * 24.6.3 - guest page fault exception causing * vmexit is governed by
	 * both VMX_EXCEPTION_BITMAP and * VMX_PF_ERROR_CODE_MASK
	 */
	exec_vmwrite32(VMX_PF_ERROR_CODE_MASK, 0U);

	/* Set up page fault error code match - second paragraph * pg 2902
	 * 24.6.3 - guest page fault exception causing * vmexit is governed by
	 * both VMX_EXCEPTION_BITMAP and * VMX_PF_ERROR_CODE_MATCH
	 */
	exec_vmwrite32(VMX_PF_ERROR_CODE_MATCH, 0U);

	/* Set up CR3 target count - An execution of mov to CR3 * by guest
	 * causes HW to evaluate operand match with * one of N CR3-Target Value
	 * registers. The CR3 target * count values tells the number of
	 * target-value regs to evaluate
	 */
	exec_vmwrite32(VMX_CR3_TARGET_COUNT, 0U);

	/* Set up IO bitmap register A and B - pg 2902 24.6.4 */
	value64 = hva2hpa(vm->arch_vm.io_bitmap);
	exec_vmwrite64(VMX_IO_BITMAP_A_FULL, value64);
	pr_dbg("VMX_IO_BITMAP_A: 0x%016lx ", value64);
	value64 = hva2hpa((void *)&(vm->arch_vm.io_bitmap[PAGE_SIZE]));
	exec_vmwrite64(VMX_IO_BITMAP_B_FULL, value64);
	pr_dbg("VMX_IO_BITMAP_B: 0x%016lx ", value64);

	init_msr_emulation(vcpu);

	/* Set up executive VMCS pointer - pg 2905 24.6.10 */
	exec_vmwrite64(VMX_EXECUTIVE_VMCS_PTR_FULL, 0UL);

	/* Setup Time stamp counter offset - pg 2902 24.6.5
	 * VMCS.OFFSET = vAdjust - pAdjust
	 */
	value64 = vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_ADJUST) - cpu_msr_read(MSR_IA32_TSC_ADJUST);
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, value64);

	/* Set up the link pointer */
	exec_vmwrite64(VMX_VMS_LINK_PTR_FULL, 0xFFFFFFFFFFFFFFFFUL);

	/* Natural-width */
	pr_dbg("Natural-width*********");

	init_cr0_cr4_host_guest_mask();

	/* The CR3 target registers work in concert with VMX_CR3_TARGET_COUNT
	 * field. Using these registers guest CR3 access can be managed. i.e.,
	 * if operand does not match one of these register values a VM exit
	 * would occur
	 */
	exec_vmwrite(VMX_CR3_TARGET_0, 0UL);
	exec_vmwrite(VMX_CR3_TARGET_1, 0UL);
	exec_vmwrite(VMX_CR3_TARGET_2, 0UL);
	exec_vmwrite(VMX_CR3_TARGET_3, 0UL);

	/* Setup PAUSE-loop exiting - 24.6.13 */
	exec_vmwrite(VMX_PLE_GAP, 128U);
	exec_vmwrite(VMX_PLE_WINDOW, 4096U);
}

static void init_entry_ctrl(const struct acrn_vcpu *vcpu)
{
	uint32_t value32;

	/* Log messages to show initializing VMX entry controls */
	pr_dbg("Initialize Entry control ");

	/* Set up VMX entry controls - pg 2908 24.8.1 * Set IA32e guest mode -
	 * on VM entry processor is in IA32e 64 bitmode * Start guest with host
	 * IA32_PAT and IA32_EFER
	 */
	value32 = (VMX_ENTRY_CTLS_LOAD_EFER |
		   VMX_ENTRY_CTLS_LOAD_PAT);

	if (get_vcpu_mode(vcpu) == CPU_MODE_64BIT) {
		value32 |= (VMX_ENTRY_CTLS_IA32E_MODE);
	}

	value32 = check_vmx_ctrl(MSR_IA32_VMX_ENTRY_CTLS, value32);

	exec_vmwrite32(VMX_ENTRY_CONTROLS, value32);
	pr_dbg("VMX_ENTRY_CONTROLS: 0x%x ", value32);

	/* Set up VMX entry MSR load count - pg 2908 24.8.2 Tells the number of
	 * MSRs on load from memory on VM entry from mem address provided by
	 * VM-entry MSR load address field
	 */
	exec_vmwrite32(VMX_ENTRY_MSR_LOAD_COUNT, vcpu->arch.msr_area.count);
	exec_vmwrite64(VMX_ENTRY_MSR_LOAD_ADDR_FULL, hva2hpa((void *)vcpu->arch.msr_area.guest));

	/* Set up VM entry interrupt information field pg 2909 24.8.3 */
	exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD, 0U);

	/* Set up VM entry exception error code - pg 2910 24.8.3 */
	exec_vmwrite32(VMX_ENTRY_EXCEPTION_ERROR_CODE, 0U);

	/* Set up VM entry instruction length - pg 2910 24.8.3 */
	exec_vmwrite32(VMX_ENTRY_INSTR_LENGTH, 0U);
}

static void init_exit_ctrl(const struct acrn_vcpu *vcpu)
{
	uint32_t value32;

	/* Log messages to show initializing VMX entry controls */
	pr_dbg("Initialize Exit control ");

	/* Set up VM exit controls - pg 2907 24.7.1 for: Host address space
	 * size is 64 bit Set up to acknowledge interrupt on exit, if 1 the HW
	 * acks the interrupt in VMX non-root and saves the interrupt vector to
	 * the relevant VM exit field for further processing by Hypervisor
	 * Enable saving and loading of IA32_PAT and IA32_EFER on VMEXIT Enable
	 * saving of pre-emption timer on VMEXIT
	 */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_EXIT_CTLS,
			 VMX_EXIT_CTLS_ACK_IRQ | VMX_EXIT_CTLS_SAVE_PAT |
			 VMX_EXIT_CTLS_LOAD_PAT | VMX_EXIT_CTLS_LOAD_EFER |
			 VMX_EXIT_CTLS_SAVE_EFER | VMX_EXIT_CTLS_HOST_ADDR64);

	exec_vmwrite32(VMX_EXIT_CONTROLS, value32);
	pr_dbg("VMX_EXIT_CONTROL: 0x%x ", value32);

	/* Set up VM exit MSR store and load counts pg 2908 24.7.2 - tells the
	 * HW number of MSRs to stored to mem and loaded from mem on VM exit.
	 * The 64 bit VM-exit MSR store and load address fields provide the
	 * corresponding addresses
	 */
	exec_vmwrite32(VMX_EXIT_MSR_STORE_COUNT, vcpu->arch.msr_area.count);
	exec_vmwrite32(VMX_EXIT_MSR_LOAD_COUNT, vcpu->arch.msr_area.count);
	exec_vmwrite64(VMX_EXIT_MSR_STORE_ADDR_FULL, hva2hpa((void *)vcpu->arch.msr_area.guest));
	exec_vmwrite64(VMX_EXIT_MSR_LOAD_ADDR_FULL, hva2hpa((void *)vcpu->arch.msr_area.host));
}

/**
 * @pre vcpu != NULL
 */
void init_vmcs(struct acrn_vcpu *vcpu)
{
	uint64_t vmx_rev_id;
	uint64_t vmcs_pa;
	void **vmcs_ptr = &get_cpu_var(vmcs_run);

	/* Log message */
	pr_dbg("Initializing VMCS");

	/* Obtain the VM Rev ID from HW and populate VMCS page with it */
	vmx_rev_id = msr_read(MSR_IA32_VMX_BASIC);
	(void)memcpy_s(vcpu->arch.vmcs, 4U, (void *)&vmx_rev_id, 4U);

	/* Execute VMCLEAR VMCS of this vcpu */
	vmcs_pa = hva2hpa(vcpu->arch.vmcs);
	exec_vmclear((void *)&vmcs_pa);

	/* Load VMCS pointer */
	exec_vmptrld((void *)&vmcs_pa);
	*vmcs_ptr = (void *)vcpu->arch.vmcs;

	/* Initialize the Virtual Machine Control Structure (VMCS) */
	init_host_state();
	/* init exec_ctrl needs to run before init_guest_state */
	init_exec_ctrl(vcpu);
	init_guest_state(vcpu);
	init_entry_ctrl(vcpu);
	init_exit_ctrl(vcpu);
}

/**
 * @pre vcpu != NULL
 */
void load_vmcs(const struct acrn_vcpu *vcpu)
{
	uint64_t vmcs_pa;
	void **vmcs_ptr = &get_cpu_var(vmcs_run);

	if (vcpu->launched && (*vmcs_ptr != (void *)vcpu->arch.vmcs)) {
		vmcs_pa = hva2hpa(vcpu->arch.vmcs);
		exec_vmptrld((void *)&vmcs_pa);
		*vmcs_ptr = (void *)vcpu->arch.vmcs;
	}
}

void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu)
{
	uint32_t value32;
	if (is_lapic_pt_configured(vcpu->vm)) {
		dev_dbg(DBG_LEVEL_LAPICPT, "%s: switching to x2apic and passthru", __func__);
		/*
		 * Disable external interrupt exiting and irq ack
		 * Disable posted interrupt processing
		 * update x2apic msr bitmap for pass-thru
		 * enable inteception only for ICR
		 * enable NMI exit as we will use NMI to kick vCPU thread
		 * disable pre-emption for TSC DEADLINE MSR
		 * Disable Register Virtualization and virtual interrupt delivery
		 * Disable "use TPR shadow"
		 */

		value32 = exec_vmread32(VMX_PIN_VM_EXEC_CONTROLS);
		value32 &= ~VMX_PINBASED_CTLS_IRQ_EXIT;
		if (is_apicv_advanced_feature_supported()) {
			value32 &= ~VMX_PINBASED_CTLS_POST_IRQ;
		}

		/*
		 * ACRN hypervisor needs to kick vCPU off VMX non-root mode to do some
		 * operations in hypervisor, such as interrupt/exception injection, EPT
		 * flush etc. For non lapic-pt vCPUs, we can use IPI to do so. But, it
		 * doesn't work for lapic-pt vCPUs as the IPI will be injected to VMs
		 * directly without vmexit. So, here we enable NMI-exiting and use NMI
		 * as notification signal after passthroughing the lapic to vCPU.
		 */
		value32 |= VMX_PINBASED_CTLS_NMI_EXIT | VMX_PINBASED_CTLS_VIRT_NMI;
		exec_vmwrite32(VMX_PIN_VM_EXEC_CONTROLS, value32);

		value32 = exec_vmread32(VMX_EXIT_CONTROLS);
		value32 &= ~VMX_EXIT_CTLS_ACK_IRQ;
		exec_vmwrite32(VMX_EXIT_CONTROLS, value32);

		vcpu->arch.proc_vm_exec_ctrls &= ~VMX_PROCBASED_CTLS_TPR_SHADOW;
		vcpu->arch.proc_vm_exec_ctrls &= ~VMX_PROCBASED_CTLS_HLT;
		exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, vcpu->arch.proc_vm_exec_ctrls);

		exec_vmwrite32(VMX_TPR_THRESHOLD, 0U);

		value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS2);
		value32 &= ~VMX_PROCBASED_CTLS2_PAUSE_LOOP;
		value32 &= ~VMX_PROCBASED_CTLS2_VAPIC;
		if (is_apicv_advanced_feature_supported()) {
			value32 &= ~VMX_PROCBASED_CTLS2_VIRQ;
			value32 &= ~VMX_PROCBASED_CTLS2_VAPIC_REGS;
		}
		exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);

		update_msr_bitmap_x2apic_passthru(vcpu);

		/*
		 * After passthroughing lapic to guest, we should use NMI signal to
		 * notify vcpu thread instead of IPI. Because the IPI will be delivered
		 * the guest directly without vmexit.
		 */
		vcpu->thread_obj.notify_mode = SCHED_NOTIFY_NMI;
	} else {
		value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS2);
		value32 &= ~VMX_PROCBASED_CTLS2_VAPIC;
		value32 |= VMX_PROCBASED_CTLS2_VX2APIC;
		exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);
		update_msr_bitmap_x2apic_apicv(vcpu);
	}
}
