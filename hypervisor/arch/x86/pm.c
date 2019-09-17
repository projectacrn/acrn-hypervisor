/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hypervisor.h>
#include <trampoline.h>

struct cpu_context cpu_ctx;

/* The values in this structure should come from host ACPI table */
struct pm_s_state_data host_pm_s_state = {
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

/* whether the host enter s3 success */
uint8_t host_enter_s3_success = 1U;

void restore_msrs(void)
{
#ifdef STACK_PROTECTOR
	struct stack_canary *psc = &get_cpu_var(stk_canary);

	msr_write(MSR_IA32_FS_BASE, (uint64_t)psc);
#endif
}

static void acpi_gas_write(const struct acpi_generic_address *gas, uint32_t val)
{
	uint16_t val16 = (uint16_t)val;

	if (gas->space_id == SPACE_SYSTEM_MEMORY) {
		mmio_write16(val16, hpa2hva(gas->address));
	} else {
		pio_write16(val16, (uint16_t)gas->address);
	}
}

static uint32_t acpi_gas_read(const struct acpi_generic_address *gas)
{
	uint32_t ret = 0U;

	if (gas->space_id == SPACE_SYSTEM_MEMORY) {
		ret = mmio_read16(hpa2hva(gas->address));
	} else {
		ret = pio_read16((uint16_t)gas->address);
	}

	return ret;
}

void do_acpi_s3(struct acrn_vm *vm, uint32_t pm1a_cnt_val,
	uint32_t pm1b_cnt_val)
{
	uint32_t s1, s2;
	struct pm_s_state_data *sx_data = vm->pm.sx_state_data;

	acpi_gas_write(&(sx_data->pm1a_cnt), pm1a_cnt_val);

	if (vm->pm.sx_state_data->pm1b_cnt.address != 0U) {
		acpi_gas_write(&(sx_data->pm1b_cnt), pm1b_cnt_val);
	}

	do {
		/* polling PM1 state register to detect wether
		 * the Sx state enter is interrupted by wakeup event.
		 */
		s1 = 0U;
		s2 = 0U;

		s1 = acpi_gas_read(&(sx_data->pm1a_evt));

		if (vm->pm.sx_state_data->pm1b_evt.address != 0U) {
			s2 = acpi_gas_read(&(sx_data->pm1b_evt));
			s1 |= s2;
		}

		/* According to ACPI spec 4.8.3.1.1 PM1 state register, the bit
		 * WAK_STS(bit 15) is set if system will transition to working
		 * state.
		 */
	} while ((s1 & (1U << BIT_WAK_STS)) == 0U);
}

static void suspend_tsc(__unused void *data)
{
	per_cpu(tsc_suspend, get_cpu_id()) = rdtsc();
}

static void resume_tsc(__unused void *data)
{
	msr_write(MSR_IA32_TIME_STAMP_COUNTER, per_cpu(tsc_suspend, get_cpu_id()));
}

void enter_s3(struct acrn_vm *vm, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val)
{
	uint64_t pmain_entry_saved;
	uint32_t guest_wakeup_vec32;
	uint16_t pcpu_id;

	/* We assume enter s3 success by default */
	host_enter_s3_success = 1U;
	if (vm->pm.sx_state_data != NULL) {
		pause_vm(vm);	/* pause vm0 before suspend system */

		pcpu_id = get_cpu_id();

		stac();
		/* Save the wakeup vec set by guest. Will return to guest
		 * with this wakeup vec as entry.
		 */
		guest_wakeup_vec32 = *vm->pm.sx_state_data->wake_vector_32;

		/* set ACRN wakeup vec instead */
		*vm->pm.sx_state_data->wake_vector_32 =
			(uint32_t) trampoline_start16_paddr;

		clac();

		/* Save TSC on all PCPU */
		smp_call_function(pcpu_active_bitmap, suspend_tsc, NULL);

		/* offline all APs */
		stop_cpus();

		stac();
		/* Save default main entry and we will restore it after
		 * back from S3. So the AP online could jmp to correct
		 * main entry.
		 */
		pmain_entry_saved = read_trampoline_sym(main_entry);

		/* Set the main entry for resume from S3 state */
		write_trampoline_sym(main_entry, (uint64_t)restore_s3_context);
		clac();

		CPU_IRQ_DISABLE();
		vmx_off(pcpu_id);

		suspend_console();
		suspend_ioapic();
		suspend_iommu();
		suspend_lapic();

		asm_enter_s3(vm, pm1a_cnt_val, pm1b_cnt_val);

		resume_lapic();
		resume_iommu();
		resume_ioapic();

		exec_vmxon_instr(pcpu_id);
		CPU_IRQ_ENABLE();

		/* restore the default main entry */
		stac();
		write_trampoline_sym(main_entry, pmain_entry_saved);
		clac();

		/* online all APs again */
		start_cpus();

		/* Restore TSC on all PCPU
		 * Caution: There should no timer setup before TSC resumed.
		 */
		smp_call_function(pcpu_active_bitmap, resume_tsc, NULL);

		/* console must be resumed after TSC restored since it will setup timer base on TSC */
		resume_console();

		/* jump back to vm */
		resume_vm_from_s3(vm, guest_wakeup_vec32);
	} else {
		pr_err("No Sx state info avaiable. No Sx support");
		host_enter_s3_success = 0U;
	}


	return;
}
