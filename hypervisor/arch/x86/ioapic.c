/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <ioapic.h>
#include <acpi.h>

#define	IOAPIC_MAX_PIN		240U
#define IOAPIC_INVALID_ID	0xFFU

/*
 * IOAPIC_MAX_LINES is architecturally defined.
 * The usable RTEs may be a subset of the total on a per IO APIC basis.
 */
#define IOAPIC_MAX_LINES	120U
#define NR_MAX_GSI		(CONFIG_MAX_IOAPIC_NUM * IOAPIC_MAX_LINES)

static struct gsi_table gsi_table_data[NR_MAX_GSI];
static uint32_t ioapic_nr_gsi;
static spinlock_t ioapic_lock;

static union ioapic_rte saved_rte[CONFIG_MAX_IOAPIC_NUM][IOAPIC_MAX_PIN];

/*
 * the irq to ioapic pin mapping should extract from ACPI MADT table
 * hardcoded here
 */
static const uint32_t legacy_irq_to_pin[NR_LEGACY_IRQ] = {
	2U, /* IRQ0*/
	1U, /* IRQ1*/
	0U, /* IRQ2 connected to Pin0 (ExtInt source of PIC) if existing */
	3U, /* IRQ3*/
	4U, /* IRQ4*/
	5U, /* IRQ5*/
	6U, /* IRQ6*/
	7U, /* IRQ7*/
	8U, /* IRQ8*/
	9U, /* IRQ9*/
	10U, /* IRQ10*/
	11U, /* IRQ11*/
	12U, /* IRQ12*/
	13U, /* IRQ13*/
	14U, /* IRQ14*/
	15U, /* IRQ15*/
};

static const uint32_t legacy_irq_trigger_mode[NR_LEGACY_IRQ] = {
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ0*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ1*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ2*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ3*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ4*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ5*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ6*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ7*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ8*/
	IOAPIC_RTE_TRGRMODE_LEVEL, /* IRQ9*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ10*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ11*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ12*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ13*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ14*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ15*/
};

static const uint32_t pic_ioapic_pin_map[NR_LEGACY_PIN] = {
	2U, /* pin0*/
	1U, /* pin1*/
	0U, /* pin2*/
	3U, /* pin3*/
	4U, /* pin4*/
	5U, /* pin5*/
	6U, /* pin6*/
	7U, /* pin7*/
	8U, /* pin8*/
	9U, /* pin9*/
	10U, /* pin10*/
	11U, /* pin11*/
	12U, /* pin12*/
	13U, /* pin13*/
	14U, /* pin14*/
	15U, /* pin15*/
};

static struct ioapic_info ioapic_array[CONFIG_MAX_IOAPIC_NUM];
static uint16_t ioapic_num;

uint32_t get_pic_pin_from_ioapic_pin(uint32_t pin_index)
{
	uint32_t pin_id = INVALID_INTERRUPT_PIN;
	if (pin_index < NR_LEGACY_PIN) {
		pin_id = pic_ioapic_pin_map[pin_index];
	}
	return pin_id;
}

void *ioapic_get_gsi_irq_addr(uint32_t irq_num)
{
	void *addr = NULL;
	if (irq_num < NR_MAX_GSI) {
		addr = gsi_table_data[irq_num].addr;
	}
	return addr;
}

uint32_t ioapic_get_nr_gsi(void)
{
	return ioapic_nr_gsi;
}

static void *map_ioapic(uint64_t ioapic_paddr)
{
	/* At some point we may need to translate this paddr to a vaddr.
	 * 1:1 mapping for now.
	 */
	return hpa2hva(ioapic_paddr);
}

static inline uint32_t
ioapic_read_reg32(void *ioapic_base, const uint32_t offset)
{
	uint32_t v;
	uint64_t rflags;

	spinlock_irqsave_obtain(&ioapic_lock, &rflags);

	/* Write IOREGSEL */
	mmio_write32(offset, ioapic_base + IOAPIC_REGSEL);
	/* Read  IOWIN */
	v = mmio_read32(ioapic_base + IOAPIC_WINDOW);

	spinlock_irqrestore_release(&ioapic_lock, rflags);
	return v;
}

static inline void
ioapic_write_reg32(void *ioapic_base, const uint32_t offset, const uint32_t value)
{
	uint64_t rflags;

	spinlock_irqsave_obtain(&ioapic_lock, &rflags);

	/* Write IOREGSEL */
	mmio_write32(offset, ioapic_base + IOAPIC_REGSEL);
	/* Write IOWIN */
	mmio_write32(value, ioapic_base + IOAPIC_WINDOW);

	spinlock_irqrestore_release(&ioapic_lock, rflags);
}

/**
 * @pre apic_id < 2
 */
static inline uint64_t
get_ioapic_base(uint8_t apic_id)
{
	/* the ioapic base should be extracted from ACPI MADT table */
	return ioapic_array[apic_id].addr;
}

void ioapic_get_rte_entry(void *ioapic_addr, uint32_t pin, union ioapic_rte *rte)
{
	uint32_t rte_addr = (pin * 2U) + 0x10U;
	rte->u.lo_32 = ioapic_read_reg32(ioapic_addr, rte_addr);
	rte->u.hi_32 = ioapic_read_reg32(ioapic_addr, rte_addr + 1U);
}

static inline void
ioapic_set_rte_entry(void *ioapic_addr,
		uint32_t pin, union ioapic_rte rte)
{
	uint32_t rte_addr = (pin * 2U) + 0x10U;
	ioapic_write_reg32(ioapic_addr, rte_addr, rte.u.lo_32);
	ioapic_write_reg32(ioapic_addr, rte_addr + 1U, rte.u.hi_32);
}

static inline union ioapic_rte
create_rte_for_legacy_irq(uint32_t irq, uint32_t vr)
{
	union ioapic_rte rte;

	/* Legacy IRQ 0-15 setup, default masked
	 * are actually defined in either MPTable or ACPI MADT table
	 * before we have ACPI table parsing in HV we use common hardcode
	 */

	rte.full = 0UL;
	rte.bits.intr_mask  = IOAPIC_RTE_MASK_SET;
	rte.bits.trigger_mode = legacy_irq_trigger_mode[irq];
	rte.bits.dest_mode = DEFAULT_DEST_MODE;
	rte.bits.delivery_mode = DEFAULT_DELIVERY_MODE;
	rte.bits.vector = vr;

	/* Fixed to active high */
	rte.bits.intr_polarity = IOAPIC_RTE_INTPOL_AHI;

	/* Dest field: legacy irq fixed to CPU0 */
	rte.bits.dest_field = 1U;

	return rte;
}

static inline union ioapic_rte
create_rte_for_gsi_irq(uint32_t irq, uint32_t vr)
{
	union ioapic_rte rte;

	rte.full = 0UL;

	if (irq < NR_LEGACY_IRQ) {
		rte = create_rte_for_legacy_irq(irq, vr);
	} else {
		/* irq default masked, level trig */
		rte.bits.intr_mask  = IOAPIC_RTE_MASK_SET;
		rte.bits.trigger_mode = IOAPIC_RTE_TRGRMODE_LEVEL;
		rte.bits.dest_mode = DEFAULT_DEST_MODE;
		rte.bits.delivery_mode = DEFAULT_DELIVERY_MODE;
		rte.bits.vector = vr;

		/* Fixed to active high */
		rte.bits.intr_polarity = IOAPIC_RTE_INTPOL_AHI;

		/* Dest field */
		rte.bits.dest_field = ALL_CPUS_MASK;
	}

	return rte;
}

static void ioapic_set_routing(uint32_t gsi, uint32_t vr)
{
	void *addr;
	union ioapic_rte rte;

	addr = gsi_table_data[gsi].addr;
	rte = create_rte_for_gsi_irq(gsi, vr);
	ioapic_set_rte_entry(addr, gsi_table_data[gsi].pin, rte);

	if (rte.bits.trigger_mode == IOAPIC_RTE_TRGRMODE_LEVEL) {
		set_irq_trigger_mode(gsi, true);
	} else {
		set_irq_trigger_mode(gsi, false);
	}

	dev_dbg(ACRN_DBG_IRQ, "GSI: irq:%d pin:%hhu rte:%lx",
		gsi, gsi_table_data[gsi].pin,
		rte.full);
}

/**
 * @pre rte != NULL
 */
void ioapic_get_rte(uint32_t irq, union ioapic_rte *rte)
{
	void *addr;

	if (ioapic_irq_is_gsi(irq)) {
		addr = gsi_table_data[irq].addr;
		ioapic_get_rte_entry(addr, gsi_table_data[irq].pin, rte);
	}
}

void ioapic_set_rte(uint32_t irq, union ioapic_rte rte)
{
	void *addr;

	if (ioapic_irq_is_gsi(irq)) {
		addr = gsi_table_data[irq].addr;
		ioapic_set_rte_entry(addr, gsi_table_data[irq].pin, rte);

		dev_dbg(ACRN_DBG_IRQ, "GSI: irq:%d pin:%hhu rte:%lx",
			irq, gsi_table_data[irq].pin,
			rte.full);
	}
}

bool ioapic_irq_is_gsi(uint32_t irq)
{
	return irq < ioapic_nr_gsi;
}

uint32_t ioapic_irq_to_pin(uint32_t irq)
{
	uint32_t ret;

	if (ioapic_irq_is_gsi(irq)) {
		ret = gsi_table_data[irq].pin;
	} else {
		ret = INVALID_INTERRUPT_PIN;
	}

	return ret;
}

bool ioapic_is_pin_valid(uint32_t pin)
{
	return (pin != INVALID_INTERRUPT_PIN);
}

uint32_t ioapic_pin_to_irq(uint32_t pin)
{
	uint32_t i;
	uint32_t irq = IRQ_INVALID;

	for (i = 0U; i < ioapic_nr_gsi; i++) {
		if (gsi_table_data[i].pin == pin) {
			irq = i;
			break;
		}
	}
	return irq;
}

static void
ioapic_irq_gsi_mask_unmask(uint32_t irq, bool mask)
{
	void *addr = NULL;
	uint32_t pin;
	union ioapic_rte rte;

	if (ioapic_irq_is_gsi(irq)) {
		addr = gsi_table_data[irq].addr;
		pin = gsi_table_data[irq].pin;

		if (addr != NULL) {
			ioapic_get_rte_entry(addr, pin, &rte);
			if (mask) {
				rte.bits.intr_mask = IOAPIC_RTE_MASK_SET;
			} else {
				rte.bits.intr_mask = IOAPIC_RTE_MASK_CLR;
			}
			ioapic_set_rte_entry(addr, pin, rte);
			dev_dbg(ACRN_DBG_PTIRQ, "update: irq:%d pin:%hhu rte:%lx",
				irq, pin, rte.full);
		} else {
			dev_dbg(ACRN_DBG_PTIRQ, "NULL Address returned from gsi_table_data");
		}
	}
}

void ioapic_gsi_mask_irq(uint32_t irq)
{
	ioapic_irq_gsi_mask_unmask(irq, true);
}

void ioapic_gsi_unmask_irq(uint32_t irq)
{
	ioapic_irq_gsi_mask_unmask(irq, false);
}

static uint32_t
ioapic_nr_pins(void *ioapic_base)
{
	uint32_t version;
	uint32_t nr_pins;

	version = ioapic_read_reg32(ioapic_base, IOAPIC_VER);
	dev_dbg(ACRN_DBG_IRQ, "IOAPIC version: %x", version);

	/* The 23:16 bits in the version register is the highest entry in the
	 * I/O redirection table, which is 1 smaller than the number of
	 * interrupt input pins. */
	nr_pins = (((version & IOAPIC_MAX_RTE_MASK) >> MAX_RTE_SHIFT) + 1U);

	ASSERT(nr_pins > NR_LEGACY_IRQ, "Legacy IRQ num > total GSI");
	ASSERT(nr_pins <= IOAPIC_MAX_PIN, "IOAPIC pins exceeding 240");

	return nr_pins;
}

uint8_t ioapic_irq_to_ioapic_id(uint32_t irq)
{
	uint8_t ret;

	if (ioapic_irq_is_gsi(irq)) {
		ret = gsi_table_data[irq].ioapic_id;
	} else {
		ret = IOAPIC_INVALID_ID;
	}

	return ret;
}

int32_t init_ioapic_id_info(void)
{
	int32_t ret = 0;

	ioapic_num = parse_madt_ioapic(&ioapic_array[0]);
	if (ioapic_num > (uint16_t)CONFIG_MAX_IOAPIC_NUM) {
		ret = -EINVAL;
	}

	return ret;
}

void ioapic_setup_irqs(void)
{
	uint8_t ioapic_id;
	uint32_t gsi = 0U;
	uint32_t vr;

	spinlock_init(&ioapic_lock);

	for (ioapic_id = 0U;
	     ioapic_id < ioapic_num; ioapic_id++) {
		void *addr;
		uint32_t pin, nr_pins;

		addr = map_ioapic(ioapic_array[ioapic_id].addr);
		hv_access_memory_region_update((uint64_t)addr, PAGE_SIZE);

		nr_pins = ioapic_nr_pins(addr);
		for (pin = 0U; pin < nr_pins; pin++) {
			gsi_table_data[gsi].ioapic_id = ioapic_array[ioapic_id].id;
			gsi_table_data[gsi].addr = addr;

			if (gsi < NR_LEGACY_IRQ) {
				gsi_table_data[gsi].pin =
					legacy_irq_to_pin[gsi] & 0xffU;
			} else {
				gsi_table_data[gsi].pin = pin;
			}

			/* pinned irq before use it */
			if (alloc_irq_num(gsi) == IRQ_INVALID) {
				pr_err("failed to alloc IRQ[%d]", gsi);
				gsi++;
				continue;
			}

			/* assign vector for this GSI
			 * for legacy irq, reserved vector and never free
			 */
			if (gsi < NR_LEGACY_IRQ) {
				vr = alloc_irq_vector(gsi);
				if (vr == VECTOR_INVALID) {
					pr_err("failed to alloc VR");
					gsi++;
					continue;
				}
			} else {
				vr = 0U; /* not to allocate VR right now */
			}

			ioapic_set_routing(gsi, vr);
			gsi++;
		}
	}

	/* system max gsi numbers */
	ioapic_nr_gsi = gsi;
	ASSERT(ioapic_nr_gsi <= NR_MAX_GSI, "GSI table overflow");
}

void suspend_ioapic(void)
{
	uint8_t ioapic_id;
	uint32_t ioapic_pin;

	for (ioapic_id = 0U; ioapic_id < ioapic_num; ioapic_id++) {
		void *addr;
		uint32_t nr_pins;

		addr = map_ioapic(get_ioapic_base(ioapic_id));
		nr_pins = ioapic_nr_pins(addr);
		for (ioapic_pin = 0U; ioapic_pin < nr_pins; ioapic_pin++) {
			ioapic_get_rte_entry(addr, ioapic_pin,
				&saved_rte[ioapic_id][ioapic_pin]);
		}
	}
}

void resume_ioapic(void)
{
	uint8_t ioapic_id;
	uint32_t ioapic_pin;

	for (ioapic_id = 0U; ioapic_id < ioapic_num; ioapic_id++) {
		void *addr;
		uint32_t nr_pins;

		addr = map_ioapic(get_ioapic_base(ioapic_id));
		nr_pins = ioapic_nr_pins(addr);
		for (ioapic_pin = 0U; ioapic_pin < nr_pins; ioapic_pin++) {
			ioapic_set_rte_entry(addr, ioapic_pin,
				saved_rte[ioapic_id][ioapic_pin]);
		}
	}
}
