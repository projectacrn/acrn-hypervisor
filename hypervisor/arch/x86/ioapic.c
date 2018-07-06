/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

/* Register offsets */
#define IOAPIC_REGSEL_OFFSET   0
#define IOAPIC_WINSWL_OFFSET   0x10

#define	IOAPIC_MAX_PIN		240U
#define IOAPIC_INVALID_PIN      0xffU

/* IOAPIC Redirection Table (RTE) Entry structure */
struct ioapic_rte {
	uint32_t lo_32;
	uint32_t hi_32;
} ioapic_rte;

struct gsi_table {
	uint8_t ioapic_id;
	uint8_t pin;
	void  *addr;
};
static struct gsi_table gsi_table[NR_MAX_GSI];
static uint32_t nr_gsi;
static spinlock_t ioapic_lock;

static struct ioapic_rte saved_rte[CONFIG_NR_IOAPICS][IOAPIC_MAX_PIN];

/*
 * the irq to ioapic pin mapping should extract from ACPI MADT table
 * hardcoded here
 */
uint8_t legacy_irq_to_pin[NR_LEGACY_IRQ] = {
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

uint32_t legacy_irq_trigger_mode[NR_LEGACY_IRQ] = {
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
	return HPA2HVA(ioapic_paddr);
}

static inline uint32_t
ioapic_read_reg32(const void *ioapic_base, const uint32_t offset)
{
	uint32_t v;

	spinlock_rflags;

	spinlock_irqsave_obtain(&ioapic_lock);

	/* Write IOREGSEL */
	mmio_write_long(offset, (void *)ioapic_base);
	/* Read  IOWIN */
	v = mmio_read_long((void *)ioapic_base + IOAPIC_WINSWL_OFFSET);

	spinlock_irqrestore_release(&ioapic_lock);
	return v;
}

static inline void
ioapic_write_reg32(const void *ioapic_base,
		const uint32_t offset, const uint32_t value)
{
	spinlock_rflags;

	spinlock_irqsave_obtain(&ioapic_lock);

	/* Write IOREGSEL */
	mmio_write_long(offset, (void *)ioapic_base);
	/* Write IOWIN */
	mmio_write_long(value, (void *)ioapic_base + IOAPIC_WINSWL_OFFSET);

	spinlock_irqrestore_release(&ioapic_lock);
}

static inline uint64_t
get_ioapic_base(uint8_t apic_id)
{
	uint64_t addr = 0xffffffffffffffffUL;

	/* should extract next ioapic from ACPI MADT table */
	if (apic_id == 0U)
		addr = DEFAULT_IO_APIC_BASE;
	else if (apic_id == 1U)
		addr = 0xfec3f000UL;
	else if (apic_id == 2U)
		addr = 0xfec7f000UL;
	else
		ASSERT(apic_id <= 2U, "ACPI MADT table missing");
	return addr;
}


static inline void
ioapic_get_rte_entry(void *ioapic_addr,
		uint8_t pin, struct ioapic_rte *rte)
{
	uint32_t rte_addr = (uint32_t)pin * 2U + 0x10U;
	rte->lo_32 = ioapic_read_reg32(ioapic_addr, rte_addr);
	rte->hi_32 = ioapic_read_reg32(ioapic_addr, rte_addr + 1U);
}

static inline void
ioapic_set_rte_entry(void *ioapic_addr,
		uint8_t pin, struct ioapic_rte *rte)
{
	uint32_t rte_addr = (uint32_t)pin * 2U + 0x10U;
	ioapic_write_reg32(ioapic_addr, rte_addr, rte->lo_32);
	ioapic_write_reg32(ioapic_addr, rte_addr + 1U, rte->hi_32);
}

static inline struct ioapic_rte
create_rte_for_legacy_irq(uint32_t irq, uint32_t vr)
{
	struct ioapic_rte rte = {0, 0};

	/* Legacy IRQ 0-15 setup, default masked
	 * are actually defined in either MPTable or ACPI MADT table
	 * before we have ACPI table parsing in HV we use common hardcode
	 */

	rte.lo_32 |= IOAPIC_RTE_INTMSET;

	rte.lo_32 |= legacy_irq_trigger_mode[irq];
	rte.lo_32 |= DEFAULT_DEST_MODE;
	rte.lo_32 |= DEFAULT_DELIVERY_MODE;
	rte.lo_32 |= (IOAPIC_RTE_INTVEC & vr);

	/* FIXME: Fixed to active Low? */
	rte.lo_32 |= IOAPIC_RTE_INTALO;

	/* Dest field: legacy irq fixed to CPU0 */
	rte.hi_32 |= 1U << 24;

	return rte;
}

static inline struct ioapic_rte
create_rte_for_gsi_irq(uint32_t irq, uint32_t vr)
{
	struct ioapic_rte rte = {0, 0};

	if (irq < NR_LEGACY_IRQ)
		return create_rte_for_legacy_irq(irq, vr);

	/* irq default masked, level trig */
	rte.lo_32 |= IOAPIC_RTE_INTMSET;
	rte.lo_32 |= IOAPIC_RTE_TRGRLVL;
	rte.lo_32 |= DEFAULT_DEST_MODE;
	rte.lo_32 |= DEFAULT_DELIVERY_MODE;
	rte.lo_32 |= (IOAPIC_RTE_INTVEC & vr);

	/* FIXME: Fixed to active Low? */
	rte.lo_32 |= IOAPIC_RTE_INTALO;

	/* Dest field */
	rte.hi_32 |= ALL_CPUS_MASK << 24U;

	return rte;
}

static void ioapic_set_routing(uint32_t gsi, uint32_t vr)
{
	void *addr;
	struct ioapic_rte rte;

	addr = gsi_table[gsi].addr;
	rte = create_rte_for_gsi_irq(gsi, vr);
	ioapic_set_rte_entry(addr, gsi_table[gsi].pin, &rte);

	if ((rte.lo_32 & IOAPIC_RTE_TRGRMOD) != 0U)
		update_irq_handler(gsi, handle_level_interrupt_common);
	else
		update_irq_handler(gsi, common_handler_edge);

	dev_dbg(ACRN_DBG_IRQ, "GSI: irq:%d pin:%hhu rte:%x",
		gsi, gsi_table[gsi].pin,
		rte.lo_32);
}

void ioapic_get_rte(uint32_t irq, uint64_t *rte)
{
	void *addr;
	struct ioapic_rte _rte;

	if (!irq_is_gsi(irq))
		return;

	addr = gsi_table[irq].addr;
	ioapic_get_rte_entry(addr, gsi_table[irq].pin, &_rte);

	*rte = _rte.hi_32;
	*rte = *rte << 32 | _rte.lo_32;
}

void ioapic_set_rte(uint32_t irq, uint64_t raw_rte)
{
	void *addr;
	struct ioapic_rte rte;

	if (!irq_is_gsi(irq))
		return;

	addr = gsi_table[irq].addr;
	rte.lo_32 = raw_rte;
	rte.hi_32 = raw_rte >> 32;
	ioapic_set_rte_entry(addr, gsi_table[irq].pin, &rte);

	dev_dbg(ACRN_DBG_IRQ, "GSI: irq:%d pin:%hhu rte:%x",
		irq, gsi_table[irq].pin,
		rte.lo_32);
}

uint32_t irq_gsi_num(void)
{
	return nr_gsi;
}

bool irq_is_gsi(uint32_t irq)
{
	return irq < nr_gsi;
}

uint8_t irq_to_pin(uint32_t irq)
{
	if (irq_is_gsi(irq))
		return gsi_table[irq].pin;
	else
		return IOAPIC_INVALID_PIN;
}

uint32_t pin_to_irq(uint8_t pin)
{
	uint32_t i;

	for (i = 0U; i < nr_gsi; i++) {
		if (gsi_table[i].pin == pin)
			return i;
	}
	return IRQ_INVALID;
}

void
irq_gsi_mask_unmask(uint32_t irq, bool mask)
{
	void *addr = gsi_table[irq].addr;
	uint8_t pin = gsi_table[irq].pin;
	struct ioapic_rte rte;

	if (!irq_is_gsi(irq))
		return;

	ioapic_get_rte_entry(addr, pin, &rte);
	if (mask)
		rte.lo_32 |= IOAPIC_RTE_INTMSET;
	else
		rte.lo_32 &= ~IOAPIC_RTE_INTMASK;
	ioapic_set_rte_entry(addr, pin, &rte);
	dev_dbg(ACRN_DBG_PTIRQ, "update: irq:%d pin:%hhu rte:%x",
		irq, pin, rte.lo_32);
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

void setup_ioapic_irq(void)
{
	uint8_t ioapic_id;
	uint32_t gsi = 0U;
	uint32_t vr;

	spinlock_init(&ioapic_lock);

	for (ioapic_id = 0U;
	     ioapic_id < CONFIG_NR_IOAPICS; ioapic_id++) {
		void *addr;
		uint8_t pin, nr_pins;

		addr = map_ioapic(get_ioapic_base(ioapic_id));
		nr_pins = ioapic_nr_pins(addr);
		for (pin = 0U; pin < nr_pins; pin++) {
			gsi_table[gsi].ioapic_id = ioapic_id;
			gsi_table[gsi].addr = addr;

			if (gsi < NR_LEGACY_IRQ)
				gsi_table[gsi].pin =
					legacy_irq_to_pin[gsi] & 0xffU;
			else
				gsi_table[gsi].pin = pin;

			/* pinned irq before use it */
			if (irq_mark_used(gsi) > NR_MAX_IRQS) {
				pr_err("failed to alloc IRQ[%d]", gsi);
				gsi++;
				continue;
			}

			/* assign vector for this GSI
			 * for legacy irq, reserved vector and never free
			 */
			if (gsi < NR_LEGACY_IRQ) {
				vr = irq_desc_alloc_vector(gsi, false);
				if (vr > NR_MAX_VECTOR) {
					pr_err("failed to alloc VR");
					gsi++;
					continue;
				}
			} else
				vr = 0U; /* not to allocate VR right now */

			ioapic_set_routing(gsi, vr);
			gsi++;
		}
	}

	/* system max gsi numbers */
	nr_gsi = gsi;
	ASSERT(nr_gsi <= NR_MAX_GSI, "GSI table overflow");
}

void dump_ioapic(void)
{
	uint32_t irq;

	for (irq = 0U; irq < nr_gsi; irq++) {
		void *addr = gsi_table[irq].addr;
		uint8_t pin = gsi_table[irq].pin;
		struct ioapic_rte rte;

		ioapic_get_rte_entry(addr, pin, &rte);
		dev_dbg(ACRN_DBG_IRQ, "DUMP: irq:%d pin:%hhu rte:%x",
			irq, pin, rte.lo_32);
	}
}

void suspend_ioapic(void)
{
	uint8_t ioapic_id, ioapic_pin;

	for (ioapic_id = 0U; ioapic_id < CONFIG_NR_IOAPICS; ioapic_id++) {
		void *addr;
		uint8_t nr_pins;

		addr = map_ioapic(get_ioapic_base(ioapic_id));
		nr_pins = ioapic_nr_pins(addr);
		for (ioapic_pin = 0U; ioapic_pin < nr_pins; ioapic_pin++)
			ioapic_get_rte_entry(addr, ioapic_pin,
				&saved_rte[ioapic_id][ioapic_pin]);
	}
}

void resume_ioapic(void)
{
	uint8_t ioapic_id, ioapic_pin;

	for (ioapic_id = 0U; ioapic_id < CONFIG_NR_IOAPICS; ioapic_id++) {
		void *addr;
		uint8_t nr_pins;

		addr = map_ioapic(get_ioapic_base(ioapic_id));
		nr_pins = ioapic_nr_pins(addr);
		for (ioapic_pin = 0U; ioapic_pin < nr_pins; ioapic_pin++)
			ioapic_set_rte_entry(addr, ioapic_pin,
				&saved_rte[ioapic_id][ioapic_pin]);
	}
}

#ifdef HV_DEBUG
void get_rte_info(struct ioapic_rte *rte, bool *mask, bool *irr,
	bool *phys, int *delmode, bool *level, int *vector, uint32_t *dest)
{
	*mask = ((rte->lo_32 & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET);
	*irr = ((rte->lo_32 & IOAPIC_RTE_REM_IRR) == IOAPIC_RTE_REM_IRR);
	*phys = ((rte->lo_32 & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
	*delmode = rte->lo_32 & IOAPIC_RTE_DELMOD;
	*level = ((rte->lo_32 & IOAPIC_RTE_TRGRLVL) != 0U) ? true : false;
	*vector = rte->lo_32 & IOAPIC_RTE_INTVEC;
	*dest = rte->hi_32 >> APIC_ID_SHIFT;
}

int get_ioapic_info(char *str, int str_max_len)
{
	uint32_t irq;
	int len, size = str_max_len;

	len = snprintf(str, size,
	"\r\nIRQ\tPIN\tRTE.HI32\tRTE.LO32\tVEC\tDST\tDM\tTM\tDELM\tIRR\tMASK");
	size -= len;
	str += len;

	for (irq = 0U; irq < nr_gsi; irq++) {
		void *addr = gsi_table[irq].addr;
		uint8_t pin = gsi_table[irq].pin;
		struct ioapic_rte rte;

		bool irr, phys, level, mask;
		int delmode, vector;
		uint32_t dest;

		ioapic_get_rte_entry(addr, pin, &rte);

		get_rte_info(&rte, &mask, &irr, &phys, &delmode, &level,
			&vector, &dest);

		len = snprintf(str, size, "\r\n%03d\t%03hhu\t0x%08X\t0x%08X\t",
			irq, pin, rte.hi_32, rte.lo_32);
		size -= len;
		str += len;

		len = snprintf(str, size, "0x%02X\t0x%02X\t%s\t%s\t%d\t%d\t%d",
			vector, dest, phys ? "phys" : "logic",
			level ? "level" : "edge", delmode >> 8, irr, mask);
		size -= len;
		str += len;

		if (size < 2) {
			pr_err("\r\nsmall buffer for ioapic dump");
			return -1;
		}
	}

	snprintf(str, size, "\r\n");
	return 0;
}
#endif /* HV_DEBUG */
