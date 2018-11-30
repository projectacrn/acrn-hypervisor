/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#define	IOAPIC_MAX_PIN		240U
#define IOAPIC_INVALID_PIN      0xffU

struct gsi_table gsi_table[NR_MAX_GSI];
uint32_t nr_gsi;
static spinlock_t ioapic_lock;

static union ioapic_rte saved_rte[NR_IOAPICS][IOAPIC_MAX_PIN];

/*
 * the irq to ioapic pin mapping should extract from ACPI MADT table
 * hardcoded here
 */
static uint8_t legacy_irq_to_pin[NR_LEGACY_IRQ] = {
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

static uint64_t legacy_irq_trigger_mode[NR_LEGACY_IRQ] = {
	IOAPIC_RTE_TRGREDG, /* IRQ0*/
	IOAPIC_RTE_TRGREDG, /* IRQ1*/
	IOAPIC_RTE_TRGREDG, /* IRQ2*/
	IOAPIC_RTE_TRGREDG, /* IRQ3*/
	IOAPIC_RTE_TRGREDG, /* IRQ4*/
	IOAPIC_RTE_TRGREDG, /* IRQ5*/
	IOAPIC_RTE_TRGREDG, /* IRQ6*/
	IOAPIC_RTE_TRGREDG, /* IRQ7*/
	IOAPIC_RTE_TRGREDG, /* IRQ8*/
	IOAPIC_RTE_TRGRLVL, /* IRQ9*/
	IOAPIC_RTE_TRGREDG, /* IRQ10*/
	IOAPIC_RTE_TRGREDG, /* IRQ11*/
	IOAPIC_RTE_TRGREDG, /* IRQ12*/
	IOAPIC_RTE_TRGREDG, /* IRQ13*/
	IOAPIC_RTE_TRGREDG, /* IRQ14*/
	IOAPIC_RTE_TRGREDG, /* IRQ15*/
};

uint8_t pic_ioapic_pin_map[NR_LEGACY_PIN] = {
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

static void *map_ioapic(uint64_t ioapic_paddr)
{
	/* At some point we may need to translate this paddr to a vaddr.
	 * 1:1 mapping for now.
	 */
	return hpa2hva(ioapic_paddr);
}

static inline uint32_t
ioapic_read_reg32(const void *ioapic_base, const uint32_t offset)
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
ioapic_write_reg32(const void *ioapic_base,
		const uint32_t offset, const uint32_t value)
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
	const uint64_t addr[2] = {IOAPIC0_BASE, IOAPIC1_BASE};

	/* the ioapic base should be extracted from ACPI MADT table */
	return addr[apic_id];
}

void ioapic_get_rte_entry(void *ioapic_addr, uint8_t pin, union ioapic_rte *rte)
{
	uint32_t rte_addr = ((uint32_t)pin * 2U) + 0x10U;
	rte->u.lo_32 = ioapic_read_reg32(ioapic_addr, rte_addr);
	rte->u.hi_32 = ioapic_read_reg32(ioapic_addr, rte_addr + 1U);
}

static inline void
ioapic_set_rte_entry(void *ioapic_addr,
		uint8_t pin, union ioapic_rte rte)
{
	uint32_t rte_addr = ((uint32_t)pin * 2U) + 0x10U;
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

	rte.full  = IOAPIC_RTE_INTMSET;
	rte.full |= legacy_irq_trigger_mode[irq];
	rte.full |= DEFAULT_DEST_MODE;
	rte.full |= DEFAULT_DELIVERY_MODE;
	rte.full |= (IOAPIC_RTE_INTVEC & (uint64_t)vr);

	/* Fixed to active high */
	rte.full |= IOAPIC_RTE_INTAHI;

	/* Dest field: legacy irq fixed to CPU0 */
	rte.full |= (1UL << IOAPIC_RTE_DEST_SHIFT);

	return rte;
}

static inline union ioapic_rte
create_rte_for_gsi_irq(uint32_t irq, uint32_t vr)
{
	union ioapic_rte rte;

	if (irq < NR_LEGACY_IRQ) {
		rte = create_rte_for_legacy_irq(irq, vr);
	} else {
		/* irq default masked, level trig */
		rte.full  = IOAPIC_RTE_INTMSET;
		rte.full |= IOAPIC_RTE_TRGRLVL;
		rte.full |= DEFAULT_DEST_MODE;
		rte.full |= DEFAULT_DELIVERY_MODE;
		rte.full |= (IOAPIC_RTE_INTVEC & (uint64_t)vr);

		/* Fixed to active high */
		rte.full |= IOAPIC_RTE_INTAHI;

		/* Dest field */
		rte.full |= (ALL_CPUS_MASK << IOAPIC_RTE_DEST_SHIFT);
	}

	return rte;
}

static void ioapic_set_routing(uint32_t gsi, uint32_t vr)
{
	void *addr;
	union ioapic_rte rte;

	addr = gsi_table[gsi].addr;
	rte = create_rte_for_gsi_irq(gsi, vr);
	ioapic_set_rte_entry(addr, gsi_table[gsi].pin, rte);

	if ((rte.full & IOAPIC_RTE_TRGRMOD) != 0UL) {
		set_irq_trigger_mode(gsi, true);
	} else {
		set_irq_trigger_mode(gsi, false);
	}

	dev_dbg(ACRN_DBG_IRQ, "GSI: irq:%d pin:%hhu rte:%lx",
		gsi, gsi_table[gsi].pin,
		rte.full);
}

/**
 * @pre rte != NULL
 */
void ioapic_get_rte(uint32_t irq, union ioapic_rte *rte)
{
	void *addr;

	if (irq_is_gsi(irq)) {
		addr = gsi_table[irq].addr;
		ioapic_get_rte_entry(addr, gsi_table[irq].pin, rte);
	}
}

void ioapic_set_rte(uint32_t irq, union ioapic_rte rte)
{
	void *addr;

	if (irq_is_gsi(irq)) {
		addr = gsi_table[irq].addr;
		ioapic_set_rte_entry(addr, gsi_table[irq].pin, rte);

		dev_dbg(ACRN_DBG_IRQ, "GSI: irq:%d pin:%hhu rte:%lx",
			irq, gsi_table[irq].pin,
			rte.full);
	}
}

bool irq_is_gsi(uint32_t irq)
{
	return irq < nr_gsi;
}

uint8_t irq_to_pin(uint32_t irq)
{
	uint8_t ret;

	if (irq_is_gsi(irq)) {
		ret = gsi_table[irq].pin;
	} else {
	        ret = IOAPIC_INVALID_PIN;
	}

	return ret;
}

uint32_t pin_to_irq(uint8_t pin)
{
	uint32_t i;

	for (i = 0U; i < nr_gsi; i++) {
		if (gsi_table[i].pin == pin) {
			return i;
		}
	}
	return IRQ_INVALID;
}

static void
irq_gsi_mask_unmask(uint32_t irq, bool mask)
{
	void *addr;
	uint8_t pin;
	union ioapic_rte rte;

	if (irq_is_gsi(irq)) {
		addr = gsi_table[irq].addr;
		pin = gsi_table[irq].pin;

		ioapic_get_rte_entry(addr, pin, &rte);
		if (mask) {
			rte.full |= IOAPIC_RTE_INTMSET;
		} else {
			rte.full &= ~IOAPIC_RTE_INTMASK;
		}
		ioapic_set_rte_entry(addr, pin, rte);
		dev_dbg(ACRN_DBG_PTIRQ, "update: irq:%d pin:%hhu rte:%lx",
			irq, pin, rte.full);
	}
}

void gsi_mask_irq(uint32_t irq)
{
	irq_gsi_mask_unmask(irq, true);
}

void gsi_unmask_irq(uint32_t irq)
{
	irq_gsi_mask_unmask(irq, false);
}

static uint8_t
ioapic_nr_pins(void *ioapic_base)
{
	uint32_t version;
	uint8_t nr_pins;

	version = ioapic_read_reg32(ioapic_base, IOAPIC_VER);
	dev_dbg(ACRN_DBG_IRQ, "IOAPIC version: %x", version);

	/* The 23:16 bits in the version register is the highest entry in the
	 * I/O redirection table, which is 1 smaller than the number of
	 * interrupt input pins. */
	nr_pins = (uint8_t)
		(((version & IOAPIC_MAX_RTE_MASK) >> MAX_RTE_SHIFT) + 1U);

	ASSERT(nr_pins > NR_LEGACY_IRQ, "Legacy IRQ num > total GSI");
	ASSERT(nr_pins <= IOAPIC_MAX_PIN, "IOAPIC pins exceeding 240");

	return nr_pins;
}

void setup_ioapic_irqs(void)
{
	uint8_t ioapic_id;
	uint32_t gsi = 0U;
	uint32_t vr;

	spinlock_init(&ioapic_lock);

	for (ioapic_id = 0U;
	     ioapic_id < NR_IOAPICS; ioapic_id++) {
		void *addr;
		uint8_t pin, nr_pins;

		addr = map_ioapic(get_ioapic_base(ioapic_id));
		nr_pins = ioapic_nr_pins(addr);
		for (pin = 0U; pin < nr_pins; pin++) {
			gsi_table[gsi].ioapic_id = ioapic_id;
			gsi_table[gsi].addr = addr;

			if (gsi < NR_LEGACY_IRQ) {
				gsi_table[gsi].pin =
					legacy_irq_to_pin[gsi] & 0xffU;
			} else {
				gsi_table[gsi].pin = pin;
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
	nr_gsi = gsi;
	ASSERT(nr_gsi <= NR_MAX_GSI, "GSI table overflow");
}

void suspend_ioapic(void)
{
	uint8_t ioapic_id, ioapic_pin;

	for (ioapic_id = 0U; ioapic_id < NR_IOAPICS; ioapic_id++) {
		void *addr;
		uint8_t nr_pins;

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
	uint8_t ioapic_id, ioapic_pin;

	for (ioapic_id = 0U; ioapic_id < NR_IOAPICS; ioapic_id++) {
		void *addr;
		uint8_t nr_pins;

		addr = map_ioapic(get_ioapic_base(ioapic_id));
		nr_pins = ioapic_nr_pins(addr);
		for (ioapic_pin = 0U; ioapic_pin < nr_pins; ioapic_pin++) {
			ioapic_set_rte_entry(addr, ioapic_pin,
				saved_rte[ioapic_id][ioapic_pin]);
		}
	}
}
