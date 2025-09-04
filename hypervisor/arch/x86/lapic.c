/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/lib/bits.h>
#include <asm/msr.h>
#include <asm/cpu.h>
#include <asm/per_cpu.h>
#include <asm/cpu_caps.h>
#include <asm/lapic.h>
#include <asm/apicreg.h>
#include <asm/irq.h>
#include <delay.h>

/* intr_lapic_icr_delivery_mode */
#define INTR_LAPIC_ICR_FIXED           0x0U
#define INTR_LAPIC_ICR_LP              0x1U
#define INTR_LAPIC_ICR_SMI             0x2U
#define INTR_LAPIC_ICR_NMI             0x4U
#define INTR_LAPIC_ICR_INIT            0x5U
#define INTR_LAPIC_ICR_STARTUP         0x6U

/* intr_lapic_icr_dest_mode */
#define INTR_LAPIC_ICR_PHYSICAL        0x0U
#define INTR_LAPIC_ICR_LOGICAL         0x1U

/* intr_lapic_icr_level */
#define INTR_LAPIC_ICR_DEASSERT        0x0U
#define INTR_LAPIC_ICR_ASSERT          0x1U

/* intr_lapic_icr_trigger */
#define INTR_LAPIC_ICR_EDGE            0x0U
#define INTR_LAPIC_ICR_LEVEL           0x1U

/* intr_lapic_icr_shorthand */
#define INTR_LAPIC_ICR_USE_DEST_ARRAY  0x0U
#define INTR_LAPIC_ICR_SELF            0x1U
#define INTR_LAPIC_ICR_ALL_INC_SELF    0x2U
#define INTR_LAPIC_ICR_ALL_EX_SELF     0x3U

union lapic_base_msr {
	uint64_t value;
	struct {
		uint32_t rsvd_1:8;
		uint32_t bsp:1;
		uint32_t rsvd_2:1;
		uint32_t x2APIC_enable:1;
		uint32_t xAPIC_enable:1;
		uint32_t lapic_paddr:24;
		uint32_t rsvd_3:28;
	} fields;
};

static struct lapic_regs saved_lapic_regs;
static union lapic_base_msr saved_lapic_base_msr;

static void clear_lapic_isr(void)
{
	uint32_t i;
	uint32_t isr_reg;

	/* This is a Intel recommended procedure and assures that the processor
	 * does not get hung up due to already set "in-service" interrupts left
	 * over from the boot loader environment. This actually occurs in real
	 * life, therefore we will ensure all the in-service bits are clear.
	 */
	for (isr_reg = MSR_IA32_EXT_APIC_ISR7; isr_reg >= MSR_IA32_EXT_APIC_ISR0; isr_reg--) {
		for (i = 0U; i < 32U; i++) {
			if (msr_read(isr_reg) != 0U) {
				msr_write(MSR_IA32_EXT_APIC_EOI, 0U);
			} else {
				break;
			}
		}
	}
}

void early_init_lapic(void)
{
	union lapic_base_msr base;

	/* Get local APIC base address */
	base.value = msr_read(MSR_IA32_APIC_BASE);

	/* Enable LAPIC in x2APIC mode*/
	/* The following sequence of msr writes to enable x2APIC
	 * will work irrespective of the state of LAPIC
	 * left by BIOS
	 */
	/* Step1: Enable LAPIC in xAPIC mode */
	base.fields.xAPIC_enable = 1U;
	msr_write(MSR_IA32_APIC_BASE, base.value);
	/* Step2: Enable LAPIC in x2APIC mode */
	base.fields.x2APIC_enable = 1U;
	msr_write(MSR_IA32_APIC_BASE, base.value);

	/* Set the mask bits for all the LVT entries by disabling a local APIC software. */
	msr_write(MSR_IA32_EXT_APIC_SIVR, 0UL);

	/* Enable Local APIC */
	/* TODO: add spurious-interrupt handler */
	msr_write(MSR_IA32_EXT_APIC_SIVR, APIC_SVR_ENABLE | APIC_SVR_VECTOR);

	/* Ensure there are no ISR bits set. */
	clear_lapic_isr();
}

void init_lapic(uint16_t pcpu_id)
{
	/* Can not put this to early_init_lapic because logical ID is not
	 * updated yet.
	 */
	per_cpu(lapic_ldr, pcpu_id) = (uint32_t) msr_read(MSR_IA32_EXT_APIC_LDR);
}

static void save_lapic(struct lapic_regs *regs)
{
	regs->tpr.v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_TPR);
	regs->ppr.v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_PPR);
	regs->tmr[0].v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_TMR0);
	regs->tmr[1].v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_TMR1);
	regs->tmr[2].v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_TMR2);
	regs->tmr[3].v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_TMR3);
	regs->tmr[4].v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_TMR4);
	regs->tmr[5].v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_TMR5);
	regs->tmr[6].v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_TMR6);
	regs->tmr[7].v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_TMR7);
	regs->svr.v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_SIVR);
	regs->lvt[APIC_LVT_TIMER].v =
		(uint32_t) msr_read(MSR_IA32_EXT_APIC_LVT_TIMER);
	regs->lvt[APIC_LVT_LINT0].v =
		(uint32_t) msr_read(MSR_IA32_EXT_APIC_LVT_LINT0);
	regs->lvt[APIC_LVT_LINT1].v =
		(uint32_t) msr_read(MSR_IA32_EXT_APIC_LVT_LINT1);
	regs->lvt[APIC_LVT_ERROR].v =
		(uint32_t) msr_read(MSR_IA32_EXT_APIC_LVT_ERROR);
	regs->icr_timer.v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_INIT_COUNT);
	regs->ccr_timer.v = (uint32_t) msr_read(MSR_IA32_EXT_APIC_CUR_COUNT);
	regs->dcr_timer.v =
		(uint32_t) msr_read(MSR_IA32_EXT_APIC_DIV_CONF);
}

static void restore_lapic(const struct lapic_regs *regs)
{
	msr_write(MSR_IA32_EXT_APIC_TPR, (uint64_t) regs->tpr.v);
	msr_write(MSR_IA32_EXT_APIC_SIVR, (uint64_t) regs->svr.v);
	msr_write(MSR_IA32_EXT_APIC_LVT_TIMER,
			(uint64_t) regs->lvt[APIC_LVT_TIMER].v);

	msr_write(MSR_IA32_EXT_APIC_LVT_LINT0,
			(uint64_t) regs->lvt[APIC_LVT_LINT0].v);
	msr_write(MSR_IA32_EXT_APIC_LVT_LINT1,
			(uint64_t) regs->lvt[APIC_LVT_LINT1].v);

	msr_write(MSR_IA32_EXT_APIC_LVT_ERROR,
			(uint64_t) regs->lvt[APIC_LVT_ERROR].v);
	msr_write(MSR_IA32_EXT_APIC_INIT_COUNT, (uint64_t) regs->icr_timer.v);
	msr_write(MSR_IA32_EXT_APIC_DIV_CONF, (uint64_t) regs->dcr_timer.v);
}

void suspend_lapic(void)
{
	uint64_t val;

	saved_lapic_base_msr.value = msr_read(MSR_IA32_APIC_BASE);
	save_lapic(&saved_lapic_regs);

	/* disable APIC with software flag */
	val = msr_read(MSR_IA32_EXT_APIC_SIVR);
	val = (~(uint64_t)APIC_SVR_ENABLE) & val;
	msr_write(MSR_IA32_EXT_APIC_SIVR, val);
}

void resume_lapic(void)
{
	msr_write(MSR_IA32_APIC_BASE, saved_lapic_base_msr.value);

	/* ACPI software flag will be restored also */
	restore_lapic(&saved_lapic_regs);
}

void send_lapic_eoi(void)
{
	msr_write(MSR_IA32_EXT_APIC_EOI, 0U);
}

uint32_t get_cur_lapic_id(void)
{
	uint32_t lapic_id;

	lapic_id = (uint32_t) msr_read(MSR_IA32_EXT_XAPICID);

	return lapic_id;
}

void
send_startup_ipi(uint16_t dest_pcpu_id, uint64_t cpu_startup_start_address)
{
	union apic_icr icr;
	struct cpuinfo_x86 *cpu_info = get_pcpu_info();

	icr.value = 0U;
	icr.value_32.hi_32 = per_cpu(lapic_id, dest_pcpu_id);

	/* Assert INIT IPI */
	icr.bits.destination_mode = INTR_LAPIC_ICR_PHYSICAL;
	icr.bits.shorthand = INTR_LAPIC_ICR_USE_DEST_ARRAY;
	icr.bits.delivery_mode = INTR_LAPIC_ICR_INIT;
	msr_write(MSR_IA32_EXT_APIC_ICR, icr.value);

	/* Give 10ms for INIT sequence to complete for old processors.
	 * BWG states that a delay cannot be avoided between the INIT IPI
	 * and first Startup IPI, so on Modern processors (family == 6)
	 * setting a delay value of 10us.
	 */
	if (cpu_info->displayfamily != 6U) {
		/* delay 10ms */
		udelay(10000U);
	} else {
		udelay(10U); /* 10us is enough for Modern processors */
	}

	/* Send Start IPI with page number of secondary reset code */
	icr.value_32.lo_32 = 0U;
	icr.bits.shorthand = INTR_LAPIC_ICR_USE_DEST_ARRAY;
	icr.bits.delivery_mode = INTR_LAPIC_ICR_STARTUP;
	icr.bits.vector = (uint8_t)(cpu_startup_start_address >> 12U);
	msr_write(MSR_IA32_EXT_APIC_ICR, icr.value);

	if (cpu_info->displayfamily == 6U) {
		udelay(10U); /* 10us is enough for Modern processors */
	} else {
		udelay(200U); /* 200us for old processors */
	}

	/* Send another start IPI as per the Intel Arch specification */
	msr_write(MSR_IA32_EXT_APIC_ICR, icr.value);
}

void arch_send_dest_ipi_mask(uint64_t dest_mask, uint32_t vector)
{
	uint16_t pcpu_id;
	uint64_t mask = dest_mask;

	pcpu_id = ffs64(mask);
	while (pcpu_id < MAX_PCPU_NUM) {
		bitmap_clear_nolock(pcpu_id, &mask);
		arch_send_single_ipi(pcpu_id, vector);
		pcpu_id = ffs64(mask);
	}
}

void arch_send_single_ipi(uint16_t pcpu_id, uint32_t vector)
{
	union apic_icr icr;

	if (is_pcpu_active(pcpu_id)) {
		if (get_pcpu_id() == pcpu_id) {
			msr_write(MSR_IA32_EXT_APIC_SELF_IPI, vector);
		} else {
			/* Set the destination field to the target processor. */
			icr.value_32.hi_32 = per_cpu(lapic_id, pcpu_id);

			/* Write the vector ID to ICR. */
			icr.value_32.lo_32 = vector | (INTR_LAPIC_ICR_PHYSICAL << 11U);

			msr_write(MSR_IA32_EXT_APIC_ICR, icr.value);
		}
	} else {
		pr_err("pcpu_id %d not in active!", pcpu_id);
	}
}

/**
 * @pre pcpu_id < MAX_PCPU_NUM
 */
void send_single_init(uint16_t pcpu_id)
{
	union apic_icr icr;

	/*
	 * Intel SDM Vol3 23.8:
	 *   The INIT signal is blocked whenever a logical processor is in VMX root operation.
	 *   It is not blocked in VMX non-root operation. Instead, INITs cause VM exits
	 */

	icr.value_32.hi_32 = per_cpu(lapic_id, pcpu_id);
	icr.value_32.lo_32 = (INTR_LAPIC_ICR_PHYSICAL << 11U) | (INTR_LAPIC_ICR_INIT << 8U);

	msr_write(MSR_IA32_EXT_APIC_ICR, icr.value);

}

void kick_pcpu(uint16_t pcpu_id)
{
	if (per_cpu(mode_to_kick_pcpu, pcpu_id) == DEL_MODE_INIT) {
		send_single_init(pcpu_id);
	} else {
		arch_send_single_ipi(pcpu_id, NOTIFY_VCPU_VECTOR);
	}
}
