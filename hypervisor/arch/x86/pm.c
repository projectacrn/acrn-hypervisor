/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <acrn_common.h>
#include <default_acpi_info.h>
#include <platform_acpi_info.h>
#include <per_cpu.h>
#include <io.h>
#include <pgtable.h>
#include <host_pm.h>
#include <trampoline.h>
#include <vmx.h>
#include <console.h>
#include <ioapic.h>
#include <vtd.h>
#include <lapic.h>
#include <vcpu.h>

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

/* This function supports enter S3 or S5 according to the value given to pm1a_cnt_val and pm1b_cnt_val */
void do_acpi_sx(struct pm_s_state_data *sstate_data, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val)
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

void host_enter_s3(struct pm_s_state_data *sstate_data, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val)
{
	uint64_t pmain_entry_saved;

	stac();

	/* set ACRN wakeup vec instead */
	*(sstate_data->wake_vector_32) = (uint32_t)get_trampoline_start16_paddr();

	clac();
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

	CPU_IRQ_DISABLE();
	vmx_off();

	suspend_console();
	suspend_ioapic();
	suspend_iommu();
	suspend_lapic();

	asm_enter_s3(sstate_data, pm1a_cnt_val, pm1b_cnt_val);

	resume_lapic();
	resume_iommu();
	resume_ioapic();
	resume_console();

	vmx_on();
	CPU_IRQ_ENABLE();

	/* restore the default main entry */
	stac();
	write_trampoline_sym(main_entry, pmain_entry_saved);
	clac();

	/* online all APs again */
	if (!start_pcpus(AP_MASK)) {
		panic("Failed to start all APs!");
	}
}

void reset_host(void)
{
	struct acpi_generic_address *gas = &(host_reset_reg.reg);


	/* TODO: gracefully shut down all guests before doing host reset. */

	/*
	 * Assumption:
	 * The platform we are running must support at least one of reset method:
	 *   - ACPI reset
	 *   - 0xcf9 reset
	 *
	 * UEFI more likely sets the reset value as 0x6 (not 0xe) for 0xcf9 port.
	 * This asserts PLTRST# to reset devices on the platform, but not the
	 * SLP_S3#/4#/5# signals, which power down the systems. This might not be
	 * enough for us.
	 */
	if ((gas->space_id == SPACE_SYSTEM_IO) &&
		(gas->bit_width == 8U) && (gas->bit_offset == 0U) &&
		(gas->address != 0U) && (gas->address != 0xcf9U)) {
		pio_write8(host_reset_reg.val, (uint16_t)host_reset_reg.reg.address);
	} else {
		/* making sure bit 2 (RST_CPU) is '0', when the reset command is issued. */
		pio_write8(0x2U, 0xcf9U);
		udelay(50U);
		pio_write8(0xeU, 0xcf9U);
	}

	pr_fatal("%s(): can't reset host.", __func__);
	while (1) {
		asm_pause();
	}
}
