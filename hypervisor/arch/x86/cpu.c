/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>
#include <version.h>

#ifdef CONFIG_EFI_STUB
#include <acrn_efi.h>
#endif

spinlock_t trampoline_spinlock = {
	.head = 0,
	.tail = 0
};

spinlock_t up_count_spinlock = {
	.head = 0,
	.tail = 0
};

struct per_cpu_region *per_cpu_data_base_ptr;
uint16_t phys_cpu_num = 0U;
unsigned long pcpu_sync = 0;
volatile uint32_t up_count = 0;

/* physical cpu active bitmap, support up to 64 cpus */
uint64_t pcpu_active_bitmap = 0;

uint64_t trampoline_start16_paddr;

/* TODO: add more capability per requirement */
/*APICv features*/
#define VAPIC_FEATURE_VIRT_ACCESS		(1U << 0)
#define VAPIC_FEATURE_VIRT_REG			(1U << 1)
#define VAPIC_FEATURE_INTR_DELIVERY		(1U << 2)
#define VAPIC_FEATURE_TPR_SHADOW		(1U << 3)
#define VAPIC_FEATURE_POST_INTR		(1U << 4)
#define VAPIC_FEATURE_VX2APIC_MODE		(1U << 5)

struct cpu_capability {
	uint8_t vapic_features;
};
static struct cpu_capability cpu_caps;

struct cpuinfo_x86 boot_cpu_data;

static void bsp_boot_post(void);
static void cpu_secondary_post(void);
static void vapic_cap_detect(void);
static void cpu_xsave_init(void);
static void cpu_set_logical_id(uint32_t logical_id);
static void print_hv_banner(void);
int cpu_find_logical_id(uint8_t lapic_id);
static void pcpu_sync_sleep(unsigned long *sync, int mask_bit);
int ibrs_type;
static uint64_t __attribute__((__section__(".bss_noinit"))) start_tsc;

/* Push sp magic to top of stack for call trace */
#define SWITCH_TO(rsp, to)                                              \
{                                                                       \
	asm volatile ("movq %0, %%rsp\n"                                \
			"pushq %1\n"                                    \
			"call %2\n"                                     \
			 :                                              \
			 : "r"(rsp), "rm"(SP_BOTTOM_MAGIC), "a"(to));   \
}

inline bool cpu_has_cap(uint32_t bit)
{
	int feat_idx = bit >> 5;
	uint32_t feat_bit = bit & 0x1fU;

	if (feat_idx >= FEATURE_WORDS)
		return false;

	return ((boot_cpu_data.cpuid_leaves[feat_idx] & (1 << feat_bit)) != 0U);
}

static inline bool get_monitor_cap(void)
{
	if (cpu_has_cap(X86_FEATURE_MONITOR)) {
		/* don't use monitor for CPU (family: 0x6 model: 0x5c)
		 * in hypervisor, but still expose it to the guests and
		 * let them handle it correctly
		 */
		if (boot_cpu_data.x86 != 0x6 || boot_cpu_data.x86_model != 0x5c)
			return true;
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
	family = (eax >> 8) & 0xffU;
	if (family == 0xFU)
		family += (eax >> 20) & 0xffU;
	boot_cpu_data.x86 = family;

	model = (eax >> 4) & 0xfU;
	if (family >= 0x06U)
		model += ((eax >> 16) & 0xfU) << 4;
	boot_cpu_data.x86_model = model;


	cpuid(CPUID_EXTEND_FEATURE, &unused,
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_EBX],
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_ECX],
		&boot_cpu_data.cpuid_leaves[FEAT_7_0_EDX]);

	cpuid(CPUID_MAX_EXTENDED_FUNCTION,
		&boot_cpu_data.extended_cpuid_level,
		&unused, &unused, &unused);

	if (boot_cpu_data.extended_cpuid_level >= CPUID_EXTEND_FUNCTION_1)
		cpuid(CPUID_EXTEND_FUNCTION_1, &unused, &unused,
			&boot_cpu_data.cpuid_leaves[FEAT_8000_0001_ECX],
			&boot_cpu_data.cpuid_leaves[FEAT_8000_0001_EDX]);

	if (boot_cpu_data.extended_cpuid_level >= CPUID_EXTEND_ADDRESS_SIZE) {
		cpuid(CPUID_EXTEND_ADDRESS_SIZE, &eax,
			&boot_cpu_data.cpuid_leaves[FEAT_8000_0008_EBX],
			&unused, &unused);

			/* EAX bits 07-00: #Physical Address Bits
			 *     bits 15-08: #Linear Address Bits
			 */
			boot_cpu_data.x86_virt_bits = (eax >> 8) & 0xffU;
			boot_cpu_data.x86_phys_bits = eax & 0xffU;
			boot_cpu_data.physical_address_mask =
				get_address_mask(boot_cpu_data.x86_phys_bits);
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
	if ((boot_cpu_data.x86_phys_bits == 0) ||
		(boot_cpu_data.x86_virt_bits == 0)) {
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

	if (!cpu_has_cap(X86_FEATURE_VMX)) {
		pr_fatal("%s, vmx not supported\n", __func__);
		return -ENODEV;
	}

	if (!cpu_has_vmx_unrestricted_guest_cap()) {
		pr_fatal("%s, unrestricted guest not supported\n", __func__);
		return -ENODEV;
	}

	ret = check_vmx_mmu_cap();
	if (ret != 0)
		return ret;

	pr_acrnlog("hardware support HV");
	return 0;
}

static void alloc_phy_cpu_data(uint16_t pcpu_num)
{
	phys_cpu_num = pcpu_num;

	per_cpu_data_base_ptr = calloc(pcpu_num, sizeof(struct per_cpu_region));
	ASSERT(per_cpu_data_base_ptr != NULL, "");
}

uint16_t __attribute__((weak)) parse_madt(uint8_t *lapic_id_base)
{
	static const uint8_t lapic_id[] = {0U, 2U, 4U, 6U};
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(lapic_id); i++)
		*lapic_id_base++ = lapic_id[i];

	return ARRAY_SIZE(lapic_id);
}

static int init_phy_cpu_storage(void)
{
	int i;
	uint16_t pcpu_num=0U;
	int bsp_cpu_id;
	uint8_t bsp_lapic_id = 0;
	uint8_t *lapic_id_base;

	/*
	 * allocate memory to save all lapic_id detected in parse_mdt.
	 * We allocate 4K size which could save 4K CPUs lapic_id info.
	 */
	lapic_id_base = alloc_page();
	ASSERT(lapic_id_base != NULL, "fail to alloc page");

	pcpu_num = parse_madt(lapic_id_base);
	alloc_phy_cpu_data(pcpu_num);

	for (i = 0; i < pcpu_num; i++)
		per_cpu(lapic_id, i) = *lapic_id_base++;

	/* free memory after lapic_id are saved in per_cpu data */
	free(lapic_id_base);

	bsp_lapic_id = get_cur_lapic_id();

	bsp_cpu_id = cpu_find_logical_id(bsp_lapic_id);
	ASSERT(bsp_cpu_id >= 0, "fail to get phy cpu id");

	return bsp_cpu_id;
}

static void cpu_set_current_state(uint32_t logical_id, int state)
{
	spinlock_obtain(&up_count_spinlock);

	/* Check if state is initializing */
	if (state == CPU_STATE_INITIALIZING) {
		/* Increment CPU up count */
		up_count++;

		/* Save this CPU's logical ID to the TSC AUX MSR */
		cpu_set_logical_id(logical_id);
	}

	/* If cpu is dead, decrement CPU up count */
	if (state == CPU_STATE_DEAD)
		up_count--;

	/* Set state for the specified CPU */
	per_cpu(state, logical_id) = state;

	spinlock_release(&up_count_spinlock);
}

#ifdef STACK_PROTECTOR
static uint64_t get_random_value(void)
{
	uint64_t random = 0;

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
	struct stack_canary *psc = &get_cpu_var(stack_canary);

	psc->canary = get_random_value();
	msr_write(MSR_IA32_FS_BASE, (uint64_t)psc);
}
#endif

static void get_cpu_name(void)
{
	cpuid(CPUID_EXTEND_FUNCTION_2,
		(uint32_t *)(boot_cpu_data.model_name),
		(uint32_t *)(boot_cpu_data.model_name + 4),
		(uint32_t *)(boot_cpu_data.model_name + 8),
		(uint32_t *)(boot_cpu_data.model_name + 12));
	cpuid(CPUID_EXTEND_FUNCTION_3,
		(uint32_t *)(boot_cpu_data.model_name + 16),
		(uint32_t *)(boot_cpu_data.model_name + 20),
		(uint32_t *)(boot_cpu_data.model_name + 24),
		(uint32_t *)(boot_cpu_data.model_name + 28));
	cpuid(CPUID_EXTEND_FUNCTION_4,
		(uint32_t *)(boot_cpu_data.model_name + 32),
		(uint32_t *)(boot_cpu_data.model_name + 36),
		(uint32_t *)(boot_cpu_data.model_name + 40),
		(uint32_t *)(boot_cpu_data.model_name + 44));

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
	memset(_ld_bss_start, 0, _ld_bss_end - _ld_bss_start);

	/* Build time sanity checks to make sure hard-coded offset
	*  is matching the actual offset!
	*/
	ASSERT(sizeof(struct trusty_startup_param)
			+ sizeof(struct key_info) < 0x1000,
		"trusty_startup_param + key_info > 1Page size(4KB)!");

	ASSERT(NR_WORLD == 2, "Only 2 Worlds supported!");
	ASSERT(offsetof(struct cpu_regs, rax) ==
		CPU_CONTEXT_OFFSET_RAX,
		"cpu_regs rax offset not match");
	ASSERT(offsetof(struct cpu_regs, rbx) ==
		CPU_CONTEXT_OFFSET_RBX,
		"cpu_regs rbx offset not match");
	ASSERT(offsetof(struct cpu_regs, rcx) ==
		CPU_CONTEXT_OFFSET_RCX,
		"cpu_regs rcx offset not match");
	ASSERT(offsetof(struct cpu_regs, rdx) ==
		CPU_CONTEXT_OFFSET_RDX,
		"cpu_regs rdx offset not match");
	ASSERT(offsetof(struct cpu_regs, rbp) ==
		CPU_CONTEXT_OFFSET_RBP,
		"cpu_regs rbp offset not match");
	ASSERT(offsetof(struct cpu_regs, rsi) ==
		CPU_CONTEXT_OFFSET_RSI,
		"cpu_regs rsi offset not match");
	ASSERT(offsetof(struct cpu_regs, rdi) ==
		CPU_CONTEXT_OFFSET_RDI,
		"cpu_regs rdi offset not match");
	ASSERT(offsetof(struct cpu_regs, r8) ==
		CPU_CONTEXT_OFFSET_R8,
		"cpu_regs r8 offset not match");
	ASSERT(offsetof(struct cpu_regs, r9) ==
		CPU_CONTEXT_OFFSET_R9,
		"cpu_regs r9 offset not match");
	ASSERT(offsetof(struct cpu_regs, r10) ==
		CPU_CONTEXT_OFFSET_R10,
		"cpu_regs r10 offset not match");
	ASSERT(offsetof(struct cpu_regs, r11) ==
		CPU_CONTEXT_OFFSET_R11,
		"cpu_regs r11 offset not match");
	ASSERT(offsetof(struct cpu_regs, r12) ==
		CPU_CONTEXT_OFFSET_R12,
		"cpu_regs r12 offset not match");
	ASSERT(offsetof(struct cpu_regs, r13) ==
		CPU_CONTEXT_OFFSET_R13,
		"cpu_regs r13 offset not match");
	ASSERT(offsetof(struct cpu_regs, r14) ==
		CPU_CONTEXT_OFFSET_R14,
		"cpu_regs r14 offset not match");
	ASSERT(offsetof(struct cpu_regs, r15) ==
		CPU_CONTEXT_OFFSET_R15,
		"cpu_regs r15 offset not match");
	ASSERT(offsetof(struct run_context, cr2) ==
		CPU_CONTEXT_OFFSET_CR2,
		"run_context cr2 offset not match");
	ASSERT(offsetof(struct run_context, ia32_spec_ctrl) ==
		CPU_CONTEXT_OFFSET_IA32_SPEC_CTRL,
		"run_context ia32_spec_ctrl offset not match");

	__bitmap_set(CPU_BOOT_ID, &pcpu_active_bitmap);

	/* Get CPU capabilities thru CPUID, including the physical address bit
	 * limit which is required for initializing paging.
	 */
	get_cpu_capabilities();

	get_cpu_name();

	load_cpu_state_data();

	/* Initialize the hypervisor paging */
	init_paging();

	early_init_lapic();

	init_phy_cpu_storage();

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

	vapic_cap_detect();

	cpu_xsave_init();

	/* Set state for this CPU to initializing */
	cpu_set_current_state(CPU_BOOT_ID, CPU_STATE_INITIALIZING);

	/* Perform any necessary BSP initialization */
	init_bsp();

	/* Initialize Serial */
	serial_init();

	/* Initialize console */
	console_init();

	/* Print Hypervisor Banner */
	print_hv_banner();

	/* Make sure rdtsc is enabled */
	check_tsc();

	/* Calibrate TSC Frequency */
	calibrate_tsc();

	/* Enable logging */
	init_logmsg(CONFIG_LOG_BUF_SIZE,
		       CONFIG_LOG_DESTINATION);

	if (HV_RC_VERSION != 0)
		pr_acrnlog("HV version %d.%d-rc%d-%s-%s %s build by %s, start time %lluus",
			HV_MAJOR_VERSION, HV_MINOR_VERSION, HV_RC_VERSION,
			HV_BUILD_TIME, HV_BUILD_VERSION, HV_BUILD_TYPE,
			HV_BUILD_USER, TICKS_TO_US(start_tsc));
	else
		pr_acrnlog("HV version %d.%d-%s-%s %s build by %s, start time %lluus",
			HV_MAJOR_VERSION, HV_MINOR_VERSION,
			HV_BUILD_TIME, HV_BUILD_VERSION, HV_BUILD_TYPE,
			HV_BUILD_USER, TICKS_TO_US(start_tsc));

	pr_acrnlog("API version %d.%d",
			HV_API_MAJOR_VERSION, HV_API_MINOR_VERSION);

	pr_acrnlog("Detect processor: %s", boot_cpu_data.model_name);

	pr_dbg("Core %d is up", CPU_BOOT_ID);

	if (hardware_detect_support() != 0) {
		pr_fatal("hardware not support!\n");
		return;
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
	interrupt_init(CPU_BOOT_ID);

	timer_init();
	setup_notification();
	ptdev_init();

	init_scheduler();

	/* Start all secondary cores */
	start_cpus();

	/* Trigger event to allow secondary CPUs to continue */
	__bitmap_set(0U, &pcpu_sync);

	ASSERT(get_cpu_id() == CPU_BOOT_ID, "");

	if (init_iommu() != 0) {
		pr_fatal("%s, init iommu failed\n", __func__);
		return;
	}

	console_setup_timer();

	/* Start initializing the VM for this CPU */
	if (hv_main(CPU_BOOT_ID) != 0)
		panic("failed to start VM for bsp\n");

	/* Control should not come here */
	cpu_dead(CPU_BOOT_ID);
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
	cpu_set_current_state(cpu_find_logical_id
			      (get_cur_lapic_id()),
			      CPU_STATE_INITIALIZING);

	__bitmap_set(get_cpu_id(), &pcpu_active_bitmap);

	/* Switch to run-time stack */
	rsp = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1]);
	rsp &= ~(CPU_STACK_ALIGN - 1UL);
	SWITCH_TO(rsp, cpu_secondary_post);
}

static void cpu_secondary_post(void)
{
	int ret;

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

	pr_dbg("Core %d is up", get_cpu_id());

	cpu_xsave_init();

	/* Initialize secondary processor interrupts. */
	interrupt_init(get_cpu_id());

	timer_init();

	/* Wait for boot processor to signal all secondary cores to continue */
	pcpu_sync_sleep(&pcpu_sync, 0);

	ret = hv_main(get_cpu_id());
	if (ret != 0)
		panic("hv_main ret = %d\n", ret);

	/* Control will only come here for secondary CPUs not configured for
	 * use or if an error occurs in hv_main
	 */
	cpu_dead(get_cpu_id());
}

int cpu_find_logical_id(uint8_t lapic_id)
{
	int i;

	for (i = 0; i < phys_cpu_num; i++) {
		if (per_cpu(lapic_id, i) == lapic_id)
			return i;
	}

	return -1;
}

static void update_trampoline_code_refs(uint64_t dest_pa)
{
	void *ptr;
	uint64_t val;
	int i;

	/*
	 * calculate the fixup CS:IP according to fixup target address
	 * dynamically.
	 *
	 * trampoline code starts in real mode,
	 * so the target addres is HPA
	 */
	val = dest_pa + (uint64_t)trampoline_fixup_target;

	ptr = HPA2HVA(dest_pa + (uint64_t)trampoline_fixup_cs);
	*(uint16_t *)(ptr) = (uint16_t)(val >> 4) & 0xFFFFU;

	ptr = HPA2HVA(dest_pa + (uint64_t)trampoline_fixup_ip);
	*(uint16_t *)(ptr) = (uint16_t)(val & 0xfU);

	/* Update temporary page tables */
	ptr = HPA2HVA(dest_pa + (uint64_t)CPU_Boot_Page_Tables_ptr);
	*(uint32_t *)(ptr) += dest_pa;

	ptr = HPA2HVA(dest_pa + (uint64_t)CPU_Boot_Page_Tables_Start);
	*(uint64_t *)(ptr) += dest_pa;

	ptr = HPA2HVA(dest_pa + (uint64_t)trampoline_pdpt_addr);
	for (i = 0; i < 4; i++)
		*(uint64_t *)(ptr + sizeof(uint64_t) * i) += dest_pa;

	/* update the gdt base pointer with relocated offset */
	ptr = HPA2HVA(dest_pa + (uint64_t)trampoline_gdt_ptr);
	*(uint64_t *)(ptr + 2) += dest_pa;

	/* update trampoline jump pointer with relocated offset */
	ptr = HPA2HVA(dest_pa + (uint64_t)trampoline_start64_fixup);
	*(uint32_t *)ptr += dest_pa;
}

static uint64_t prepare_trampoline(void)
{
	uint64_t size, dest_pa;

	size = (uint64_t)_ld_trampoline_end - (uint64_t)trampoline_start16;
#ifndef CONFIG_EFI_STUB
	dest_pa = e820_alloc_low_memory(CONFIG_LOW_RAM_SIZE);
#else
	dest_pa = (uint64_t)get_ap_trampoline_buf();
#endif

	pr_dbg("trampoline code: %llx size %x", dest_pa, size);

	/* Copy segment for AP initialization code below 1MB */
	memcpy_s(HPA2HVA(dest_pa), size, _ld_trampoline_load, size);
	update_trampoline_code_refs(dest_pa);
	trampoline_start16_paddr = dest_pa;

	return dest_pa;
}

/*
 * Start all secondary CPUs.
 */
void start_cpus()
{
	uint32_t timeout;
	uint32_t expected_up;
	uint64_t startup_paddr;

	startup_paddr = prepare_trampoline();

	/* Set flag showing number of CPUs expected to be up to all
	 * cpus
	 */
	expected_up = phys_cpu_num;

	/* Broadcast IPIs to all other CPUs */
	send_startup_ipi(INTR_CPU_STARTUP_ALL_EX_SELF,
			-1U, startup_paddr);

	/* Wait until global count is equal to expected CPU up count or
	 * configured time-out has expired
	 */
	timeout = CONFIG_CPU_UP_TIMEOUT * 1000;
	while ((up_count != expected_up) && (timeout != 0)) {
		/* Delay 10us */
		udelay(10);

		/* Decrement timeout value */
		timeout -= 10;
	}

	/* Check to see if all expected CPUs are actually up */
	if (up_count != expected_up) {
		/* Print error */
		pr_fatal("Secondary CPUs failed to come up");

		/* Error condition - loop endlessly for now */
		do {
		} while (1);
	}
}

void stop_cpus()
{
	int i;
	uint32_t timeout, expected_up;

	timeout = CONFIG_CPU_UP_TIMEOUT * 1000;
	for (i = 0; i < phys_cpu_num; i++) {
		if (get_cpu_id() == i)	/* avoid offline itself */
			continue;

		make_pcpu_offline(i);
	}

	expected_up = 1;
	while ((up_count != expected_up) && (timeout !=0)) {
		/* Delay 10us */
		udelay(10);

		/* Decrement timeout value */
		timeout -= 10;
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

void cpu_dead(uint32_t logical_id)
{
	/* For debug purposes, using a stack variable in the while loop enables
	 * us to modify the value using a JTAG probe and resume if needed.
	 */
	int halt = 1;

	if (bitmap_test_and_clear(logical_id, &pcpu_active_bitmap) == false) {
		pr_err("pcpu%d already dead", logical_id);
		return;
	}

	/* Set state to show CPU is dead */
	cpu_set_current_state(logical_id, CPU_STATE_DEAD);

	/* clean up native stuff */
	timer_cleanup();
	vmx_off(logical_id);
	CACHE_FLUSH_INVALIDATE_ALL();

	/* Halt the CPU */
	do {
		asm volatile ("hlt");
	} while (halt != 0);
}

static void cpu_set_logical_id(uint32_t logical_id)
{
	/* Write TSC AUX register */
	msr_write(MSR_IA32_TSC_AUX, (uint64_t) logical_id);
}

static void print_hv_banner(void)
{
	char *boot_msg = "ACRN Hypervisor\n\r";

	/* Print the boot message */
	printf(boot_msg);
}

static void pcpu_sync_sleep(unsigned long *sync, int mask_bit)
{
	uint64_t wake_sync = (1UL << mask_bit);

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
	return ((((uint32_t)(msr_val >> 32)) & ctrl) == ctrl);
}

static void vapic_cap_detect(void)
{
	uint8_t features;
	uint64_t msr_val;

	features = 0;

	msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS);
	if (!is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS_TPR_SHADOW)) {
		cpu_caps.vapic_features = 0;
		return;
	}
	features |= VAPIC_FEATURE_TPR_SHADOW;

	msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS2);
	if (!is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VAPIC)) {
		cpu_caps.vapic_features = features;
		return;
	}
	features |= VAPIC_FEATURE_VIRT_ACCESS;

	if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VAPIC_REGS))
		features |= VAPIC_FEATURE_VIRT_REG;

	if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VX2APIC))
		features |= VAPIC_FEATURE_VX2APIC_MODE;

	if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VIRQ)) {
		features |= VAPIC_FEATURE_INTR_DELIVERY;

		msr_val = msr_read(MSR_IA32_VMX_PINBASED_CTLS);
		if (is_ctrl_setting_allowed(msr_val,
						VMX_PINBASED_CTLS_POST_IRQ))
			features |= VAPIC_FEATURE_POST_INTR;
	}

	cpu_caps.vapic_features = features;
}

bool is_vapic_supported(void)
{
	return ((cpu_caps.vapic_features & VAPIC_FEATURE_VIRT_ACCESS) != 0);
}

bool is_vapic_intr_delivery_supported(void)
{
	return ((cpu_caps.vapic_features & VAPIC_FEATURE_INTR_DELIVERY) != 0);
}

bool is_vapic_virt_reg_supported(void)
{
	return ((cpu_caps.vapic_features & VAPIC_FEATURE_VIRT_REG) != 0);
}

static void cpu_xsave_init(void)
{
	uint64_t val64;

	if (cpu_has_cap(X86_FEATURE_XSAVE)) {
		CPU_CR_READ(cr4, &val64);
		val64 |= CR4_OSXSAVE;
		CPU_CR_WRITE(cr4, val64);

		if (get_cpu_id() == CPU_BOOT_ID) {
			uint32_t ecx, unused;
			cpuid(CPUID_FEATURES, &unused, &unused, &ecx, &unused);

			/* if set, update it */
			if ((ecx & CPUID_ECX_OSXSAVE) != 0U)
				boot_cpu_data.cpuid_leaves[FEAT_1_ECX] |=
						CPUID_ECX_OSXSAVE;
		}
	}
}
