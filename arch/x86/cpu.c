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

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <bsp_extern.h>
#include <hv_arch.h>
#include <schedule.h>
#include <version.h>
#include <hv_debug.h>

#ifdef CONFIG_EFI_STUB
extern uint32_t efi_physical_available_ap_bitmap;
#endif

uint64_t tsc_clock_freq = 1000000000;

spinlock_t cpu_secondary_spinlock = {
	.head = 0,
	.tail = 0
};

spinlock_t up_count_spinlock = {
	.head = 0,
	.tail = 0
};

void *per_cpu_data_base_ptr;
int phy_cpu_num;
unsigned long pcpu_sync = 0;
uint32_t up_count = 0;

DEFINE_CPU_DATA(uint8_t[STACK_SIZE], stack) __aligned(16);
DEFINE_CPU_DATA(uint8_t, lapic_id);
DEFINE_CPU_DATA(void *, vcpu);
DEFINE_CPU_DATA(int, state);

/* TODO: add more capability per requirement */
/*Define APICv features*/
#define APICV_FEATURE_VAPIC			(1 << 0)
#define APICV_FEATURE_VAPIC_REGS		(1 << 1)
#define APICV_FEATURE_VIRQ			(1 << 2)
#define APICV_FEATURE_USE_TPR_SHADOW		(1 << 3)
#define APICV_FEATURE_POST_IRQ			(1 << 4)

struct cpu_capability {
	bool tsc_adjust_supported;
	bool ibrs_ibpb_supported;
	bool stibp_supported;
	uint8_t apicv_features;
	bool monitor_supported;
};
static struct cpu_capability cpu_caps;

static void apicv_cap_detect(void);
static void monitor_cap_detect(void);
static void cpu_set_logical_id(uint32_t logical_id);
static void print_hv_banner(void);
static inline bool get_monitor_cap(void);
int cpu_find_logical_id(uint32_t lapic_id);
#ifndef CONFIG_EFI_STUB
static void start_cpus();
#endif
static void pcpu_sync_sleep(unsigned long *sync, int mask_bit);
int ibrs_type;
static void check_cpu_capability(void)
{
	uint32_t  eax, ebx, ecx, edx;

	memset(&cpu_caps, 0, sizeof(struct cpu_capability));

	cpuid(CPUID_EXTEND_FEATURE, &eax, &ebx, &ecx, &edx);

	cpu_caps.tsc_adjust_supported = (ebx & CPUID_EBX_TSC_ADJ) ?
							(true) : (false);
	cpu_caps.ibrs_ibpb_supported = (edx & CPUID_EDX_IBRS_IBPB) ?
							(true) : (false);
	cpu_caps.stibp_supported = (edx & CPUID_EDX_STIBP) ?
							(true) : (false);

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
	if (cpu_caps.ibrs_ibpb_supported) {
		ibrs_type = IBRS_RAW;
		if (cpu_caps.stibp_supported)
			ibrs_type = IBRS_OPT;
	}
#endif
}

bool check_tsc_adjust_support(void)
{
	return cpu_caps.tsc_adjust_supported;
}

bool check_ibrs_ibpb_support(void)
{
	return cpu_caps.ibrs_ibpb_supported;
}

bool check_stibp_support(void)
{
	return cpu_caps.stibp_supported;
}

static void alloc_phy_cpu_data(int pcpu_num)
{
	phy_cpu_num = pcpu_num;

	per_cpu_data_base_ptr = calloc(1, PER_CPU_DATA_SIZE * pcpu_num);
	ASSERT(per_cpu_data_base_ptr != NULL, "");
}

int __attribute__((weak)) parse_madt(uint8_t *lapic_id_base)
{
	static const uint32_t lapic_id[] = {0, 2, 4, 6};
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(lapic_id); i++)
		*lapic_id_base++ = lapic_id[i];

	return ARRAY_SIZE(lapic_id);
}

static int init_phy_cpu_storage(void)
{
	int i, pcpu_num = 0;
	int bsp_cpu_id;
	uint8_t bsp_lapic_id = 0;
	uint8_t *lapic_id_base;

	/*
	 * allocate memory to save all lapic_id detected in parse_mdt.
	 * We allocate 4K size which could save 4K CPUs lapic_id info.
	 */
	lapic_id_base = alloc_page(CPU_PAGE_SIZE);
	ASSERT(lapic_id_base != NULL, "fail to alloc page");

	pcpu_num = parse_madt(lapic_id_base);
	alloc_phy_cpu_data(pcpu_num);

	for (i = 0; i < pcpu_num; i++) {
		per_cpu(lapic_id, i) = *lapic_id_base++;
#ifdef CONFIG_EFI_STUB
		efi_physical_available_ap_bitmap |= 1 << per_cpu(lapic_id, i);
#endif
	}

	/* free memory after lapic_id are saved in per_cpu data */
	free(lapic_id_base);

	bsp_lapic_id = get_cur_lapic_id();

#ifdef CONFIG_EFI_STUB
	efi_physical_available_ap_bitmap &= ~(1 << bsp_lapic_id);
#endif

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

	/* Set state for the specified CPU */
	per_cpu(state, logical_id) = state;

	spinlock_release(&up_count_spinlock);
}

#ifdef STACK_PROTECTOR
struct stack_canary {
	/* Gcc generates extra code, using [fs:40] to access canary */
	uint8_t reserved[40];
	uint64_t canary;
};

static DEFINE_CPU_DATA(struct stack_canary, stack_canary);

static uint64_t get_random_value(void)
{
	uint64_t random = 0;

	asm volatile ("1: rdrand %%rax\n"
			"jnc 1b\n"
			"mov %%rax, %0\n"
			: "=r"(random) :: );
	return random;
}

static void set_fs_base(void)
{
	struct stack_canary *psc = &get_cpu_var(stack_canary);

	psc->canary = get_random_value();
	msr_write(MSR_IA32_FS_BASE, (uint64_t)psc);
}
#endif

void bsp_boot_init(void)
{
#ifdef HV_DEBUG
	uint64_t start_tsc = rdtsc();
#endif

	/* Clear BSS */
	memset(_ld_bss_start, 0, _ld_bss_end - _ld_bss_start);

	/* Build time sanity checks to make sure hard-coded offset
	*  is matching the actual offset!
	*/
	STATIC_ASSERT(offsetof(struct cpu_regs, rax) ==
		VMX_MACHINE_T_GUEST_RAX_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, rbx) ==
		VMX_MACHINE_T_GUEST_RBX_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, rcx) ==
		VMX_MACHINE_T_GUEST_RCX_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, rdx) ==
		VMX_MACHINE_T_GUEST_RDX_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, rbp) ==
		VMX_MACHINE_T_GUEST_RBP_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, rsi) ==
		VMX_MACHINE_T_GUEST_RSI_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, rdi) ==
		VMX_MACHINE_T_GUEST_RDI_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, r8) ==
		VMX_MACHINE_T_GUEST_R8_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, r9) ==
		VMX_MACHINE_T_GUEST_R9_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, r10) ==
		VMX_MACHINE_T_GUEST_R10_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, r11) ==
		VMX_MACHINE_T_GUEST_R11_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, r12) ==
		VMX_MACHINE_T_GUEST_R12_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, r13) ==
		VMX_MACHINE_T_GUEST_R13_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, r14) ==
		VMX_MACHINE_T_GUEST_R14_OFFSET);
	STATIC_ASSERT(offsetof(struct cpu_regs, r15) ==
		VMX_MACHINE_T_GUEST_R15_OFFSET);
	STATIC_ASSERT(offsetof(struct run_context, cr2) ==
		VMX_MACHINE_T_GUEST_CR2_OFFSET);
	STATIC_ASSERT(offsetof(struct run_context, ia32_spec_ctrl) ==
		VMX_MACHINE_T_GUEST_SPEC_CTRL_OFFSET);

	/* Initialize the hypervisor paging */
	init_paging();

	early_init_lapic();

	init_phy_cpu_storage();

	load_gdtr_and_tr();

	/* Switch to run-time stack */
	CPU_SP_WRITE(&get_cpu_var(stack)[STACK_SIZE - 1]);

#ifdef STACK_PROTECTOR
	set_fs_base();
#endif

	check_cpu_capability();

	apicv_cap_detect();

	monitor_cap_detect();

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

	/* Calculate TSC Frequency */
	tsc_clock_freq = tsc_cycles_in_period(1000) / 1000 * 1000000;

	/* Enable logging */
	init_logmsg(LOG_BUF_SIZE,
		       LOG_DESTINATION);

#ifdef HV_DEBUG
	/* Log first messages */
	if (HV_RC_VERSION)
		printf("HV version %d.%d-rc%d-%s-%s build by %s, start time %lluus\r\n",
			HV_MAJOR_VERSION, HV_MINOR_VERSION, HV_RC_VERSION,
			HV_BUILD_TIME, HV_BUILD_VERSION, HV_BUILD_USER,
			TICKS_TO_US(start_tsc));
	else
		printf("HV version %d.%d-%s-%s build by %s, start time %lluus\r\n",
			HV_MAJOR_VERSION, HV_MINOR_VERSION,
			HV_BUILD_TIME, HV_BUILD_VERSION, HV_BUILD_USER,
			TICKS_TO_US(start_tsc));

	printf("API version %d.%d\r\n",
			HV_API_MAJOR_VERSION, HV_API_MINOR_VERSION);
#endif
	pr_dbg("Core %d is up", CPU_BOOT_ID);

	/* Warn for security feature not ready */
	if (!check_ibrs_ibpb_support() && !check_stibp_support()) {
		pr_fatal("SECURITY WARNING!!!!!!");
		pr_fatal("Please apply the latest CPU uCode patch!");
	}

	/* Initialize the shell */
	shell_init();

	/* Initialize interrupts */
	interrupt_init(CPU_BOOT_ID);

	timer_init();
	setup_notification();
	ptdev_init();

	init_scheduler();

#ifndef CONFIG_EFI_STUB
	/* Start all secondary cores */
	start_cpus();

	/* Trigger event to allow secondary CPUs to continue */
	bitmap_set(0, &pcpu_sync);
#else
	memcpy_s(_ld_cpu_secondary_reset_start,
		(unsigned long)&_ld_cpu_secondary_reset_size,
		_ld_cpu_secondary_reset_load,
		(unsigned long)&_ld_cpu_secondary_reset_size);
#endif

	ASSERT(get_cpu_id() == CPU_BOOT_ID, "");

	init_iommu();

	console_setup_timer();

	/* Start initializing the VM for this CPU */
	hv_main(CPU_BOOT_ID);

	/* Control should not come here */
	cpu_halt(CPU_BOOT_ID);
}

void cpu_secondary_init(void)
{
	/* NOTE: Use of local / stack variables in this function is problematic
	 * since the stack is switched in the middle of the function.  For this
	 * reason, the logical id is only temporarily stored in a static
	 * variable, but this will be over-written once subsequent CPUs
	 * start-up.  Once the spin-lock is released, the cpu_logical_id_get()
	 * API is used to obtain the logical ID
	 */

	/* Switch this CPU to use the same page tables set-up by the
	 * primary/boot CPU
	 */
	enable_paging(get_paging_pml4());
	early_init_lapic();

	/* Find the logical ID of this CPU given the LAPIC ID
	 * temp_logical_id =
	 * cpu_find_logical_id(get_cur_lapic_id());
	 */
	cpu_find_logical_id(get_cur_lapic_id());

	/* Set state for this CPU to initializing */
	cpu_set_current_state(cpu_find_logical_id
			      (get_cur_lapic_id()),
			      CPU_STATE_INITIALIZING);

	/* Switch to run-time stack */
	CPU_SP_WRITE(&get_cpu_var(stack)[STACK_SIZE - 1]);

#ifdef STACK_PROTECTOR
	set_fs_base();
#endif

	load_gdtr_and_tr();

	/* Make sure rdtsc is enabled */
	check_tsc();

	pr_dbg("Core %d is up", get_cpu_id());

	/* Release secondary boot spin-lock to allow one of the next CPU(s) to
	 * perform this common initialization
	 */
	spinlock_release(&cpu_secondary_spinlock);

	/* Initialize secondary processor interrupts. */
	interrupt_init(get_cpu_id());

	timer_init();

	/* Wait for boot processor to signal all secondary cores to continue */
	pcpu_sync_sleep(&pcpu_sync, 0);

#ifdef CONFIG_EFI_STUB
	bitmap_clr(0, &pcpu_sync);
#endif

	hv_main(get_cpu_id());

	/* Control will only come here for secondary CPUs not configured for
	 * use or if an error occurs in hv_main
	 */
	cpu_halt(get_cpu_id());
}

int cpu_find_logical_id(uint32_t lapic_id)
{
	int i;

	for (i = 0; i < phy_cpu_num; i++) {
		if (per_cpu(lapic_id, i) == lapic_id)
			return i;
	}

	return -1;
}

#ifndef CONFIG_EFI_STUB
/*
 * Start all secondary CPUs.
 */
static void start_cpus()
{
	uint32_t timeout;
	uint32_t expected_up;

	/*Copy segment for AP initialization code below 1MB */
	memcpy_s(_ld_cpu_secondary_reset_start,
		(unsigned long)&_ld_cpu_secondary_reset_size,
		_ld_cpu_secondary_reset_load,
		(unsigned long)&_ld_cpu_secondary_reset_size);

	/* Set flag showing number of CPUs expected to be up to all
	 * cpus
	 */
	expected_up = phy_cpu_num;

	/* Broadcast IPIs to all other CPUs */
	send_startup_ipi(INTR_CPU_STARTUP_ALL_EX_SELF,
		       -1U, ((paddr_t) cpu_secondary_reset));

	/* Wait until global count is equal to expected CPU up count or
	 * configured time-out has expired
	 */
	timeout = CPU_UP_TIMEOUT * 1000;
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
#endif

void cpu_halt(uint32_t logical_id)
{
	/* For debug purposes, using a stack variable in the while loop enables
	 * us to modify the value using a JTAG probe and resume if needed.
	 */
	int halt = 1;

	/* Set state to show CPU is halted */
	cpu_set_current_state(logical_id, CPU_STATE_HALTED);

	/* Halt the CPU */
	do {
		asm volatile ("hlt");
	} while (halt);
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
	int wake_sync = (1 << mask_bit);

	if (get_monitor_cap()) {
		/* Wait for the event to be set using monitor/mwait */
		asm volatile ("1: cmpl      %%ebx,(%%eax)\n"
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
		asm volatile ("1: cmpl      %%ebx,(%%eax)\n"
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

static void apicv_cap_detect(void)
{
	uint8_t features;
	uint64_t msr_val;

	features = 0;

	msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS2);
	if (!is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VAPIC)) {
		cpu_caps.apicv_features = 0;
		return;
	}

	features |= APICV_FEATURE_VAPIC;

	if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VAPIC_REGS))
		features |= APICV_FEATURE_VAPIC_REGS;

	if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS2_VIRQ)) {
		features |= APICV_FEATURE_VIRQ;

		msr_val = msr_read(MSR_IA32_VMX_PINBASED_CTLS);
		if (is_ctrl_setting_allowed(msr_val,
						VMX_PINBASED_CTLS_POST_IRQ))
			features |= APICV_FEATURE_POST_IRQ;
	}

	msr_val = msr_read(MSR_IA32_VMX_PROCBASED_CTLS);
	if (is_ctrl_setting_allowed(msr_val, VMX_PROCBASED_CTLS_TPR_SHADOW))
		features |= APICV_FEATURE_USE_TPR_SHADOW;

	cpu_caps.apicv_features = features;
}

bool is_apicv_enabled(void)
{
	uint8_t features;

	features = (APICV_FEATURE_VAPIC | APICV_FEATURE_USE_TPR_SHADOW);
	return ((cpu_caps.apicv_features & features) == features);
}

bool is_apicv_virq_enabled(void)
{
	if (!is_apicv_enabled())
		return false;

	return ((cpu_caps.apicv_features & APICV_FEATURE_VIRQ) != 0);
}

bool is_apicv_vapic_regs_enabled(void)
{
	if (!is_apicv_enabled())
		return false;

	return ((cpu_caps.apicv_features & APICV_FEATURE_VAPIC_REGS) != 0);
}

static void monitor_cap_detect(void)
{
	uint32_t  eax, ebx, ecx, edx;
	uint32_t family;
	uint32_t model;

	/* Run CPUID to determine if MONITOR support available */
	cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);

	/* See if MONITOR feature bit is set in ECX */
	if (ecx & CPUID_ECX_MONITOR)
		cpu_caps.monitor_supported = true;

	/* don't use monitor for CPU (family: 0x6 model: 0x5c)
	 * in hypervisor, but still expose it to the guests and
	 * let them handle it correctly
	 */
	family = (eax >> 8) & 0xff;
	if (family == 0xF)
        family += (eax >> 20) & 0xff;

	model = (eax >> 4) & 0xf;
	if (family >= 0x06)
		model += ((eax >> 16) & 0xf) << 4;

	if (cpu_caps.monitor_supported &&
		(family == 0x06) &&
		(model == 0x5c)) {
		cpu_caps.monitor_supported = false;
	}
}

static inline bool get_monitor_cap(void)
{
	return cpu_caps.monitor_supported;
}
