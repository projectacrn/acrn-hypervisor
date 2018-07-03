/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hypervisor.h>

struct run_context cpu_ctx;

/* whether the host enter s3 success */
uint8_t host_enter_s3_success = 1U;

void restore_msrs(void)
{
#ifdef STACK_PROTECTOR
	struct stack_canary *psc = &get_cpu_var(stack_canary);

	msr_write(MSR_IA32_FS_BASE, (uint64_t)psc);
#endif
}

static void acpi_gas_write(struct acpi_generic_address *gas, uint32_t val)
{
	if (gas->space_id == SPACE_SYSTEM_MEMORY)
		mmio_write_word(val, (void *)HPA2HVA(gas->address));
	else
		io_write_word(val, gas->address);
}

static uint32_t acpi_gas_read(struct acpi_generic_address *gas)
{
	uint32_t ret = 0;

	if (gas->space_id == SPACE_SYSTEM_MEMORY)
		ret = mmio_read_word((void *)HPA2HVA(gas->address));
	else
		ret = io_read_word(gas->address);

	return ret;
}

void do_acpi_s3(struct vm *vm, uint32_t pm1a_cnt_val,
	uint32_t pm1b_cnt_val)
{
	uint32_t s1, s2;

	acpi_gas_write(&vm->pm.sx_state_data->pm1a_cnt, pm1a_cnt_val);

	if (vm->pm.sx_state_data->pm1b_cnt.address != 0)
		acpi_gas_write(&vm->pm.sx_state_data->pm1b_cnt, pm1b_cnt_val);

	while (1) {
		/* polling PM1 state register to detect wether
		 * the Sx state enter is interrupted by wakeup event.
		 */
		s1 = s2 = 0;

		s1 = acpi_gas_read(&vm->pm.sx_state_data->pm1a_evt);

		if (vm->pm.sx_state_data->pm1b_evt.address != 0) {
			s2 = acpi_gas_read(&vm->pm.sx_state_data->pm1b_evt);
			s1 |= s2;
		}

		/* According to ACPI spec 4.8.3.1.1 PM1 state register, the bit
		 * WAK_STS(bit 15) is set if system will transition to working
		 * state.
		 */
		if ((s1 & (1 << BIT_WAK_STS)) != 0)
			break;
	}
}

int enter_s3(struct vm *vm, uint32_t pm1a_cnt_val,
	uint32_t pm1b_cnt_val)
{
	uint32_t pcpu_id;
	int ret;
	uint64_t pmain_entry_saved;
	uint32_t guest_wakeup_vec32;
	uint64_t *pmain_entry;

	/* We assume enter s3 success by default */
	host_enter_s3_success = 1;
	if (vm->pm.sx_state_data == NULL) {
		pr_err("No Sx state info avaiable. No Sx support");
		host_enter_s3_success = 0;
		return -1;
	}

	pause_vm(vm);	/* pause vm0 before suspend system */

	pcpu_id = get_cpu_id();

	/* Save the wakeup vec set by guest. Will return to guest
	 * with this wakeup vec as entry.
	 */
	guest_wakeup_vec32 = *vm->pm.sx_state_data->wake_vector_32;

	/* set ACRN wakeup vec instead */
	*vm->pm.sx_state_data->wake_vector_32 =
		(uint32_t) trampoline_start16_paddr;

	/* offline all APs */
	stop_cpus();

	/* Trampoline code is relocatable now. We have to calculate
	 * main_entry address with relocation base address
	 */
	pmain_entry =
		(uint64_t *)(HPA2HVA(trampoline_start16_paddr) +
		(uint64_t) main_entry);

	/* Save default main entry and we will restore it after
	 * back from S3. So the AP online could jmp to correct
	 * main entry.
	 */
	pmain_entry_saved = *pmain_entry;
	/* Set the main entry for resume from S3 state */
	*pmain_entry = (uint64_t)restore_s3_context;

	CPU_IRQ_DISABLE();
	vmx_off(pcpu_id);

	suspend_console();
	suspend_ioapic();
	suspend_iommu();
	suspend_lapic();

	__enter_s3(vm, pm1a_cnt_val, pm1b_cnt_val);

	/* release the lock aquired in trampoline code */
	spinlock_release(&trampoline_spinlock);

	resume_lapic();
	resume_iommu();
	resume_ioapic();
	resume_console();

	exec_vmxon_instr(pcpu_id);
	CPU_IRQ_ENABLE();

	/* restore the default main entry */
	*pmain_entry = pmain_entry_saved;

	/* online all APs again */
	start_cpus();

	/* jump back to vm */
	resume_vm_from_s3(vm, guest_wakeup_vec32);

	return 0;
}
