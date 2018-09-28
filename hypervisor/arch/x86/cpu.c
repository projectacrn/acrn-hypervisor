/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>
#include <version.h>
#include <reloc.h>

spinlock_t trampoline_spinlock = {
	.head = 0U,
	.tail = 0U
};

static spinlock_t up_count_spinlock = {
	.head = 0U,
	.tail = 0U
};

struct per_cpu_region *per_cpu_data_base_ptr;
uint16_t phys_cpu_num = 0U;
static uint64_t pcpu_sync = 0UL;
static volatile uint16_t up_count = 0U;
static uint64_t startup_paddr = 0UL;

/* physical cpu active bitmap, support up to 64 cpus */
uint64_t pcpu_active_bitmap = 0UL;

/* X2APIC mode is disabled by default. */
bool x2apic_enabled = false;

/* TODO: add more capability per requirement */
/* APICv features */
#define VAPIC_FEATURE_VIRT_ACCESS		(1U << 0)
#define VAPIC_FEATURE_VIRT_REG			(1U << 1)
#define VAPIC_FEATURE_INTR_DELIVERY		(1U << 2)
#define VAPIC_FEATURE_TPR_SHADOW		(1U << 3)
#define VAPIC_FEATURE_POST_INTR		(1U << 4)
#define VAPIC_FEATURE_VX2APIC_MODE		(1U << 5)

struct cpu_capability {
	uint8_t apicv_features;
	uint8_t ept_features;
};
static struct cpu_capability cpu_caps;

struct cpuinfo_x86 boot_cpu_data;

static void bsp_boot_post(void);
static void cpu_secondary_post(void);
static void cpu_cap_detect(void);
static void cpu_xsave_init(void);
static void set_current_cpu_id(uint16_t pcpu_id);
static void print_hv_banner(void);
static uint16_t get_cpu_id_from_lapic_id(uint8_t lapic_id);
int ibrs_type;
static uint64_t start_tsc __attribute__((__section__(".bss_noinit")));

/* Push sp magic to top of stack for call trace */
#define SWITCH_TO(rsp, to)                                              \
{                                                                       \
	asm volatile ("movq %0, %%rsp\n"                                \
			"pushq %1\n"                                    \
			"call %2\n"                                     \
			 :                                              \
			 : "r"(rsp), "rm"(SP_BOTTOM_MAGIC), "a"(to));   \
}

bool cpu_has_cap(uint32_t bit)
{
	uint32_t feat_idx = bit >> 5U;
	uint32_t feat_bit = bit & 0x1fU;

	if (feat_idx >= FEATURE_WORDS) {
		return false;
	}

	return ((boot_cpu_data.cpuid_leaves[feat_idx] & (1U << feat_bit)) != 0U);
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

static uint64_t get_address_mask(uint8_t limit)
{
	return ((1UL << limit) - 1UL) & CPU_PAGE_MASK;
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
		if (cpu_has_cap(X86_FEATURE_STIBP))
			ibrs_type = IBRS_OPT;
	}
#endif
}

/*
 * basic hardware capability check
 * we should supplement which feature/capability we must support
 * here later.
 */
static int hardware_detect_support(void)
{
	int ret;

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

	ret = check_vmx_mmu_cap();
	if (ret != 0) {
		return ret;
	}

	pr_acrnlog("hardware support HV");
	return 0;
}

static void alloc_phy_cpu_data(uint16_t pcpu_num)
{
	phys_cpu_num = pcpu_num;

	per_cpu_data_base_ptr = calloc(pcpu_num, sizeof(struct per_cpu_region));
	ASSERT(per_cpu_data_base_ptr != NULL, "");
}

uint16_t __attribute__((weak)) parse_madt(uint8_t lapic_id_array[MAX_PCPU_NUM])
{
	static const uint8_t lapic_id[] = {0U, 2U, 4U, 6U};
	uint32_t i;

	for (i = 0U; i < ARRAY_SIZE(lapic_id); i++) {
		lapic_id_array[i] = lapic_id[i];
	}

	return ((uint16_t)ARRAY_SIZE(lapic_id));
}

static void init_percpu_data_area(void)
{
	uint16_t i;
	uint16_t pcpu_num = 0U;
	uint8_t lapic_id_array[MAX_PCPU_NUM];

	/* Save all lapic_id detected via parse_mdt in lapic_id_array */
	pcpu_num = parse_madt(lapic_id_array);
	if (pcpu_num == 0U) {
		/* failed to get the physcial cpu number */
		ASSERT(false);
	}

	alloc_phy_cpu_data(pcpu_num);

	for (i = 0U; i < pcpu_num; i++) {
		per_cpu(lapic_id, i) = lapic_id_array[i];
	}

	ASSERT(get_cpu_id_from_lapic_id(get_cur_lapic_id()) != INVALID_CPU_ID,
		"fail to get phy cpu id");
}

static void cpu_set_current_state(uint16_t pcpu_id, enum cpu_state state)
{
	spinlock_obtain(&up_count_spinlock);

	/* Check if state is initializing */
	if (state == CPU_STATE_INITIALIZING) {
		/* Increment CPU up count */
		up_count++;

		/* Save this CPU's logical ID to the TSC AUX MSR */
		set_current_cpu_id(pcpu_id);
	}

	/* If cpu is dead, decrement CPU up count */
	if (state == CPU_STATE_DEAD) {
		up_count--;
	}

	/* Set state for the specified CPU */
	per_cpu(cpu_state, pcpu_id) = state;

	spinlock_release(&up_count_spinlock);
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

/* NOTE: this function is using temp stack, and after SWITCH_TO(runtime_sp, to)
 * it will switch to runtime stack.
 */
void bsp_boot_init(void)
{
	uint64_t rsp;

	start_tsc = rdtsc();

	/* Clear BSS */
	(void)memset(&_ld_bss_start, 0U,
			(size_t)(&_ld_bss_end - &_ld_bss_start));

	bitmap_set_nolock(BOOT_CPU_ID, &pcpu_active_bitmap);

	/* Get CPU capabilities thru CPUID, including the physical address bit
	 * limit which is required for initializing paging.
	 */
	get_cpu_capabilities();

	get_cpu_name();

	load_cpu_state_data();

	/* Initialize the hypervisor paging */
	init_paging();

	early_init_lapic();

	init_percpu_data_area();

	load_gdtr_and_tr();

	/* Switch to run-time stack */
	rsp = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1]);
	rsp &= ~(CPU_STACK_ALIGN - 1UL);
	SWITCH_TO(rsp, bsp_boot_post);
}

static void bsp_boot_post(void)
{
#ifdef STACK_PROTECTOR
	set_fs_base();
#endif

	cpu_cap_detect();

	cpu_xsave_init();

	/* Set state for this CPU to initializing */
	cpu_set_current_state(BOOT_CPU_ID, CPU_STATE_INITIALIZING);

	/* Perform any necessary BSP initialization */
	init_bsp();

	/* Initialize console */
	console_init();

	/* Print Hypervisor Banner */
	print_hv_banner();

	/* Make sure rdtsc is enabled */
	check_tsc();

	/* Calibrate TSC Frequency */
	calibrate_tsc();

	/* Enable logging */
	init_logmsg(CONFIG_LOG_DESTINATION);

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
	if (!cpu_has_cap(X86_FEATURE_IBRS_IBPB) &&
			!cpu_has_cap(X86_FEATURE_STIBP)) {
		pr_fatal("SECURITY WARNING!!!!!!");
		pr_fatal("Please apply the latest CPU uCode patch!");
	}

	enable_smep();

	/* Initialize the shell */
	shell_init();

	/* Initialize interrupts */
	interrupt_init(BOOT_CPU_ID);

	timer_init();
	setup_notification();
	ptdev_init();

	init_scheduler();

	/* Start all secondary cores */
	startup_paddr = prepare_trampoline();
	start_cpus();

	ASSERT(get_cpu_id() == BOOT_CPU_ID, "");

	init_iommu();

	console_setup_timer();

	exec_vmxon_instr(BOOT_CPU_ID);

	prepare_vm(BOOT_CPU_ID);

	default_idle();

	/* Control should not come here */
	cpu_dead(BOOT_CPU_ID);
}

/* NOTE: this function is using temp stack, and after SWITCH_TO(runtime_sp, to)
 * it will switch to runtime stack.
 */
void cpu_secondary_init(void)
{
	uint64_t rsp;

	/* Switch this CPU to use the same page tables set-up by the
	 * primary/boot CPU
	 */
	enable_paging(get_paging_pml4());

	enable_smep();

	early_init_lapic();

	/* Find the logical ID of this CPU given the LAPIC ID
	 * and Set state for this CPU to initializing
	 */
	cpu_set_current_state(get_cpu_id_from_lapic_id(get_cur_lapic_id()),
			      CPU_STATE_INITIALIZING);

	bitmap_set_nolock(get_cpu_id(), &pcpu_active_bitmap);

	/* Switch to run-time stack */
	rsp = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1]);
	rsp &= ~(CPU_STACK_ALIGN - 1UL);
	SWITCH_TO(rsp, cpu_secondary_post);
}

static void cpu_secondary_post(void)
{

	/* Release secondary boot spin-lock to allow one of the next CPU(s) to
	 * perform this common initialization
	 */
	spinlock_release(&trampoline_spinlock);

#ifdef STACK_PROTECTOR
	set_fs_base();
#endif

	load_gdtr_and_tr();

	/* Make sure rdtsc is enabled */
	check_tsc();

	pr_dbg("Core %hu is up", get_cpu_id());

	cpu_xsave_init();

	/* Initialize secondary processor interrupts. */
	interrupt_init(get_cpu_id());

	timer_init();

	/* Wait for boot processor to signal all secondary cores to continue */
	wait_sync_change(&pcpu_sync, 0UL);

	exec_vmxon_instr(get_cpu_id());

#ifdef CONFIG_PARTITION_MODE
	prepare_vm(get_cpu_id());
#endif

	default_idle();

	/* Control will only come here for secondary
	 * CPUs not configured for use.
	 */
	cpu_dead(get_cpu_id());
}

static uint16_t get_cpu_id_from_lapic_id(uint8_t lapic_id)
{
	uint16_t i;

	for (i = 0U; i < phys_cpu_num; i++) {
		if (per_cpu(lapic_id, i) == lapic_id) {
			return i;
		}
	}

	return INVALID_CPU_ID;
}

/*
 * Start all secondary CPUs.
 */
void start_cpus(void)
{
	uint32_t timeout;
	uint16_t expected_up;

	/* secondary cpu start up will wait for pcpu_sync -> 0UL */
	atomic_store64(&pcpu_sync, 1UL);

	/* Set flag showing number of CPUs expected to be up to all
	 * cpus
	 */
	expected_up = phys_cpu_num;

	/* Broadcast IPIs to all other CPUs,
	 * In this case, INTR_CPU_STARTUP_ALL_EX_SELF decides broadcasting
	 * IPIs, INVALID_CPU_ID is parameter value to destination pcpu_id.
	 */
	send_startup_ipi(INTR_CPU_STARTUP_ALL_EX_SELF,
			INVALID_CPU_ID, startup_paddr);

	/* Wait until global count is equal to expected CPU up count or
	 * configured time-out has expired
	 */
	timeout = CONFIG_CPU_UP_TIMEOUT * 1000U;
	while ((up_count != expected_up) && (timeout != 0U)) {
		/* Delay 10us */
		udelay(10U);

		/* Decrement timeout value */
		timeout -= 10U;
	}

	/* Check to see if all expected CPUs are actually up */
	if (up_count != expected_up) {
		/* Print error */
		pr_fatal("Secondary CPUs failed to come up");

		/* Error condition - loop endlessly for now */
		do {
		} while (1);
	}

	/* Trigger event to allow secondary CPUs to continue */
	atomic_store64(&pcpu_sync, 0UL);
}

void stop_cpus(void)
{
	uint16_t pcpu_id, expected_up;
	uint32_t timeout;

	timeout = CONFIG_CPU_UP_TIMEOUT * 1000U;
	for (pcpu_id = 0U; pcpu_id < phys_cpu_num; pcpu_id++) {
		if (get_cpu_id() == pcpu_id) {	/* avoid offline itself */
			continue;
		}

		make_pcpu_offline(pcpu_id);
	}

	expected_up = 1U;
	while ((up_count != expected_up) && (timeout != 0U)) {
		/* Delay 10us */
		udelay(10U);

		/* Decrement timeout value */
		timeout -= 10U;
	}

	if (up_count != expected_up) {
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

void cpu_dead(uint16_t pcpu_id)
{
	/* For debug purposes, using a stack variable in the while loop enables
	 * us to modify the value using a JTAG probe and resume if needed.
	 */
	int halt = 1;

	if (bitmap_test_and_clear_lock(pcpu_id, &pcpu_active_bitmap) == false) {
		pr_err("pcpu%hu already dead", pcpu_id);
		return;
	}

	/* clean up native stuff */
	vmx_off(pcpu_id);
	cache_flush_invalidate_all();

	/* Set state to show CPU is dead */
	cpu_set_current_state(pcpu_id, CPU_STATE_DEAD);

	/* Halt the CPU */
	do {
		asm volatile ("hlt");
	} while (halt != 0);
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

	/* Check if secondary processor based VM control is available. */
	if ((msr_val & (((uint64_t)VMX_PROCBASED_CTLS_SECONDARY) << 32)) == 0U)
		return;

	/* Read secondary processor based VM control. */
	msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS2);

	if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_EPT))
		cpu_caps.ept_features = 1U;
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

bool is_apicv_intr_delivery_supported(void)
{
	return ((cpu_caps.apicv_features & VAPIC_FEATURE_INTR_DELIVERY) != 0U);
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
