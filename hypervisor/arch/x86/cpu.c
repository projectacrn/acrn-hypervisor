/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <acpi.h>
#include <schedule.h>
#include <version.h>
#include <trampoline.h>
#include <e820.h>

struct per_cpu_region per_cpu_data[CONFIG_MAX_PCPU_NUM] __aligned(PAGE_SIZE);
uint16_t phys_cpu_num = 0U;
static uint64_t pcpu_sync = 0UL;
static uint16_t up_count = 0U;
static uint64_t startup_paddr = 0UL;

/* physical cpu active bitmap, support up to 64 cpus */
uint64_t pcpu_active_bitmap = 0UL;

static bool skip_l1dfl_vmentry;
static uint64_t x86_arch_capabilities;

/* TODO: add more capability per requirement */
/* APICv features */
#define VAPIC_FEATURE_VIRT_ACCESS		(1U << 0U)
#define VAPIC_FEATURE_VIRT_REG			(1U << 1U)
#define VAPIC_FEATURE_INTR_DELIVERY		(1U << 2U)
#define VAPIC_FEATURE_TPR_SHADOW		(1U << 3U)
#define VAPIC_FEATURE_POST_INTR			(1U << 4U)
#define VAPIC_FEATURE_VX2APIC_MODE		(1U << 5U)

struct cpu_capability {
	uint8_t apicv_features;
	uint8_t ept_features;
};
static struct cpu_capability cpu_caps;

struct cpuinfo_x86 boot_cpu_data;

static void cpu_cap_detect(void);
static void cpu_xsave_init(void);
static void set_current_cpu_id(uint16_t pcpu_id);
static void print_hv_banner(void);
static uint16_t get_cpu_id_from_lapic_id(uint32_t lapic_id);
int32_t ibrs_type;
static uint64_t start_tsc __attribute__((__section__(".bss_noinit")));

bool cpu_has_cap(uint32_t bit)
{
	uint32_t feat_idx = bit >> 5U;
	uint32_t feat_bit = bit & 0x1fU;
	bool ret;

	if (feat_idx >= FEATURE_WORDS) {
		ret = false;
	} else {
		ret = ((boot_cpu_data.cpuid_leaves[feat_idx] & (1U << feat_bit)) != 0U);
	}

	return ret;
}

static inline bool get_monitor_cap(void)
{
	if (cpu_has_cap(X86_FEATURE_MONITOR)) {
		/* don't use monitor for CPU (family: 0x6 model: 0x5c)
		 * in hypervisor, but still expose it to the guests and
		 * let them handle it correctly
		 */
		if ((boot_cpu_data.family != 0x6U) || (boot_cpu_data.model != 0x5cU)) {
			return true;
		}
	}

	return false;
}

static inline bool is_fast_string_erms_supported_and_enabled(void)
{
	bool ret = false;
	uint32_t misc_enable = (uint32_t)msr_read(MSR_IA32_MISC_ENABLE);

	if ((misc_enable & MSR_IA32_MISC_ENABLE_FAST_STRING) == 0U) {
		pr_fatal("%s, fast string is not enabled\n", __func__);
	} else {
		if (!cpu_has_cap(X86_FEATURE_ERMS)) {
			pr_fatal("%s, enhanced rep movsb/stosb not supported\n", __func__);
		} else {
			ret = true;
		}
	}

	return ret;
}

static uint64_t get_address_mask(uint8_t limit)
{
	return ((1UL << limit) - 1UL) & PAGE_MASK;
}

static void get_cpu_capabilities(void)
{
	uint32_t eax, unused;
	uint32_t family, model;

	cpuid(CPUID_VENDORSTRING,
		&boot_cpu_data.cpuid_level,
		&unused, &unused, &unused);

	cpuid(CPUID_FEATURES, &eax, &unused,
		&boot_cpu_data.cpuid_leaves[FEAT_1_ECX],
		&boot_cpu_data.cpuid_leaves[FEAT_1_EDX]);
	family = (eax >> 8U) & 0xffU;
	if (family == 0xFU) {
		family += (eax >> 20U) & 0xffU;
	}
	boot_cpu_data.family = (uint8_t)family;

	model = (eax >> 4U) & 0xfU;
	if (family >= 0x06U) {
		model += ((eax >> 16U) & 0xfU) << 4U;
	}
	boot_cpu_data.model = (uint8_t)model;


	cpuid(CPUID_EXTEND_FEATURE, &unused,
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_EBX],
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_ECX],
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_EDX]);

	cpuid(CPUID_MAX_EXTENDED_FUNCTION,
		&boot_cpu_data.extended_cpuid_level,
		&unused, &unused, &unused);

	if (boot_cpu_data.extended_cpuid_level >= CPUID_EXTEND_FUNCTION_1) {
		cpuid(CPUID_EXTEND_FUNCTION_1, &unused, &unused,
			&boot_cpu_data.cpuid_leaves[FEAT_8000_0001_ECX],
			&boot_cpu_data.cpuid_leaves[FEAT_8000_0001_EDX]);
	}

	if (boot_cpu_data.extended_cpuid_level >= CPUID_EXTEND_ADDRESS_SIZE) {
		cpuid(CPUID_EXTEND_ADDRESS_SIZE, &eax,
			&boot_cpu_data.cpuid_leaves[FEAT_8000_0008_EBX],
			&unused, &unused);

			/* EAX bits 07-00: #Physical Address Bits
			 *     bits 15-08: #Linear Address Bits
			 */
			boot_cpu_data.virt_bits = (uint8_t)((eax >> 8U) & 0xffU);
			boot_cpu_data.phys_bits = (uint8_t)(eax & 0xffU);
			boot_cpu_data.physical_address_mask =
				get_address_mask(boot_cpu_data.phys_bits);
	}

	/* For speculation defence.
	 * The default way is to set IBRS at vmexit and then do IBPB at vcpu
	 * context switch(ibrs_type == IBRS_RAW).
	 * Now provide an optimized way (ibrs_type == IBRS_OPT) which set
	 * STIBP and do IBPB at vmexit,since having STIBP always set has less
	 * impact than having IBRS always set. Also since IBPB is already done
	 * at vmexit, it is no necessary to do so at vcpu context switch then.
	 */
	ibrs_type = IBRS_NONE;

	/* Currently for APL, if we enabled retpoline, then IBRS should not
	 * take effect
	 * TODO: add IA32_ARCH_CAPABILITIES[1] check, if this bit is set, IBRS
	 * should be set all the time instead of relying on retpoline
	 */
#ifndef CONFIG_RETPOLINE
	if (cpu_has_cap(X86_FEATURE_IBRS_IBPB)) {
		ibrs_type = IBRS_RAW;
		if (cpu_has_cap(X86_FEATURE_STIBP)) {
			ibrs_type = IBRS_OPT;
		}
	}
#endif
}

/*
 * basic hardware capability check
 * we should supplement which feature/capability we must support
 * here later.
 */
static int32_t hardware_detect_support(void)
{
	int32_t ret;

	/* Long Mode (x86-64, 64-bit support) */
	if (!cpu_has_cap(X86_FEATURE_LM)) {
		pr_fatal("%s, LM not supported\n", __func__);
		return -ENODEV;
	}
	if ((boot_cpu_data.phys_bits == 0U) ||
		(boot_cpu_data.virt_bits == 0U)) {
		pr_fatal("%s, can't detect Linear/Physical Address size\n",
			__func__);
		return -ENODEV;
	}

	/* lapic TSC deadline timer */
	if (!cpu_has_cap(X86_FEATURE_TSC_DEADLINE)) {
		pr_fatal("%s, TSC deadline not supported\n", __func__);
		return -ENODEV;
	}

	/* Execute Disable */
	if (!cpu_has_cap(X86_FEATURE_NX)) {
		pr_fatal("%s, NX not supported\n", __func__);
		return -ENODEV;
	}

	/* Supervisor-Mode Execution Prevention */
	if (!cpu_has_cap(X86_FEATURE_SMEP)) {
		pr_fatal("%s, SMEP not supported\n", __func__);
		return -ENODEV;
	}

	/* Supervisor-Mode Access Prevention */
	if (!cpu_has_cap(X86_FEATURE_SMAP)) {
		pr_fatal("%s, SMAP not supported\n", __func__);
		return -ENODEV;
	}

	if (!cpu_has_cap(X86_FEATURE_MTRR)) {
		pr_fatal("%s, MTRR not supported\n", __func__);
		return -ENODEV;
	}

	if (!cpu_has_cap(X86_FEATURE_PAGE1GB)) {
		pr_fatal("%s, not support 1GB page\n", __func__);
		return -ENODEV;
	}

	if (!cpu_has_cap(X86_FEATURE_VMX)) {
		pr_fatal("%s, vmx not supported\n", __func__);
		return -ENODEV;
	}

	if (!is_fast_string_erms_supported_and_enabled()) {
		return -ENODEV;
	}


	if (!cpu_has_vmx_unrestricted_guest_cap()) {
		pr_fatal("%s, unrestricted guest not supported\n", __func__);
		return -ENODEV;
	}

	if (!is_ept_supported()) {
		pr_fatal("%s, EPT not supported\n", __func__);
		return -ENODEV;
	}

	if (boot_cpu_data.cpuid_level < 0x15U) {
		pr_fatal("%s, required CPU feature not supported\n", __func__);
		return -ENODEV;
	}

	if (is_vmx_disabled()) {
		pr_fatal("%s, VMX can not be enabled\n", __func__);
		return -ENODEV;
	}

	if (phys_cpu_num > CONFIG_MAX_PCPU_NUM) {
		pr_fatal("%s, pcpu number(%d) is out of range\n", __func__, phys_cpu_num);
		return -ENODEV;
	}

	ret = check_vmx_mmu_cap();
	if (ret != 0) {
		return ret;
	}

	pr_acrnlog("hardware support HV");
	return 0;
}

static void init_percpu_lapic_id(void)
{
	uint16_t i;
	uint16_t pcpu_num = 0U;
	uint32_t lapic_id_array[CONFIG_MAX_PCPU_NUM];

	/* Save all lapic_id detected via parse_mdt in lapic_id_array */
	pcpu_num = parse_madt(lapic_id_array);
	if (pcpu_num == 0U) {
		/* failed to get the physcial cpu number */
		ASSERT(false);
	}

	phys_cpu_num = pcpu_num;

	for (i = 0U; (i < pcpu_num) && (i < CONFIG_MAX_PCPU_NUM); i++) {
		per_cpu(lapic_id, i) = lapic_id_array[i];
	}
}

static void cpu_set_current_state(uint16_t pcpu_id, enum pcpu_boot_state state)
{
	/* Check if state is initializing */
	if (state == PCPU_STATE_INITIALIZING) {
		/* Increment CPU up count */
		atomic_inc16(&up_count);

		/* Save this CPU's logical ID to the TSC AUX MSR */
		set_current_cpu_id(pcpu_id);
	}

	/* If cpu is dead, decrement CPU up count */
	if (state == PCPU_STATE_DEAD) {
		atomic_dec16(&up_count);
	}

	/* Set state for the specified CPU */
	per_cpu(boot_state, pcpu_id) = state;
}

#ifdef STACK_PROTECTOR
static uint64_t get_random_value(void)
{
	uint64_t random = 0UL;

	asm volatile ("1: rdrand %%rax\n"
			"jnc 1b\n"
			"mov %%rax, %0\n"
			: "=r"(random)
			:
			:"%rax");
	return random;
}

static void set_fs_base(void)
{
	struct stack_canary *psc = &get_cpu_var(stk_canary);

	psc->canary = get_random_value();
	msr_write(MSR_IA32_FS_BASE, (uint64_t)psc);
}
#endif

static void get_cpu_name(void)
{
	cpuid(CPUID_EXTEND_FUNCTION_2,
		(uint32_t *)(boot_cpu_data.model_name),
		(uint32_t *)(&boot_cpu_data.model_name[4]),
		(uint32_t *)(&boot_cpu_data.model_name[8]),
		(uint32_t *)(&boot_cpu_data.model_name[12]));
	cpuid(CPUID_EXTEND_FUNCTION_3,
		(uint32_t *)(&boot_cpu_data.model_name[16]),
		(uint32_t *)(&boot_cpu_data.model_name[20]),
		(uint32_t *)(&boot_cpu_data.model_name[24]),
		(uint32_t *)(&boot_cpu_data.model_name[28]));
	cpuid(CPUID_EXTEND_FUNCTION_4,
		(uint32_t *)(&boot_cpu_data.model_name[32]),
		(uint32_t *)(&boot_cpu_data.model_name[36]),
		(uint32_t *)(&boot_cpu_data.model_name[40]),
		(uint32_t *)(&boot_cpu_data.model_name[44]));

	boot_cpu_data.model_name[48] = '\0';
}

static bool check_cpu_security_config(void)
{
	if (cpu_has_cap(X86_FEATURE_ARCH_CAP)) {
		x86_arch_capabilities = msr_read(MSR_IA32_ARCH_CAPABILITIES);
		skip_l1dfl_vmentry = ((x86_arch_capabilities
			& IA32_ARCH_CAP_SKIP_L1DFL_VMENTRY) != 0UL);
	} else {
		return false;
	}

	if ((!cpu_has_cap(X86_FEATURE_L1D_FLUSH)) && (!skip_l1dfl_vmentry)) {
		return false;
	}

	if (!cpu_has_cap(X86_FEATURE_IBRS_IBPB) &&
		!cpu_has_cap(X86_FEATURE_STIBP)) {
		return false;
	}

	return true;
}

void init_cpu_pre(uint16_t pcpu_id)
{
	if (pcpu_id == BOOT_CPU_ID) {
		start_tsc = rdtsc();

		/* Clear BSS */
		(void)memset(&ld_bss_start, 0U,
				(size_t)(&ld_bss_end - &ld_bss_start));

		/* Get CPU capabilities thru CPUID, including the physical address bit
		 * limit which is required for initializing paging.
		 */
		get_cpu_capabilities();

		get_cpu_name();

		load_cpu_state_data();

		/* Initialize the hypervisor paging */
		init_e820();
		init_paging();

		if (!cpu_has_cap(X86_FEATURE_X2APIC)) {
			panic("x2APIC is not present!");
		}

		cpu_cap_detect();

		early_init_lapic();

		init_percpu_lapic_id();
	} else {
		/* Switch this CPU to use the same page tables set-up by the
		 * primary/boot CPU
		 */
		enable_paging();

		early_init_lapic();

		pcpu_id = get_cpu_id_from_lapic_id(get_cur_lapic_id());
		if (pcpu_id >= CONFIG_MAX_PCPU_NUM) {
			panic("Invalid pCPU ID!");
		}
	}

	bitmap_set_nolock(pcpu_id, &pcpu_active_bitmap);

	/* Set state for this CPU to initializing */
	cpu_set_current_state(pcpu_id, PCPU_STATE_INITIALIZING);
}

void init_cpu_post(uint16_t pcpu_id)
{
#ifdef STACK_PROTECTOR
	set_fs_base();
#endif
	load_gdtr_and_tr();

	enable_smep();

	enable_smap();

	cpu_xsave_init();

	if (pcpu_id == BOOT_CPU_ID) {
		/* Print Hypervisor Banner */
		print_hv_banner();

		/* Calibrate TSC Frequency */
		calibrate_tsc();

		pr_acrnlog("HV version %s-%s-%s %s (daily tag:%s) build by %s, start time %lluus",
				HV_FULL_VERSION,
				HV_BUILD_TIME, HV_BUILD_VERSION, HV_BUILD_TYPE,
				HV_DAILY_TAG,
				HV_BUILD_USER, ticks_to_us(start_tsc));

		pr_acrnlog("API version %u.%u",
				HV_API_MAJOR_VERSION, HV_API_MINOR_VERSION);

		pr_acrnlog("Detect processor: %s", boot_cpu_data.model_name);

		pr_dbg("Core %hu is up", BOOT_CPU_ID);

		if (hardware_detect_support() != 0) {
			panic("hardware not support!");
		}

		/* Warn for security feature not ready */
		if (!check_cpu_security_config()) {
			pr_fatal("SECURITY WARNING!!!!!!");
			pr_fatal("Please apply the latest CPU uCode patch!");
		}

		/* Initialize interrupts */
		interrupt_init(BOOT_CPU_ID);

		timer_init();
		setup_notification();
		setup_posted_intr_notification();

		/* Start all secondary cores */
		startup_paddr = prepare_trampoline();
		start_cpus();

		ASSERT(get_cpu_id() == BOOT_CPU_ID, "");
	} else {
		pr_dbg("Core %hu is up", pcpu_id);

		/* Initialize secondary processor interrupts. */
		interrupt_init(pcpu_id);

		timer_init();

		/* Wait for boot processor to signal all secondary cores to continue */
		wait_sync_change(&pcpu_sync, 0UL);
	}
}

static uint16_t get_cpu_id_from_lapic_id(uint32_t lapic_id)
{
	uint16_t i;

	for (i = 0U; (i < phys_cpu_num) && (i < CONFIG_MAX_PCPU_NUM); i++) {
		if (per_cpu(lapic_id, i) == lapic_id) {
			return i;
		}
	}

	return INVALID_CPU_ID;
}

static void start_cpu(uint16_t pcpu_id)
{
	uint32_t timeout;

	/* Update the stack for pcpu */
	stac();
	write_trampoline_stack_sym(pcpu_id);
	clac();

	send_startup_ipi(INTR_CPU_STARTUP_USE_DEST, pcpu_id, startup_paddr);

	/* Wait until the pcpu with pcpu_id is running and set the active bitmap or
	 * configured time-out has expired
	 */
	timeout = (uint32_t)CONFIG_CPU_UP_TIMEOUT * 1000U;
	while ((bitmap_test(pcpu_id, &pcpu_active_bitmap) == false) && (timeout != 0U)) {
		/* Delay 10us */
		udelay(10U);

		/* Decrement timeout value */
		timeout -= 10U;
	}

	/* Check to see if expected CPU is actually up */
	if (bitmap_test(pcpu_id, &pcpu_active_bitmap) == false) {
		/* Print error */
		pr_fatal("Secondary CPUs failed to come up");

		/* Error condition - loop endlessly for now */
		do {
		} while (1);
	}
}

void start_cpus(void)
{
	uint16_t i;

	/* secondary cpu start up will wait for pcpu_sync -> 0UL */
	atomic_store64(&pcpu_sync, 1UL);

	for (i = 0U; i < phys_cpu_num; i++) {
		if (get_cpu_id() == i) {
			continue;
		}

		start_cpu(i);
	}

	/* Trigger event to allow secondary CPUs to continue */
	atomic_store64(&pcpu_sync, 0UL);
}

void stop_cpus(void)
{
	uint16_t pcpu_id, expected_up;
	uint32_t timeout;

	timeout = (uint32_t)CONFIG_CPU_UP_TIMEOUT * 1000U;
	for (pcpu_id = 0U; pcpu_id < phys_cpu_num; pcpu_id++) {
		if (get_cpu_id() == pcpu_id) {	/* avoid offline itself */
			continue;
		}

		make_pcpu_offline(pcpu_id);
	}

	expected_up = 1U;
	while ((atomic_load16(&up_count) != expected_up) && (timeout != 0U)) {
		/* Delay 10us */
		udelay(10U);

		/* Decrement timeout value */
		timeout -= 10U;
	}

	if (atomic_load16(&up_count) != expected_up) {
		pr_fatal("Can't make all APs offline");

		/* if partial APs is down, it's not easy to recover
		 * per our current implementation (need make up dead
		 * APs one by one), just print error mesage and dead
		 * loop here.
		 *
		 * FIXME:
		 * We need to refine here to handle the AP offline
		 * failure for release/debug version. Ideally, we should
		 * define how to handle general unrecoverable error and
		 * follow it here.
		 */
		do {
		} while (1);
	}
}

void cpu_do_idle(void)
{
	__asm __volatile("pause" ::: "memory");
}

/**
 * only run on current pcpu
 */
void cpu_dead(void)
{
	/* For debug purposes, using a stack variable in the while loop enables
	 * us to modify the value using a JTAG probe and resume if needed.
	 */
	int32_t halt = 1;
	uint16_t pcpu_id = get_cpu_id();

	if (bitmap_test_and_clear_lock(pcpu_id, &pcpu_active_bitmap)) {
		/* clean up native stuff */
		vmx_off(pcpu_id);
		cache_flush_invalidate_all();

		/* Set state to show CPU is dead */
		cpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);

		/* Halt the CPU */
		do {
			hlt_cpu();
		} while (halt != 0);
	} else {
		pr_err("pcpu%hu already dead", pcpu_id);
	}
}

static void set_current_cpu_id(uint16_t pcpu_id)
{
	/* Write TSC AUX register */
	msr_write(MSR_IA32_TSC_AUX, (uint64_t) pcpu_id);
}

static void print_hv_banner(void)
{
	const char *boot_msg = "ACRN Hypervisor\n\r";

	/* Print the boot message */
	printf(boot_msg);
}

/* wait until *sync == wake_sync */
void wait_sync_change(uint64_t *sync, uint64_t wake_sync)
{
	if (get_monitor_cap()) {
		/* Wait for the event to be set using monitor/mwait */
		asm volatile ("1: cmpq      %%rbx,(%%rax)\n"
			      "   je        2f\n"
			      "   monitor\n"
			      "   mwait\n"
			      "   jmp       1b\n"
			      "2:\n"
			      :
			      : "a" (sync), "d"(0), "c"(0),
			      "b"(wake_sync)
			      : "cc");
	} else {
		/* Wait for the event to be set using pause */
		asm volatile ("1: cmpq      %%rbx,(%%rax)\n"
			      "   je        2f\n"
			      "   pause\n"
			      "   jmp       1b\n"
			      "2:\n"
			      :
			      : "a" (sync), "d"(0), "c"(0),
			      "b"(wake_sync)
			      : "cc");
	}
}

/*check allowed ONEs setting in vmx control*/
static bool is_ctrl_setting_allowed(uint64_t msr_val, uint32_t ctrl)
{
	/*
	 * Intel SDM Appendix A.3
	 * - bitX in ctrl can be set 1
	 *   only if bit 32+X in msr_val is 1
	 */
	return ((((uint32_t)(msr_val >> 32UL)) & ctrl) == ctrl);
}

static void ept_cap_detect(void)
{
	uint64_t msr_val;

	cpu_caps.ept_features = 0U;

	/* Read primary processor based VM control. */
	msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS);

	/*
	 * According to SDM A.3.2 Primary Processor-Based VM-Execution Controls:
	 * The IA32_VMX_PROCBASED_CTLS MSR (index 482H) reports on the allowed
	 * settings of most of the primary processor-based VM-execution controls
	 * (see Section 24.6.2):
	 * Bits 63:32 indicate the allowed 1-settings of these controls.
	 * VM entry allows control X to be 1 if bit 32+X in the MSR is set to 1;
	 * if bit 32+X in the MSR is cleared to 0, VM entry fails if control X
	 * is 1.
	 */
	msr_val = msr_val >> 32U;

	/* Check if secondary processor based VM control is available. */
	if ((msr_val & VMX_PROCBASED_CTLS_SECONDARY) != 0UL) {
		/* Read secondary processor based VM control. */
		msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS2);

		if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_EPT)) {
			cpu_caps.ept_features = 1U;
		}
	}
}

static void apicv_cap_detect(void)
{
	uint8_t features;
	uint64_t msr_val;

	msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS);
	if (!is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS_TPR_SHADOW)) {
		pr_fatal("APICv: No APIC TPR virtualization support.");
		return;
	}

	msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS2);
	if (!is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VAPIC)) {
		pr_fatal("APICv: No APIC-access virtualization support.");
		return;
	}

	if (!is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VAPIC_REGS)) {
		pr_fatal("APICv: No APIC-register virtualization support.");
		return;
	}

	features = (VAPIC_FEATURE_TPR_SHADOW
			| VAPIC_FEATURE_VIRT_ACCESS
			| VAPIC_FEATURE_VIRT_REG);

	if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VX2APIC)) {
		features |= VAPIC_FEATURE_VX2APIC_MODE;
	}

	if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VIRQ)) {
		features |= VAPIC_FEATURE_INTR_DELIVERY;

		msr_val = msr_read(MSR_IA32_VMX_PINBASED_CTLS);
		if (is_ctrl_setting_allowed(msr_val,
						VMX_PINBASED_CTLS_POST_IRQ)) {
			features |= VAPIC_FEATURE_POST_INTR;
		}
	}
	cpu_caps.apicv_features = features;
}

static void cpu_cap_detect(void)
{
	apicv_cap_detect();
	ept_cap_detect();
}

bool is_ept_supported(void)
{
	return (cpu_caps.ept_features != 0U);
}

bool is_apicv_reg_virtualization_supported(void)
{
	return ((cpu_caps.apicv_features & VAPIC_FEATURE_VIRT_REG) != 0U);
}

bool is_apicv_intr_delivery_supported(void)
{
	return ((cpu_caps.apicv_features & VAPIC_FEATURE_INTR_DELIVERY) != 0U);
}

bool is_apicv_posted_intr_supported(void)
{
	return ((cpu_caps.apicv_features & VAPIC_FEATURE_POST_INTR) != 0U);
}

static void cpu_xsave_init(void)
{
	uint64_t val64;

	if (cpu_has_cap(X86_FEATURE_XSAVE)) {
		CPU_CR_READ(cr4, &val64);
		val64 |= CR4_OSXSAVE;
		CPU_CR_WRITE(cr4, val64);

		if (get_cpu_id() == BOOT_CPU_ID) {
			uint32_t ecx, unused;
			cpuid(CPUID_FEATURES, &unused, &unused, &ecx, &unused);

			/* if set, update it */
			if ((ecx & CPUID_ECX_OSXSAVE) != 0U) {
				boot_cpu_data.cpuid_leaves[FEAT_1_ECX] |=
						CPUID_ECX_OSXSAVE;
			}
		}
	}
}

void cpu_l1d_flush(void)
{
	/*
	 * 'skip_l1dfl_vmentry' will be true on platform that
	 * is not affected by L1TF.
	 *
	 */
	if (!skip_l1dfl_vmentry) {
		if (cpu_has_cap(X86_FEATURE_L1D_FLUSH)) {
			msr_write(MSR_IA32_FLUSH_CMD, IA32_L1D_FLUSH);
		}
	}

}
