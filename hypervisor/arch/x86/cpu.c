/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <bits.h>
#include <asm/page.h>
#include <asm/e820.h>
#include <asm/mmu.h>
#include <asm/guest/ept.h>
#include <asm/guest/vept.h>
#include <asm/vtd.h>
#include <asm/lapic.h>
#include <asm/irq.h>
#include <per_cpu.h>
#include <asm/cpufeatures.h>
#include <asm/cpu_caps.h>
#include <acpi.h>
#include <asm/ioapic.h>
#include <asm/trampoline.h>
#include <asm/cpuid.h>
#include <version.h>
#include <asm/vmx.h>
#include <asm/msr.h>
#include <asm/host_pm.h>
#include <ptdev.h>
#include <logmsg.h>
#include <asm/rdt.h>
#include <asm/sgx.h>
#include <uart16550.h>
#include <vpci.h>
#include <ivshmem.h>
#include <asm/rtcm.h>
#include <reloc.h>
#include <asm/tsc.h>
#include <ticks.h>
#include <delay.h>
#include <thermal.h>
#include <common/notify.h>
#include <cpu.h>

static uint16_t phys_cpu_num = 0U;
static uint64_t pcpu_sync = 0UL;
static uint64_t startup_paddr = 0UL;

static void init_pcpu_xsave(void);
static void init_keylocker(void);
static void print_hv_banner(void);
static uint16_t get_pcpu_id_from_lapic_id(uint32_t lapic_id);
static uint64_t start_tick __attribute__((__section__(".bss_noinit")));

/**
 * This function will be called by function get_pcpu_nums()
 * in common/cpu.c
 */
uint16_t arch_get_pcpu_num(void)
{
	return phys_cpu_num;
}

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
			per_cpu(arch.lapic_id, i) = lapic_id_array[i];
		}
		success = true;
	}

	return success;
}

static void enable_ac_for_splitlock(void)
{
#ifdef CONFIG_SPLIT_LOCK_DETECTION_ENABLED
	uint64_t test_ctl;

	if (has_core_cap(CORE_CAP_SPLIT_LOCK)) {
		test_ctl = msr_read(MSR_TEST_CTL);
		test_ctl |= MSR_TEST_CTL_AC_SPLITLOCK;
		msr_write(MSR_TEST_CTL, test_ctl);
	}
#endif /*CONFIG_SPLIT_LOCK_DETECTION_ENABLED*/
}

static void enable_gp_for_uclock(void)
{
#ifdef CONFIG_UC_LOCK_DETECTION_ENABLED
	uint64_t test_ctl;

	if (has_core_cap(CORE_CAP_UC_LOCK)) {
		test_ctl = msr_read(MSR_TEST_CTL);
		test_ctl |= MSR_TEST_CTL_GP_UCLOCK;
		msr_write(MSR_TEST_CTL, test_ctl);
	}
#endif /*CONFIG_UC_LOCK_DETECTION_ENABLED*/
}

static void init_pcpu_idle_and_kick(uint16_t pcpu_id)
{
	per_cpu(arch.idle_mode, pcpu_id) = IDLE_MODE_HLT;
	per_cpu(arch.kick_pcpu_mode, pcpu_id) = DEL_MODE_IPI;
}

void init_pcpu_pre(bool is_bsp)
{
	uint16_t pcpu_id;
	int32_t ret;

	if (is_bsp) {
		pcpu_id = BSP_CPU_ID;
		start_tick = cpu_ticks();

		/* Get CPU capabilities thru CPUID, including the physical address bit
		 * limit which is required for initializing paging.
		 */
		init_pcpu_capabilities();

		if (detect_hardware_support() != 0) {
			panic("hardware not support!");
		}

		init_pcpu_model_name();

		load_pcpu_state_data();

		init_frequency_policy();

		init_e820();

		/* reserve ppt buffer from e820 */
		allocate_ppt_pages();

		/* Initialize the hypervisor paging */
		init_paging();

		/*
		 * Need update uart_base_address here for vaddr2paddr mapping may changed
		 * WARNNING: DO NOT CALL PRINTF BETWEEN ENABLE PAGING IN init_paging AND HERE!
		 */
		uart16550_init(false);

		early_init_lapic();

		init_acpi();
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

#ifdef CONFIG_VCAT_ENABLED
		init_intercepted_cat_msr_list();
#endif

		/* NOTE: this must call after MMCONFIG is parsed in acpi_fixup() and before APs are INIT.
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

	set_pcpu_active(pcpu_id);

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
	enable_gp_for_uclock();

	init_pcpu_xsave();

#ifdef CONFIG_RETPOLINE
	disable_rrsba();
#endif

	if (pcpu_id == BSP_CPU_ID) {
		/* Print Hypervisor Banner */
		print_hv_banner();

		/* Initialie HPET */
		hpet_init();

		/* Calibrate TSC Frequency */
		calibrate_tsc();

		pr_acrnlog("HV: %s-%s-%s %s%s%s%s %s@%s build by %s, start time %luus",
				HV_BRANCH_VERSION, HV_COMMIT_TIME, HV_COMMIT_DIRTY, HV_BUILD_TYPE,
				(sizeof(HV_COMMIT_TAGS) > 1) ? "(tag: " : "", HV_COMMIT_TAGS,
				(sizeof(HV_COMMIT_TAGS) > 1) ? ")" : "", HV_BUILD_SCENARIO,
				HV_BUILD_BOARD, HV_BUILD_USER, ticks_to_us(start_tick));

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
		thermal_init();
		init_smp_call();

		if (init_iommu() != 0) {
			panic("failed to initialize iommu!");
		}

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
		reserve_buffer_for_ept_pages();

		init_vept();

		pcpu_sync = ALL_CPUS_MASK;
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
		thermal_init();
		ptdev_init();
	}

	if (!init_software_sram(pcpu_id == BSP_CPU_ID)) {
		panic("failed to initialize software SRAM!");
	}

	apply_frequency_policy();

	init_pcpu_idle_and_kick(pcpu_id);

	init_sched(pcpu_id);

#ifdef CONFIG_RDT_ENABLED
	setup_clos(pcpu_id);
#endif

	enable_smep();

	enable_smap();

	init_keylocker();

	bitmap_clear(pcpu_id, &pcpu_sync);
	/* Waiting for each pCPU has done its initialization before to continue */
	wait_sync_change(&pcpu_sync, 0UL);
}

static uint16_t get_pcpu_id_from_lapic_id(uint32_t lapic_id)
{
	uint16_t i;
	uint16_t pcpu_id = INVALID_CPU_ID;

	for (i = 0U; i < get_pcpu_nums(); i++) {
		if (per_cpu(arch.lapic_id, i) == lapic_id) {
			pcpu_id = i;
			break;
		}
	}

	return pcpu_id;
}

void arch_start_pcpu(uint16_t pcpu_id)
{
	/* Update the stack for pcpu */
	stac();
	write_trampoline_stack_sym(pcpu_id);
	clac();

	/* Using the MFENCE to make sure trampoline code
	 * has been updated (clflush) into memory beforing start APs.
	 */
	cpu_memory_barrier();
	send_startup_ipi(pcpu_id, startup_paddr);
}

void make_pcpu_offline(uint16_t pcpu_id)
{
	bitmap_set(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id));
	if (get_pcpu_id() != pcpu_id) {
		kick_pcpu(pcpu_id);
	}
}

bool need_offline(uint16_t pcpu_id)
{
	return bitmap_test_and_clear(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id));
}

void wait_pcpus_offline(uint64_t mask)
{
	uint32_t timeout;

	timeout = CPU_DOWN_TIMEOUT * 1000U;
	while (check_pcpus_inactive(mask) && (timeout != 0U)) {
		udelay(10U);
		timeout -= 10U;
	}
}

void stop_pcpus(void)
{
	uint16_t pcpu_id;
	uint64_t mask = 0UL;

	for (pcpu_id = 0U; pcpu_id < get_pcpu_nums(); pcpu_id++) {
		if (get_pcpu_id() == pcpu_id) {	/* avoid offline itself */
			continue;
		}

		bitmap_set_non_atomic(pcpu_id, &mask);
		make_pcpu_offline(pcpu_id);
	}

	/**
	 * Timeout never occurs here:
	 *   If target cpu received a NMI and panic, it has called cpu_dead and make_pcpu_offline success.
	 *   If target cpu is running, an IPI will be delivered to it and then call cpu_dead.
	 */
	wait_pcpus_offline(mask);
}

void arch_cpu_do_idle(void)
{
#ifdef CONFIG_KEEP_IRQ_DISABLED
	asm_pause();
#else
	uint16_t pcpu_id = get_pcpu_id();

	if (per_cpu(arch.idle_mode, pcpu_id) == IDLE_MODE_HLT) {
		asm_safe_hlt();
	} else {
		struct acrn_vcpu *vcpu = get_ever_run_vcpu(pcpu_id);

		if ((vcpu != NULL) && !is_lapic_pt_enabled(vcpu)) {
			local_irq_enable();
		}
		asm_pause();
		if ((vcpu != NULL) && !is_lapic_pt_enabled(vcpu)) {
			local_irq_disable();
		}
	}
#endif
}

/**
 * only run on current pcpu
 */
void arch_cpu_dead(void)
{
	/* For debug purposes, using a stack variable in the while loop enables
	 * us to modify the value using a JTAG probe and resume if needed.
	 */
	int32_t halt = 1;
	uint16_t pcpu_id = get_pcpu_id();

	if (is_pcpu_active(pcpu_id)) {
		/* clean up native stuff */
		vmx_off();

		stac();
		flush_cache_range((void *)get_hv_image_base(), get_hv_image_size());
		clac();

		/* Set state to show CPU is dead */
		pcpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);

		clear_pcpu_active(pcpu_id);

		/* Halt the CPU */
		do {
			asm_hlt();
		} while (halt != 0);
	} else {
		pr_err("pcpu%hu already dead", pcpu_id);
	}
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

	if (pcpu_has_cap(X86_FEATURE_XSAVE)) {
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
}

static void init_keylocker(void)
{
	uint64_t val64;

	/* Enable host CR4.KL if keylocker feature is supported */
	if (pcpu_has_cap(X86_FEATURE_KEYLOCKER)) {
		CPU_CR_READ(cr4, &val64);
		val64 |= CR4_KL;
		CPU_CR_WRITE(cr4, val64);
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
		bitmap_set_non_atomic(pcpu_id, &mask);
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
		bitmap_set_non_atomic(pcpu_id, &mask);
		smp_call_function(mask, smpcall_read_msr_func, &msr);
		ret = msr.read_val;
	}

	return ret;
}
