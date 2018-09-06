/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

/* Rate range 1 to 1000 or 1uSec to 1mSec */
#define APIC_TIMER_MAX      0xffffffffU
#define HYPE_PERIOD_MAX     1000
#define APIC_DIVIDE_BY_ONE  0x0b
#define PIT_TARGET          0x3FFFU

/* xAPIC/x2APIC Interrupt Command Register (ICR) structure */
union apic_icr {
	uint64_t value;
	struct {
		uint32_t lo_32;
		uint32_t hi_32;
	} value_32;
	struct {
		uint32_t vector:8;
		uint32_t delivery_mode:3;
		uint32_t destination_mode:1;
		uint32_t delivery_status:1;
		uint32_t rsvd_1:1;
		uint32_t level:1;
		uint32_t trigger_mode:1;
		uint32_t rsvd_2:2;
		uint32_t shorthand:2;
		uint32_t rsvd_3:12;
		uint32_t rsvd_4:32;
	} bits;
	struct {
		uint32_t rsvd_1:32;
		uint32_t rsvd_2:24;
		uint32_t dest_field:8;
	} x_bits;
	struct {
		uint32_t rsvd_1:32;
		uint32_t dest_field:32;
	} x2_bits;
};

/* xAPIC/x2APIC Interrupt Command Register (ICR) structure */
union apic_lvt {
	uint32_t value;
	union {
		struct {
			uint32_t vector:8;
			uint32_t rsvd_1:4;
			uint32_t delivery_status:1;
			uint32_t rsvd_2:3;
			uint32_t mask:1;
			uint32_t mode:2;
			uint32_t rsvd_3:13;
		} timer;
		struct {
			uint32_t vector:8;
			uint32_t delivery_mode:3;
			uint32_t rsvd_1:1;
			uint32_t delivery_status:1;
			uint32_t rsvd_2:3;
			uint32_t mask:1;
			uint32_t rsvd_3:15;
		} cmci;
		struct {
			uint32_t vector:8;
			uint32_t delivery_mode:3;
			uint32_t rsvd_1:1;
			uint32_t delivery_status:1;
			uint32_t polarity:1;
			uint32_t remote_irr:1;
			uint32_t trigger_mode:1;
			uint32_t mask:1;
			uint32_t rsvd_2:15;
		} lint;
		struct {
			uint32_t vector:8;
			uint32_t rsvd_1:4;
			uint32_t delivery_status:1;
			uint32_t rsvd_2:3;
			uint32_t mask:1;
			uint32_t rsvd_3:15;
		} error;
		struct {
			uint32_t vector:8;
			uint32_t delivery_mode:3;
			uint32_t rsvd_1:1;
			uint32_t delivery_status:1;
			uint32_t rsvd_2:3;
			uint32_t mask:1;
			uint32_t rsvd_3:15;
		} pmc;
		struct {
			uint32_t vector:8;
			uint32_t delivery_mode:3;
			uint32_t rsvd_1:1;
			uint32_t delivery_status:1;
			uint32_t rsvd_2:3;
			uint32_t mask:1;
			uint32_t rsvd_3:15;
		} thermal;
		struct {
			uint32_t vector:8;
			uint32_t rsvd_1:4;
			uint32_t delivery_status:1;
			uint32_t rsvd_2:3;
			uint32_t mask:1;
			uint32_t rsvd_3:15;
		} common;
	} bits;
};

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

struct lapic_info {
	bool init_done;
	struct {
		uint64_t paddr;
		void *vaddr;
	} xapic;
};

static struct lapic_info lapic_info;
static struct lapic_regs saved_lapic_regs;
static union lapic_base_msr lapic_base_msr;

static inline uint32_t read_lapic_reg32(uint32_t offset)
{
	if (offset < 0x20U || offset > 0x3ffU)
		return 0;

	return mmio_read32(lapic_info.xapic.vaddr + offset);
}

void write_lapic_reg32(uint32_t offset, uint32_t value)
{
	if (offset < 0x20U || offset > 0x3ffU)
		return;

	mmio_write32(value, lapic_info.xapic.vaddr + offset);
}

static void clear_lapic_isr(void)
{
	uint32_t isr_reg = LAPIC_IN_SERVICE_REGISTER_0;

	/* This is a Intel recommended procedure and assures that the processor
	 * does not get hung up due to already set "in-service" interrupts left
	 * over from the boot loader environment. This actually occurs in real
	 * life, therefore we will ensure all the in-service bits are clear.
	 */
	do {
		if (read_lapic_reg32(isr_reg) != 0U) {
			write_lapic_reg32(LAPIC_EOI_REGISTER, 0U);
			continue;
		}
		isr_reg += 0x10U;
	} while (isr_reg <= LAPIC_IN_SERVICE_REGISTER_7);
}

static void map_lapic(void)
{
	/* At some point we may need to translate this paddr to a vaddr. 1:1
	 * mapping for now.
	 */
	lapic_info.xapic.vaddr = hpa2hva(lapic_info.xapic.paddr);
}

void early_init_lapic(void)
{
	/* Get local APIC base address */
	lapic_base_msr.value = msr_read(MSR_IA32_APIC_BASE);

	/* Initialize globals only 1 time */
	if (lapic_info.init_done == false) {
		/* Get Local APIC physical address. */
		lapic_info.xapic.paddr = LAPIC_BASE;

		/* Map in the local xAPIC */
		map_lapic();

		lapic_info.init_done = true;
	}

	/* Check if xAPIC mode enabled */
	if (lapic_base_msr.fields.xAPIC_enable == 0U) {
		/* Ensure in xAPIC mode */
		lapic_base_msr.fields.xAPIC_enable = 1U;
		lapic_base_msr.fields.x2APIC_enable = 0U;
		msr_write(MSR_IA32_APIC_BASE, lapic_base_msr.value);
	} else {
		/* Check if x2apic is disabled */
		ASSERT(lapic_base_msr.fields.x2APIC_enable == 0U,
			"Disable X2APIC in BIOS");
	}
}

/**
 * @pre pcpu_id < 8U
 */
void init_lapic(uint16_t pcpu_id)
{
	/* Set the Logical Destination Register */
	write_lapic_reg32(LAPIC_LOGICAL_DESTINATION_REGISTER,
		((1U << pcpu_id) << 24U));

	/* Set the Destination Format Register */
	write_lapic_reg32(LAPIC_DESTINATION_FORMAT_REGISTER, 0xfU << 28U);

	/* Mask all LAPIC LVT entries before enabling the local APIC */
	write_lapic_reg32(LAPIC_LVT_CMCI_REGISTER, LAPIC_LVT_MASK);
	write_lapic_reg32(LAPIC_LVT_TIMER_REGISTER, LAPIC_LVT_MASK);
	write_lapic_reg32(LAPIC_LVT_THERMAL_SENSOR_REGISTER, LAPIC_LVT_MASK);
	write_lapic_reg32(LAPIC_LVT_PMC_REGISTER, LAPIC_LVT_MASK);
	write_lapic_reg32(LAPIC_LVT_LINT0_REGISTER, LAPIC_LVT_MASK);
	write_lapic_reg32(LAPIC_LVT_LINT1_REGISTER, LAPIC_LVT_MASK);
	write_lapic_reg32(LAPIC_LVT_ERROR_REGISTER, LAPIC_LVT_MASK);

	/* Enable Local APIC */
	/* TODO: add spurious-interrupt handler */
	write_lapic_reg32(LAPIC_SPURIOUS_VECTOR_REGISTER,
		       LAPIC_SVR_APIC_ENABLE_MASK | LAPIC_SVR_VECTOR);

	/* Ensure there are no ISR bits set. */
	clear_lapic_isr();
}

void save_lapic(struct lapic_regs *regs)
{
	regs->id = read_lapic_reg32(LAPIC_ID_REGISTER);
	regs->tpr = read_lapic_reg32(LAPIC_TASK_PRIORITY_REGISTER);
	regs->apr = read_lapic_reg32(LAPIC_ARBITRATION_PRIORITY_REGISTER);
	regs->ppr = read_lapic_reg32(LAPIC_PROCESSOR_PRIORITY_REGISTER);
	regs->ldr = read_lapic_reg32(LAPIC_LOGICAL_DESTINATION_REGISTER);
	regs->dfr = read_lapic_reg32(LAPIC_DESTINATION_FORMAT_REGISTER);
	regs->tmr[0].val = read_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_0);
	regs->tmr[1].val = read_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_1);
	regs->tmr[2].val = read_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_2);
	regs->tmr[3].val = read_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_3);
	regs->tmr[4].val = read_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_4);
	regs->tmr[5].val = read_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_5);
	regs->tmr[6].val = read_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_6);
	regs->tmr[7].val = read_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_7);
	regs->svr = read_lapic_reg32(LAPIC_SPURIOUS_VECTOR_REGISTER);
	regs->lvt[APIC_LVT_TIMER].val =
		read_lapic_reg32(LAPIC_LVT_TIMER_REGISTER);
	regs->lvt[APIC_LVT_LINT0].val =
		read_lapic_reg32(LAPIC_LVT_LINT0_REGISTER);
	regs->lvt[APIC_LVT_LINT1].val =
		read_lapic_reg32(LAPIC_LVT_LINT1_REGISTER);
	regs->lvt[APIC_LVT_ERROR].val =
		read_lapic_reg32(LAPIC_LVT_ERROR_REGISTER);
	regs->icr_timer = read_lapic_reg32(LAPIC_INITIAL_COUNT_REGISTER);
	regs->ccr_timer = read_lapic_reg32(LAPIC_CURRENT_COUNT_REGISTER);
	regs->dcr_timer = read_lapic_reg32(LAPIC_DIVIDE_CONFIGURATION_REGISTER);
}

static void restore_lapic(struct lapic_regs *regs)
{
	write_lapic_reg32(LAPIC_ID_REGISTER, regs->id);
	write_lapic_reg32(LAPIC_TASK_PRIORITY_REGISTER, regs->tpr);
	write_lapic_reg32(LAPIC_LOGICAL_DESTINATION_REGISTER, regs->ldr );
	write_lapic_reg32(LAPIC_DESTINATION_FORMAT_REGISTER, regs->dfr );
	write_lapic_reg32(LAPIC_SPURIOUS_VECTOR_REGISTER, regs->svr );
	write_lapic_reg32(LAPIC_LVT_TIMER_REGISTER,
			regs->lvt[APIC_LVT_TIMER].val);

	write_lapic_reg32(LAPIC_LVT_LINT0_REGISTER,
			regs->lvt[APIC_LVT_LINT0].val);
	write_lapic_reg32(LAPIC_LVT_LINT1_REGISTER,
			regs->lvt[APIC_LVT_LINT1].val);

	write_lapic_reg32(LAPIC_LVT_ERROR_REGISTER,
			regs->lvt[APIC_LVT_ERROR].val);
	write_lapic_reg32(LAPIC_INITIAL_COUNT_REGISTER, regs->icr_timer);
	write_lapic_reg32(LAPIC_DIVIDE_CONFIGURATION_REGISTER, regs->dcr_timer);


	write_lapic_reg32(LAPIC_ARBITRATION_PRIORITY_REGISTER, regs->apr);
	write_lapic_reg32(LAPIC_PROCESSOR_PRIORITY_REGISTER, regs->ppr);
	write_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_0, regs->tmr[0].val);
	write_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_1, regs->tmr[1].val);
	write_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_2, regs->tmr[2].val);
	write_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_3, regs->tmr[3].val);
	write_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_4, regs->tmr[4].val);
	write_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_5, regs->tmr[5].val);
	write_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_6, regs->tmr[6].val);
	write_lapic_reg32(LAPIC_TRIGGER_MODE_REGISTER_7, regs->tmr[7].val);
	write_lapic_reg32(LAPIC_CURRENT_COUNT_REGISTER, regs->ccr_timer);
}

void suspend_lapic(void)
{
	uint32_t val;

	save_lapic(&saved_lapic_regs);

	/* disable APIC with software flag */
	val = read_lapic_reg32(LAPIC_SPURIOUS_VECTOR_REGISTER);
	val = (~LAPIC_SVR_APIC_ENABLE_MASK) & val;
	write_lapic_reg32(LAPIC_SPURIOUS_VECTOR_REGISTER, val);
}

void resume_lapic(void)
{
	msr_write(MSR_IA32_APIC_BASE, lapic_base_msr.value);

	/* ACPI software flag will be restored also */
	restore_lapic(&saved_lapic_regs);
}

void send_lapic_eoi(void)
{
	write_lapic_reg32(LAPIC_EOI_REGISTER, 0U);
}

static void wait_for_delivery(void)
{
	union apic_icr tmp;

	do {
		tmp.value_32.lo_32 =
			read_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_0);
	} while (tmp.bits.delivery_status != 0U);
}

uint8_t get_cur_lapic_id(void)
{
	uint32_t lapic_id_reg;
	uint8_t lapic_id;

	lapic_id_reg = read_lapic_reg32(LAPIC_ID_REGISTER);
	lapic_id = (uint8_t)(lapic_id_reg >> 24U);

	return lapic_id;
}

/**
 * @pre cpu_startup_shorthand < INTR_CPU_STARTUP_UNKNOWN
 */
void
send_startup_ipi(enum intr_cpu_startup_shorthand cpu_startup_shorthand,
	uint16_t dest_pcpu_id, uint64_t cpu_startup_start_address)
{
	union apic_icr icr;
	uint8_t shorthand;

	icr.value = 0U;
	icr.bits.destination_mode = INTR_LAPIC_ICR_PHYSICAL;

	if (cpu_startup_shorthand == INTR_CPU_STARTUP_USE_DEST) {
		shorthand = INTR_LAPIC_ICR_USE_DEST_ARRAY;
		icr.x_bits.dest_field = per_cpu(lapic_id, dest_pcpu_id);
	} else {		/* Use destination shorthand */
		shorthand = INTR_LAPIC_ICR_ALL_EX_SELF;
		icr.value_32.hi_32 = 0U;
	}

	/* Assert INIT IPI */
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_1, icr.value_32.hi_32);
	icr.bits.shorthand = shorthand;
	icr.bits.delivery_mode = INTR_LAPIC_ICR_INIT;
	icr.bits.level = INTR_LAPIC_ICR_ASSERT;
	icr.bits.trigger_mode = INTR_LAPIC_ICR_LEVEL;
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_0, icr.value_32.lo_32);
	wait_for_delivery();

	/* Give 10ms for INIT sequence to complete for old processors.
	 * Modern processors (family == 6) don't need to wait here.
	 */
	if (boot_cpu_data.family != 6U)
		mdelay(10U);

	/* De-assert INIT IPI */
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_1, icr.value_32.hi_32);
	icr.bits.level = INTR_LAPIC_ICR_DEASSERT;
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_0, icr.value_32.lo_32);
	wait_for_delivery();

	/* Send Start IPI with page number of secondary reset code */
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_1, icr.value_32.hi_32);
	icr.value_32.lo_32 = 0U;
	icr.bits.shorthand = shorthand;
	icr.bits.delivery_mode = INTR_LAPIC_ICR_STARTUP;
	icr.bits.vector = (uint8_t)(cpu_startup_start_address >> 12U);
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_0, icr.value_32.lo_32);
	wait_for_delivery();

	if (boot_cpu_data.family == 6U)
		udelay(10U); /* 10us is enough for Modern processors */
	else
		udelay(200U); /* 200us for old processors */

	/* Send another start IPI as per the Intel Arch specification */
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_1, icr.value_32.hi_32);
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_0, icr.value_32.lo_32);
	wait_for_delivery();
}

/* dest_mode must be INTR_LAPIC_ICR_PHYSICAL(0x0U) or
 * INTR_LAPIC_ICR_LOGICAL(0x1U)
 */
void send_dest_ipi(uint32_t dest, uint32_t vector, uint32_t dest_mode)
{
	uint32_t hi_32, lo_32;

	/* Set the target processor. */
	hi_32 = dest << 24U;

	/* Set the vector ID. */
	lo_32 = vector | (dest_mode << 11U);

	/* Set the destination field to the target processor. */
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_1, hi_32);

	/* Write the vector ID to ICR. */
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_0, lo_32);

	wait_for_delivery();
}

void send_single_ipi(uint16_t pcpu_id, uint32_t vector)
{
	uint32_t dest_lapic_id;

	/* Get the lapic ID of the destination processor. */
	dest_lapic_id = per_cpu(lapic_id, pcpu_id);

	return send_dest_ipi(dest_lapic_id, vector, INTR_LAPIC_ICR_PHYSICAL);
}

int send_shorthand_ipi(uint8_t vector,
	uint8_t shorthand, uint8_t delivery_mode)
{
	union apic_icr icr;
	int status = 0;

	if ((shorthand < INTR_LAPIC_ICR_SELF)
			|| (shorthand > INTR_LAPIC_ICR_ALL_EX_SELF)
			|| (delivery_mode > INTR_LAPIC_ICR_NMI))
		status = -EINVAL;

	ASSERT(status == 0, "Incorrect arguments");

	icr.value = 0U;
	icr.bits.shorthand = shorthand;
	icr.bits.delivery_mode = delivery_mode;
	icr.bits.vector = vector;
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_1, icr.value_32.hi_32);
	write_lapic_reg32(LAPIC_INT_COMMAND_REGISTER_0, icr.value_32.lo_32);
	wait_for_delivery();

	return status;
}
