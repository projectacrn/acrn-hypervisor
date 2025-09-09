/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <acrn_common.h>
#include <asm/default_acpi_info.h>
#include <platform_acpi_info.h>
#include <per_cpu.h>
#include <io.h>
#include <asm/msr.h>
#include <asm/pgtable.h>
#include <asm/host_pm.h>
#include <asm/trampoline.h>
#include <asm/vmx.h>
#include <console.h>
#include <asm/ioapic.h>
#include <asm/vtd.h>
#include <asm/lapic.h>
#include <asm/tsc.h>
#include <delay.h>
#include <asm/board.h>
#include <asm/cpuid.h>
#include <cpu.h>

struct cpu_context cpu_ctx;

/* The values in this structure should come from host ACPI table */
static struct pm_s_state_data host_pm_s_state = {
	.pm1a_evt = {
		.space_id = PM1A_EVT_SPACE_ID,
		.bit_width = PM1A_EVT_BIT_WIDTH,
		.bit_offset = PM1A_EVT_BIT_OFFSET,
		.access_size = PM1A_EVT_ACCESS_SIZE,
		.address = PM1A_EVT_ADDRESS
	},
	.pm1b_evt = {
		.space_id = PM1B_EVT_SPACE_ID,
		.bit_width = PM1B_EVT_BIT_WIDTH,
		.bit_offset = PM1B_EVT_BIT_OFFSET,
		.access_size = PM1B_EVT_ACCESS_SIZE,
		.address = PM1B_EVT_ADDRESS
	},
	.pm1a_cnt = {
		.space_id = PM1A_CNT_SPACE_ID,
		.bit_width = PM1A_CNT_BIT_WIDTH,
		.bit_offset = PM1A_CNT_BIT_OFFSET,
		.access_size = PM1A_CNT_ACCESS_SIZE,
		.address = PM1A_CNT_ADDRESS
	},
	.pm1b_cnt = {
		.space_id = PM1B_CNT_SPACE_ID,
		.bit_width = PM1B_CNT_BIT_WIDTH,
		.bit_offset = PM1B_CNT_BIT_OFFSET,
		.access_size = PM1B_CNT_ACCESS_SIZE,
		.address = PM1B_CNT_ADDRESS
	},
	.s3_pkg = {
		.val_pm1a = S3_PKG_VAL_PM1A,
		.val_pm1b = S3_PKG_VAL_PM1B,
		.reserved = S3_PKG_RESERVED
	},
	.s5_pkg = {
		.val_pm1a = S5_PKG_VAL_PM1A,
		.val_pm1b = S5_PKG_VAL_PM1B,
		.reserved = S5_PKG_RESERVED
	},
	.wake_vector_32 = (uint32_t *)WAKE_VECTOR_32,
	.wake_vector_64 = (uint64_t *)WAKE_VECTOR_64
};

/* host reset register defined in ACPI */
static struct acpi_reset_reg host_reset_reg = {
	.reg = {
		.space_id = RESET_REGISTER_SPACE_ID,
		.bit_width = RESET_REGISTER_BIT_WIDTH,
		.bit_offset = RESET_REGISTER_BIT_OFFSET,
		.access_size = RESET_REGISTER_ACCESS_SIZE,
		.address = RESET_REGISTER_ADDRESS,
	},
	.val = RESET_REGISTER_VALUE
};

struct pm_s_state_data *get_host_sstate_data(void)
{
	return &host_pm_s_state;
}

struct acpi_reset_reg *get_host_reset_reg_data(void)
{
	return &host_reset_reg;
}

void restore_msrs(void)
{
#ifdef STACK_PROTECTOR
	struct stack_canary *psc = &get_cpu_var(stk_canary);

	msr_write(MSR_IA32_FS_BASE, (uint64_t)psc);
#endif
}

static void acpi_gas_write(const struct acrn_acpi_generic_address *gas, uint32_t val)
{
	uint16_t val16 = (uint16_t)val;

	if (gas->space_id == SPACE_SYSTEM_MEMORY) {
		mmio_write16(val16, hpa2hva(gas->address));
	} else {
		pio_write16(val16, (uint16_t)gas->address);
	}
}

static uint32_t acpi_gas_read(const struct acrn_acpi_generic_address *gas)
{
	uint32_t ret = 0U;

	if (gas->space_id == SPACE_SYSTEM_MEMORY) {
		ret = mmio_read16(hpa2hva(gas->address));
	} else {
		ret = pio_read16((uint16_t)gas->address);
	}

	return ret;
}

/* This function supports enter S3 or S5 according to the value given to pm1a_cnt_val and pm1b_cnt_val */
void do_acpi_sx(const struct pm_s_state_data *sstate_data, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val)
{
	uint32_t s1, s2;

	acpi_gas_write(&(sstate_data->pm1a_cnt), pm1a_cnt_val);

	if (sstate_data->pm1b_cnt.address != 0U) {
		acpi_gas_write(&(sstate_data->pm1b_cnt), pm1b_cnt_val);
	}

	do {
		/* polling PM1 state register to detect wether
		 * the Sx state enter is interrupted by wakeup event.
		 */
		s1 = acpi_gas_read(&(sstate_data->pm1a_evt));

		if (sstate_data->pm1b_evt.address != 0U) {
			s2 = acpi_gas_read(&(sstate_data->pm1b_evt));
			s1 |= s2;
		}

		/* According to ACPI spec 4.8.3.1.1 PM1 state register, the bit
		 * WAK_STS(bit 15) is set if system will transition to working
		 * state.
		 */
	} while ((s1 & (1U << BIT_WAK_STS)) == 0U);
}

static uint32_t system_pm1a_cnt_val, system_pm1b_cnt_val;
void save_s5_reg_val(uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val)
{
	system_pm1a_cnt_val = pm1a_cnt_val;
	system_pm1b_cnt_val = pm1b_cnt_val;
}

void shutdown_system(void)
{
	struct pm_s_state_data *sx_data = get_host_sstate_data();
	do_acpi_sx(sx_data, system_pm1a_cnt_val, system_pm1b_cnt_val);
}

static void suspend_tsc(__unused void *data)
{
	per_cpu(arch.tsc_suspend, get_pcpu_id()) = rdtsc();
}

static void resume_tsc(__unused void *data)
{
	msr_write(MSR_IA32_TIME_STAMP_COUNTER, per_cpu(arch.tsc_suspend, get_pcpu_id()));
}

void host_enter_s3(const struct pm_s_state_data *sstate_data, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val)
{
	uint64_t pmain_entry_saved;

	stac();

	/* set ACRN wakeup vec instead */
	*(sstate_data->wake_vector_32) = (uint32_t)get_trampoline_start16_paddr();

	clac();

	/* Save TSC on all PCPU */
	smp_call_function(get_active_pcpu_bitmap(), suspend_tsc, NULL);

	/* offline all APs */
	stop_pcpus();

	stac();
	/* Save default main entry and we will restore it after
	 * back from S3. So the AP online could jmp to correct
	 * main entry.
	 */
	pmain_entry_saved = read_trampoline_sym(main_entry);

	/* Set the main entry for resume from S3 state */
	write_trampoline_sym(main_entry, (uint64_t)restore_s3_context);
	clac();

	local_irq_disable();
	vmx_off();

	suspend_console();
	suspend_vrtc();
	suspend_sched();
	suspend_ioapic();
	suspend_iommu();
	suspend_lapic();

	asm_enter_s3(sstate_data, pm1a_cnt_val, pm1b_cnt_val);

	resume_lapic();
	resume_iommu();
	resume_ioapic();
	init_frequency_policy();

	vmx_on();
	local_irq_enable();

	/* restore the default main entry */
	stac();
	write_trampoline_sym(main_entry, pmain_entry_saved);
	clac();

	/* online all APs again */
	if (!start_pcpus(AP_MASK)) {
		panic("Failed to start all APs!");
	}

	/* Restore TSC on all PCPU
	 * Caution: There should no timer setup before TSC resumed.
	 */
	smp_call_function(get_active_pcpu_bitmap(), resume_tsc, NULL);

	/* console must be resumed after TSC restored since it will setup timer base on TSC */
	resume_sched();
	resume_vrtc();
	resume_console();
}

void reset_host(bool warm)
{
	struct acrn_acpi_generic_address *gas = &(host_reset_reg.reg);
	uint8_t reboot_code = warm ? CF9_RESET_WARM : CF9_RESET_COLD;

	/* TODO: gracefully shut down all guests before doing host reset. */

	/*
	 * Assumption:
	 * The platform we are running must support at least one of reset method:
	 *   - ACPI reset
	 *   - 0xcf9 reset
	 */
	if ((gas->space_id == SPACE_SYSTEM_IO) &&
		(gas->bit_width == 8U) && (gas->bit_offset == 0U) &&
		(gas->address != 0U) && (gas->address != 0xcf9U)) {
		pio_write8(host_reset_reg.val, (uint16_t)host_reset_reg.reg.address);
	} else {
		/* making sure bit 2 (RST_CPU) is '0', when the reset command is issued. */
		pio_write8(0x2U, 0xcf9U);
		udelay(50U);
		pio_write8(reboot_code, 0xcf9U);
		udelay(50U);
	}

	pr_fatal("%s(): can't reset host.", __func__);
	while (1) {
		asm_pause();
	}
}

static enum acrn_cpufreq_policy_type cpufreq_policy = CPUFREQ_POLICY_PERFORMANCE;

void init_frequency_policy(void)
{
	uint32_t cpuid_06_eax, unused;
	struct acrn_boot_info *abi = get_acrn_boot_info();
	const char *cmd_src = abi->cmdline;

	/*
	 * Parse cmdline, decide which policy type to use.
	 * User can either specify cpu_perf_policy=Nominal or cpu_perf_policy=Performance
	 * The default type is 'Performance'
	 */
	if(strstr_s(cmd_src, MAX_BOOTARGS_SIZE, "cpu_perf_policy=Nominal", 24U) != NULL) {
		cpufreq_policy = CPUFREQ_POLICY_NOMINAL;
	}

	cpuid_subleaf(0x6U, 0U, &cpuid_06_eax, &unused, &unused, &unused);
	if ((cpuid_06_eax & CPUID_EAX_HWP) != 0U) {
		/* If HWP is available, enable HWP early. This will unlock other HWP MSRs. */
		msr_write(MSR_IA32_PM_ENABLE, 1U);
	}
}

/*
 * This Function is to be called by each pcpu after init_cpufreq().
 * It applies the frequency policy, which can be specified from boot parameters.
 *   - cpu_perf_policy=Performance: HWP autonomous selection, between highest HWP level and
 *     lowest HWP level. If HWP is not available, the frequency is fixed to highest p-state.
 *   - cpu_perf_policy=Nominal: frequency is fixed to guaranteed HWP level or nominal p-state.
 * The default policy is 'Performance'.
 *
 * ACRN will not be governing pcpu's frequency after this.
 */
void apply_frequency_policy(void)
{
	struct acrn_cpufreq_limits *limits = &cpufreq_limits[get_pcpu_id()];
	uint64_t highest_lvl_req = limits->highest_hwp_lvl, lowest_lvl_req = limits->lowest_hwp_lvl, reg;
	uint8_t pstate_req = limits->performance_pstate;
	uint32_t cpuid_06_eax, cpuid_01_ecx, unused;

	cpuid_subleaf(0x6U, 0U, &cpuid_06_eax, &unused, &unused, &unused);
	cpuid_subleaf(0x1U, 0U, &unused, &unused, &cpuid_01_ecx, &unused);
	/* Both HWP and ACPI p-state are supported. HWP is the first choice. */
	if ((cpuid_06_eax & CPUID_EAX_HWP) != 0U) {
		/*
		 * For Performance policy(default): CPU frequency will be autonomously selected between highest and lowest
		 * For Nominal policy: set to fixed frequency by letting highest=lowest=guaranteed
		 */
		if (cpufreq_policy == CPUFREQ_POLICY_NOMINAL) {
			highest_lvl_req = limits->guaranteed_hwp_lvl;
			lowest_lvl_req = limits->guaranteed_hwp_lvl;
		}
		/* EPP(0x80: default) | Desired_Performance(0: HWP auto) | Maximum_Performance | Minimum_Performance */
		reg = (0x80UL << 24U) | (0x00UL << 16U) | (highest_lvl_req << 8U) | lowest_lvl_req;
	    msr_write(MSR_IA32_HWP_REQUEST, reg);
	} else if ((cpuid_01_ecx & CPUID_ECX_EST) != 0U) {
		struct cpu_state_info *pm_s_state_data = get_cpu_pm_state_info();

		/*
		 * Set to fixed frequency in ACPI p-state mode.
		 * Performance policy: performance_pstate
		 * Nominal policy: nominal_pstate
		 */
		if (cpufreq_policy == CPUFREQ_POLICY_NOMINAL) {
			pstate_req = limits->nominal_pstate;
		}

		/* PX info might be missing on some platforms (px_cnt equals 0). Do nothing if so. */
		if (pm_s_state_data->px_cnt != 0) {
			if (pstate_req < pm_s_state_data->px_cnt) {
				msr_write(MSR_IA32_PERF_CTL, pm_s_state_data->px_data[pstate_req].control);
			} else {
				ASSERT(false, "invalid p-state index");
			}
		}
	} else {
		/* If no frequency interface is presented, just let CPU run by itself. Do nothing here.*/
	}
}
