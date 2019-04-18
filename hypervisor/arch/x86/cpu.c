/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <atomic.h>
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
#include <vm.h>
#include <ld_sym.h>
#include <logmsg.h>
#include <cat.h>
#include <firmware.h>

#define CPU_UP_TIMEOUT		100U /* millisecond */
#define CPU_DOWN_TIMEOUT	100U /* millisecond */

#define AP_MASK			(((1UL << phys_cpu_num) - 1UL) & ~(1UL << 0U))

struct per_cpu_region per_cpu_data[CONFIG_MAX_PCPU_NUM] __aligned(PAGE_SIZE);
static uint16_t phys_cpu_num = 0U;
static uint64_t pcpu_sync = 0UL;
static uint16_t up_count = 0U;
static uint64_t startup_paddr = 0UL;

/* physical cpu active bitmap, support up to 64 cpus */
static uint64_t pcpu_active_bitmap = 0UL;

static void cpu_xsave_init(void);
static void set_current_cpu_id(uint16_t pcpu_id);
static void print_hv_banner(void);
static uint16_t get_cpu_id_from_lapic_id(uint32_t lapic_id);
static uint64_t start_tsc __attribute__((__section__(".bss_noinit")));

static void init_percpu_lapic_id(void)
{
	uint16_t i;
	uint16_t pcpu_num;
	uint32_t lapic_id_array[CONFIG_MAX_PCPU_NUM];

	/* Save all lapic_id detected via parse_mdt in lapic_id_array */
	pcpu_num = parse_madt(lapic_id_array);
	if (pcpu_num == 0U) {
		/* failed to get the physcial cpu number */
		panic("failed to get the physcial cpu number");
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

void init_cpu_pre(uint16_t pcpu_id_args)
{
	uint16_t pcpu_id = pcpu_id_args;
	int32_t ret;

	if (pcpu_id == BOOT_CPU_ID) {
		start_tsc = rdtsc();

		/* Clear BSS */
		(void)memset(&ld_bss_start, 0U, (size_t)(&ld_bss_end - &ld_bss_start));

		/* Get CPU capabilities thru CPUID, including the physical address bit
		 * limit which is required for initializing paging.
		 */
		init_cpu_capabilities();

		init_firmware_operations();

		init_cpu_model_name();

		load_cpu_state_data();

		/* Initialize the hypervisor paging */
		init_e820();
		init_paging();

		if (!cpu_has_cap(X86_FEATURE_X2APIC)) {
			panic("x2APIC is not present!");
		}

		early_init_lapic();

		init_percpu_lapic_id();

		ret = init_ioapic_id_info();
		if (ret != 0) {
			panic("System IOAPIC info is incorrect!");
		}

		ret = init_cat_cap_info();
		if (ret != 0) {
			panic("Platform CAT info is incorrect!");
		}

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

		pr_acrnlog("Detect processor: %s", (get_cpu_info())->model_name);

		pr_dbg("Core %hu is up", BOOT_CPU_ID);

		if (detect_hardware_support() != 0) {
			panic("hardware not support!");
		}

		if (!sanitize_vm_config()) {
			panic("VM Configuration Error!");
		}

		/* Warn for security feature not ready */
		if (!check_cpu_security_cap()) {
			pr_fatal("SECURITY WARNING!!!!!!");
			pr_fatal("Please apply the latest CPU uCode patch!");
		}

		init_scheduler();

		/* Initialize interrupts */
		interrupt_init(BOOT_CPU_ID);

		timer_init();
		setup_notification();
		setup_posted_intr_notification();
		init_pci_pdev_list();

		if (init_iommu() != 0) {
			panic("failed to initialize iommu!");
		}

		ptdev_init();

		/* Start all secondary cores */
		startup_paddr = prepare_trampoline();
		if (!start_cpus(AP_MASK)) {
			panic("Failed to start all secondary cores!");
		}

		ASSERT(get_cpu_id() == BOOT_CPU_ID, "");
	} else {
		pr_dbg("Core %hu is up", pcpu_id);

		/* Initialize secondary processor interrupts. */
		interrupt_init(pcpu_id);

		timer_init();

		/* Wait for boot processor to signal all secondary cores to continue */
		wait_sync_change(&pcpu_sync, 0UL);
	}

	setup_clos(pcpu_id);
}

static uint16_t get_cpu_id_from_lapic_id(uint32_t lapic_id)
{
	uint16_t i;
	uint16_t pcpu_id = INVALID_CPU_ID;

	for (i = 0U; (i < phys_cpu_num) && (i < CONFIG_MAX_PCPU_NUM); i++) {
		if (per_cpu(lapic_id, i) == lapic_id) {
			pcpu_id = i;
			break;
		}
	}

	return pcpu_id;
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
		cpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);
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
bool start_cpus(uint64_t mask)
{
	uint16_t i;
	uint16_t pcpu_id = get_cpu_id();
	uint64_t expected_start_mask = mask;

	/* secondary cpu start up will wait for pcpu_sync -> 0UL */
	atomic_store64(&pcpu_sync, 1UL);

	i = ffs64(expected_start_mask);
	while (i != INVALID_BIT_INDEX) {
		bitmap_clear_nolock(i, &expected_start_mask);

		if (pcpu_id == i) {
			continue; /* Avoid start itself */
		}

		start_cpu(i);
		i = ffs64(expected_start_mask);
	}

	/* Trigger event to allow secondary CPUs to continue */
	atomic_store64(&pcpu_sync, 0UL);

	return ((pcpu_active_bitmap & mask) == mask);
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

void stop_cpus(void)
{
	uint16_t pcpu_id, expected_up;
	uint32_t timeout;

	for (pcpu_id = 0U; pcpu_id < phys_cpu_num; pcpu_id++) {
		if (get_cpu_id() == pcpu_id) {	/* avoid offline itself */
			continue;
		}

		make_pcpu_offline(pcpu_id);
	}

	expected_up = 1U;
	timeout = CPU_DOWN_TIMEOUT * 1000U;
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
	uint16_t pcpu_id = get_cpu_id();

	if (bitmap_test(pcpu_id, &pcpu_active_bitmap)) {
		/* clean up native stuff */
		vmx_off();
		cache_flush_invalidate_all();

		/* Set state to show CPU is dead */
		cpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);
		bitmap_clear_nolock(pcpu_id, &pcpu_active_bitmap);

		/* Halt the CPU */
		do {
			asm_hlt();
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
	if (has_monitor_cap()) {
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

static void cpu_xsave_init(void)
{
	uint64_t val64;
	struct cpuinfo_x86 *cpu_info;

	if (cpu_has_cap(X86_FEATURE_XSAVE)) {
		CPU_CR_READ(cr4, &val64);
		val64 |= CR4_OSXSAVE;
		CPU_CR_WRITE(cr4, val64);

		if (get_cpu_id() == BOOT_CPU_ID) {
			uint32_t ecx, unused;
			cpuid(CPUID_FEATURES, &unused, &unused, &ecx, &unused);

			/* if set, update it */
			if ((ecx & CPUID_ECX_OSXSAVE) != 0U) {
				cpu_info = get_cpu_info();
				cpu_info->cpuid_leaves[FEAT_1_ECX] |= CPUID_ECX_OSXSAVE;
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

	if (pcpu_id == get_cpu_id()) {
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

	if (pcpu_id == get_cpu_id()) {
		ret = msr_read(msr_index);
	} else {
		msr.msr_index = msr_index;
		bitmap_set_nolock(pcpu_id, &mask);
		smp_call_function(mask, smpcall_read_msr_func, &msr);
		ret = msr.read_val;
	}

	return ret;
}
