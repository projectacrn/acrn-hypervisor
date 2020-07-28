/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <bits.h>
#include <page.h>
#include <e820.h>
#include <mmu.h>
#include <vtd.h>
#include <lapic.h>
#include <per_cpu.h>
#include <cpufeatures.h>
#include <cpu_caps.h>
#include <acpi.h>
#include <ioapic.h>
#include <trampoline.h>
#include <cpuid.h>
#include <version.h>
#include <vmx.h>
#include <msr.h>
#include <ptdev.h>
#include <logmsg.h>
#include <rdt.h>
#include <vboot.h>
#include <sgx.h>
#include <uart16550.h>
#include <ivshmem.h>

#define CPU_UP_TIMEOUT		100U /* millisecond */
#define CPU_DOWN_TIMEOUT	100U /* millisecond */

struct per_cpu_region per_cpu_data[MAX_PCPU_NUM] __aligned(PAGE_SIZE);
static uint16_t phys_cpu_num = 0U;
static uint64_t pcpu_sync = 0UL;
static uint64_t startup_paddr = 0UL;

/* physical cpu active bitmap, support up to 64 cpus */
static volatile uint64_t pcpu_active_bitmap = 0UL;

static void init_pcpu_xsave(void);
static void set_current_pcpu_id(uint16_t pcpu_id);
static void print_hv_banner(void);
static uint16_t get_pcpu_id_from_lapic_id(uint32_t lapic_id);
static uint64_t start_tsc __attribute__((__section__(".bss_noinit")));

/**
 * @pre phys_cpu_num <= MAX_PCPU_NUM
 */
static bool init_percpu_lapic_id(void)
{
	uint16_t i;
	uint32_t lapic_id_array[MAX_PCPU_NUM];
	bool success = false;

	/* Save all lapic_id detected via parse_mdt in lapic_id_array */
	phys_cpu_num = parse_madt(lapic_id_array);

	if ((phys_cpu_num != 0U) && (phys_cpu_num <= MAX_PCPU_NUM)) {
		for (i = 0U; i < phys_cpu_num; i++) {
			per_cpu(lapic_id, i) = lapic_id_array[i];
		}
		success = true;
	}

	return success;
}

static void pcpu_set_current_state(uint16_t pcpu_id, enum pcpu_boot_state state)
{
	/* Check if state is initializing */
	if (state == PCPU_STATE_INITIALIZING) {

		/* Save this CPU's logical ID to the TSC AUX MSR */
		set_current_pcpu_id(pcpu_id);
	}

	/* Set state for the specified CPU */
	per_cpu(boot_state, pcpu_id) = state;
}

/*
 * @post return <= MAX_PCPU_NUM
 */
uint16_t get_pcpu_nums(void)
{
	return phys_cpu_num;
}

bool is_pcpu_active(uint16_t pcpu_id)
{
	return bitmap_test(pcpu_id, &pcpu_active_bitmap);
}

uint64_t get_active_pcpu_bitmap(void)
{
	return pcpu_active_bitmap;
}

static void enable_ac_for_splitlock(void)
{
#ifndef CONFIG_ENFORCE_TURNOFF_AC
	uint64_t test_ctl;

	if (has_core_cap(1U << 5U)) {
		test_ctl = msr_read(MSR_TEST_CTL);
		test_ctl |= (1U << 29U);
		msr_write(MSR_TEST_CTL, test_ctl);
	}
#endif /*CONFIG_ENFORCE_TURNOFF_AC*/
}

void init_pcpu_pre(bool is_bsp)
{
	uint16_t pcpu_id;
	int32_t ret;

	if (is_bsp) {
		pcpu_id = BSP_CPU_ID;
		start_tsc = rdtsc();

		/* Get CPU capabilities thru CPUID, including the physical address bit
		 * limit which is required for initializing paging.
		 */
		init_pcpu_capabilities();

		if (detect_hardware_support() != 0) {
			panic("hardware not support!");
		}

		if (sanitize_multiboot_info() != 0) {
			panic("Multiboot info error!");
		}

		init_pcpu_model_name();

		load_pcpu_state_data();

		/* Initialize the hypervisor paging */
		init_e820();
		init_paging();

		/*
		 * Need update uart_base_address here for vaddr2paddr mapping may changed
		 * WARNNING: DO NOT CALL PRINTF BETWEEN ENABLE PAGING IN init_paging AND HERE!
		 */
		uart16550_init(false);

		early_init_lapic();

		init_vboot();
#ifdef CONFIG_ACPI_PARSE_ENABLED
		ret = acpi_fixup();
		if (ret != 0) {
			panic("failed to parse/fix up ACPI table!");
		}
#endif

		if (!init_percpu_lapic_id()) {
			panic("failed to init_percpu_lapic_id!");
		}

		ret = init_ioapic_id_info();
		if (ret != 0) {
			panic("System IOAPIC info is incorrect!");
		}

#ifdef CONFIG_RDT_ENABLED
		init_rdt_info();
#endif

		/* NOTE: this must call after MMCONFIG is parsed in init_vboot and before APs are INIT.
		 * We only support platform with MMIO based CFG space access.
		 * IO port access only support in debug version.
		 */
		pci_switch_to_mmio_cfg_ops();
	} else {

		/* Switch this CPU to use the same page tables set-up by the
		 * primary/boot CPU
		 */
		enable_paging();

		early_init_lapic();

		pcpu_id = get_pcpu_id_from_lapic_id(get_cur_lapic_id());
		if (pcpu_id >= MAX_PCPU_NUM) {
			panic("Invalid pCPU ID!");
		}
	}

	bitmap_set_lock(pcpu_id, &pcpu_active_bitmap);

	/* Set state for this CPU to initializing */
	pcpu_set_current_state(pcpu_id, PCPU_STATE_INITIALIZING);
}

void init_pcpu_post(uint16_t pcpu_id)
{
#ifdef STACK_PROTECTOR
	set_fs_base();
#endif
	load_gdtr_and_tr();

	enable_ac_for_splitlock();

	init_pcpu_xsave();

	if (pcpu_id == BSP_CPU_ID) {
		/* Print Hypervisor Banner */
		print_hv_banner();

		/* Calibrate TSC Frequency */
		calibrate_tsc();

		pr_acrnlog("HV version %s-%s-%s %s (daily tag:%s) %s@%s build by %s%s, start time %luus",
				HV_FULL_VERSION,
				HV_BUILD_TIME, HV_BUILD_VERSION, HV_BUILD_TYPE,
				HV_DAILY_TAG, HV_BUILD_SCENARIO, HV_BUILD_BOARD,
				HV_BUILD_USER, HV_CONFIG_TOOL, ticks_to_us(start_tsc));

		pr_acrnlog("API version %u.%u",	HV_API_MAJOR_VERSION, HV_API_MINOR_VERSION);

		pr_acrnlog("Detect processor: %s", (get_pcpu_info())->model_name);

		pr_dbg("Core %hu is up", BSP_CPU_ID);

		/* Warn for security feature not ready */
		if (!check_cpu_security_cap()) {
			pr_fatal("SECURITY WARNING!!!!!!");
			pr_fatal("Please apply the latest CPU uCode patch!");
		}

		/* Initialize interrupts */
		init_interrupt(BSP_CPU_ID);

		timer_init();
		setup_notification();
		setup_pi_notification();

		if (init_iommu() != 0) {
			panic("failed to initialize iommu!");
		}

		hv_access_memory_region_update(get_mmcfg_base(), PCI_MMCONFIG_SIZE);
#ifdef CONFIG_IVSHMEM_ENABLED
		init_ivshmem_shared_memory();
#endif
		init_pci_pdev_list(); /* init_iommu must come before this */
		ptdev_init();

		if (init_sgx() != 0) {
			panic("failed to initialize sgx!");
		}

		/*
		 * Reserve memory from platform E820 for EPT 4K pages for all VMs
		 */
#ifdef CONFIG_LAST_LEVEL_EPT_AT_BOOT
		reserve_buffer_for_ept_pages();
#endif
		/* Start all secondary cores */
		startup_paddr = prepare_trampoline();
		if (!start_pcpus(AP_MASK)) {
			panic("Failed to start all secondary cores!");
		}

		ASSERT(get_pcpu_id() == BSP_CPU_ID, "");
	} else {
		pr_dbg("Core %hu is up", pcpu_id);

		pr_warn("Skipping VM configuration check which should be done before building HV binary.");

		/* Initialize secondary processor interrupts. */
		init_interrupt(pcpu_id);

		timer_init();
		ptdev_init();

		/* Wait for boot processor to signal all secondary cores to continue */
		wait_sync_change(&pcpu_sync, 0UL);
	}

	init_sched(pcpu_id);

#ifdef CONFIG_RDT_ENABLED
	setup_clos(pcpu_id);
#endif

	enable_smep();

	enable_smap();
}

static uint16_t get_pcpu_id_from_lapic_id(uint32_t lapic_id)
{
	uint16_t i;
	uint16_t pcpu_id = INVALID_CPU_ID;

	for (i = 0U; i < phys_cpu_num; i++) {
		if (per_cpu(lapic_id, i) == lapic_id) {
			pcpu_id = i;
			break;
		}
	}

	return pcpu_id;
}

static void start_pcpu(uint16_t pcpu_id)
{
	uint32_t timeout;

	/* Update the stack for pcpu */
	stac();
	write_trampoline_stack_sym(pcpu_id);
	clac();

	send_startup_ipi(pcpu_id, startup_paddr);

	/* Wait until the pcpu with pcpu_id is running and set the active bitmap or
	 * configured time-out has expired
	 */
	timeout = CPU_UP_TIMEOUT * 1000U;
	while (!is_pcpu_active(pcpu_id) && (timeout != 0U)) {
		/* Delay 10us */
		udelay(10U);

		/* Decrement timeout value */
		timeout -= 10U;
	}

	/* Check to see if expected CPU is actually up */
	if (!is_pcpu_active(pcpu_id)) {
		pr_fatal("Secondary CPU%hu failed to come up", pcpu_id);
		pcpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);
	}
}


/**
 * @brief Start all cpus if the bit is set in mask except itself
 *
 * @param[in] mask bits mask of cpus which should be started
 *
 * @return true if all cpus set in mask are started
 * @return false if there are any cpus set in mask aren't started
 */
bool start_pcpus(uint64_t mask)
{
	uint16_t i;
	uint16_t pcpu_id = get_pcpu_id();
	uint64_t expected_start_mask = mask;

	/* secondary cpu start up will wait for pcpu_sync -> 0UL */
	pcpu_sync = 1UL;
	cpu_write_memory_barrier();

	i = ffs64(expected_start_mask);
	while (i != INVALID_BIT_INDEX) {
		bitmap_clear_nolock(i, &expected_start_mask);

		if (pcpu_id == i) {
			continue; /* Avoid start itself */
		}

		start_pcpu(i);
		i = ffs64(expected_start_mask);
	}

	/* Trigger event to allow secondary CPUs to continue */
	pcpu_sync = 0UL;

	return ((pcpu_active_bitmap & mask) == mask);
}

void make_pcpu_offline(uint16_t pcpu_id)
{
	bitmap_set_lock(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id));
	if (get_pcpu_id() != pcpu_id) {
		send_single_ipi(pcpu_id, NOTIFY_VCPU_VECTOR);
	}
}

bool need_offline(uint16_t pcpu_id)
{
	return bitmap_test_and_clear_lock(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id));
}

void wait_pcpus_offline(uint64_t mask)
{
	uint32_t timeout;

	timeout = CPU_DOWN_TIMEOUT * 1000U;
	while (((pcpu_active_bitmap & mask) != 0UL) && (timeout != 0U)) {
		udelay(10U);
		timeout -= 10U;
	}
}

void stop_pcpus(void)
{
	uint16_t pcpu_id;
	uint64_t mask = 0UL;

	for (pcpu_id = 0U; pcpu_id < phys_cpu_num; pcpu_id++) {
		if (get_pcpu_id() == pcpu_id) {	/* avoid offline itself */
			continue;
		}

		bitmap_set_nolock(pcpu_id, &mask);
		make_pcpu_offline(pcpu_id);
	}

	/**
	 * Timeout never occurs here:
	 *   If target cpu received a NMI and panic, it has called cpu_dead and make_pcpu_offline success.
	 *   If target cpu is running, an IPI will be delivered to it and then call cpu_dead.
	 */
	wait_pcpus_offline(mask);
}

void cpu_do_idle(void)
{
	asm_pause();
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
	uint16_t pcpu_id = get_pcpu_id();

	deinit_sched(pcpu_id);
	if (bitmap_test(pcpu_id, &pcpu_active_bitmap)) {
		/* clean up native stuff */
		vmx_off();
		cache_flush_invalidate_all();

		/* Set state to show CPU is dead */
		pcpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);
		bitmap_clear_lock(pcpu_id, &pcpu_active_bitmap);

		/* Halt the CPU */
		do {
			asm_hlt();
		} while (halt != 0);
	} else {
		pr_err("pcpu%hu already dead", pcpu_id);
	}
}

static void set_current_pcpu_id(uint16_t pcpu_id)
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

static
inline void asm_monitor(volatile const uint64_t *addr, uint64_t ecx, uint64_t edx)
{
	asm volatile("monitor\n" : : "a" (addr), "c" (ecx), "d" (edx));
}

static
inline void asm_mwait(uint64_t eax, uint64_t ecx)
{
	asm volatile("mwait\n" : : "a" (eax), "c" (ecx));
}

/* wait until *sync == wake_sync */
void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync)
{
	if (has_monitor_cap()) {
		/* Wait for the event to be set using monitor/mwait */
		while ((*sync) != wake_sync) {
			asm_monitor(sync, 0UL, 0UL);
			if ((*sync) != wake_sync) {
				asm_mwait(0UL, 0UL);
			}
		}
	} else {
		while ((*sync) != wake_sync) {
			asm_pause();
		}
	}
}

static void init_pcpu_xsave(void)
{
	uint64_t val64;
	struct cpuinfo_x86 *cpu_info;
	uint64_t xcr0, xss;
	uint32_t eax, ecx, unused, xsave_area_size;

	CPU_CR_READ(cr4, &val64);
	val64 |= CR4_OSXSAVE;
	CPU_CR_WRITE(cr4, val64);

	if (get_pcpu_id() == BSP_CPU_ID) {
		cpuid_subleaf(CPUID_FEATURES, 0x0U, &unused, &unused, &ecx, &unused);

		/* if set, update it */
		if ((ecx & CPUID_ECX_OSXSAVE) != 0U) {
			cpu_info = get_pcpu_info();
			cpu_info->cpuid_leaves[FEAT_1_ECX] |= CPUID_ECX_OSXSAVE;

			/* set xcr0 and xss with the componets bitmap get from cpuid */
			xcr0 = ((uint64_t)cpu_info->cpuid_leaves[FEAT_D_0_EDX] << 32U)
				+ cpu_info->cpuid_leaves[FEAT_D_0_EAX];
			xss = ((uint64_t)cpu_info->cpuid_leaves[FEAT_D_1_EDX] << 32U)
				+ cpu_info->cpuid_leaves[FEAT_D_1_ECX];
			write_xcr(0, xcr0);
			msr_write(MSR_IA32_XSS, xss);

			/* get xsave area size, containing all the state components
			 * corresponding to bits currently set in XCR0 | IA32_XSS */
			cpuid_subleaf(CPUID_XSAVE_FEATURES, 1U,
				&eax,
				&xsave_area_size,
				&ecx,
				&unused);
			if (xsave_area_size > XSAVE_STATE_AREA_SIZE) {
				panic("XSAVE area (%d bytes) exceeds the pre-allocated 4K region\n",
						xsave_area_size);
			}
		}
	}
}


static void smpcall_write_msr_func(void *data)
{
	struct msr_data_struct *msr = (struct msr_data_struct *)data;

	msr_write(msr->msr_index, msr->write_val);
}

void msr_write_pcpu(uint32_t msr_index, uint64_t value64, uint16_t pcpu_id)
{
	struct msr_data_struct msr = {0};
	uint64_t mask = 0UL;

	if (pcpu_id == get_pcpu_id()) {
		msr_write(msr_index, value64);
	} else {
		msr.msr_index = msr_index;
		msr.write_val = value64;
		bitmap_set_nolock(pcpu_id, &mask);
		smp_call_function(mask, smpcall_write_msr_func, &msr);
	}
}

static void smpcall_read_msr_func(void *data)
{
	struct msr_data_struct *msr = (struct msr_data_struct *)data;

	msr->read_val = msr_read(msr->msr_index);
}

uint64_t msr_read_pcpu(uint32_t msr_index, uint16_t pcpu_id)
{
	struct msr_data_struct msr = {0};
	uint64_t mask = 0UL;
	uint64_t ret = 0;

	if (pcpu_id == get_pcpu_id()) {
		ret = msr_read(msr_index);
	} else {
		msr.msr_index = msr_index;
		bitmap_set_nolock(pcpu_id, &mask);
		smp_call_function(mask, smpcall_read_msr_func, &msr);
		ret = msr.read_val;
	}

	return ret;
}
