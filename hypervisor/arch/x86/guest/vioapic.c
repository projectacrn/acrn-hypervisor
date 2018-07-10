/*-
 * Copyright (c) 2013 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#define pr_prefix	"vioapic: "

#include <hypervisor.h>

#define	IOREGSEL	0x00
#define	IOWIN		0x10
#define	IOEOI		0x40

#define REDIR_ENTRIES_HW	120U /* SOS align with native ioapic */
#define	RTBL_RO_BITS	((uint64_t)(IOAPIC_RTE_REM_IRR | IOAPIC_RTE_DELIVS))

#define ACRN_DBG_IOAPIC	6

struct vioapic {
	struct vm	*vm;
	spinlock_t	mtx;
	uint32_t	id;
	uint32_t	ioregsel;
	struct {
		uint64_t reg;
		int	 acnt;	/* sum of pin asserts (+1) and deasserts (-1) */
	} rtbl[REDIR_ENTRIES_HW];
};

#define	VIOAPIC_LOCK(vioapic)	spinlock_obtain(&((vioapic)->mtx))
#define	VIOAPIC_UNLOCK(vioapic)	 spinlock_release(&((vioapic)->mtx))

#define MASK_ALL_INTERRUPTS   0x0001000000010000UL

static inline const char *pinstate_str(bool asserted)
{
	return (asserted) ? "asserted" : "deasserted";
}

struct vioapic *
vm_ioapic(struct vm *vm)
{
	return (struct vioapic *)vm->arch_vm.virt_ioapic;
}

static void
vioapic_send_intr(struct vioapic *vioapic, uint8_t pin)
{
	int delmode;
	uint32_t vector, low, high, dest;
	bool level, phys;
	uint8_t pincount = vioapic_pincount(vioapic->vm);

	if (pin >= pincount)
		pr_err("vioapic_send_intr: invalid pin number %hhu", pin);

	low = vioapic->rtbl[pin].reg;
	high = vioapic->rtbl[pin].reg >> 32;

	if ((low & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET) {
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%hhu: masked", pin);
		return;
	}

	phys = ((low & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
	delmode = low & IOAPIC_RTE_DELMOD;
	level = (low & IOAPIC_RTE_TRGRLVL) != 0U ? true : false;
	if (level)
		vioapic->rtbl[pin].reg |= IOAPIC_RTE_REM_IRR;

	vector = low & IOAPIC_RTE_INTVEC;
	dest = high >> APIC_ID_SHIFT;
	vlapic_deliver_intr(vioapic->vm, level, dest, phys, delmode, vector);
}

static void
vioapic_set_pinstate(struct vioapic *vioapic, uint8_t pin, bool newstate)
{
	int oldcnt, newcnt;
	bool needintr;
	uint8_t pincount = vioapic_pincount(vioapic->vm);

	if (pin >= pincount)
		pr_err("vioapic_set_pinstate: invalid pin number %hhu", pin);

	oldcnt = vioapic->rtbl[pin].acnt;
	if (newstate)
		vioapic->rtbl[pin].acnt++;
	else
		vioapic->rtbl[pin].acnt--;
	newcnt = vioapic->rtbl[pin].acnt;

	if (newcnt < 0) {
		pr_err("ioapic pin%hhu: bad acnt %d", pin, newcnt);
	}

	needintr = false;
	if (oldcnt == 0 && newcnt == 1) {
		needintr = true;
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%hhu: asserted", pin);
	} else if (oldcnt == 1 && newcnt == 0) {
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%hhu: deasserted", pin);
	} else {
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%hhu: %s, ignored, acnt %d",
		    pin, pinstate_str(newstate), newcnt);
	}

	if (needintr)
		vioapic_send_intr(vioapic, pin);
}

enum irqstate {
	IRQSTATE_ASSERT,
	IRQSTATE_DEASSERT,
	IRQSTATE_PULSE
};

static int
vioapic_set_irqstate(struct vm *vm, uint32_t irq, enum irqstate irqstate)
{
	struct vioapic *vioapic;
	uint8_t pin = (uint8_t)irq;

	if (pin >= vioapic_pincount(vm))
		return -EINVAL;

	vioapic = vm_ioapic(vm);

	VIOAPIC_LOCK(vioapic);
	switch (irqstate) {
	case IRQSTATE_ASSERT:
		vioapic_set_pinstate(vioapic, pin, true);
		break;
	case IRQSTATE_DEASSERT:
		vioapic_set_pinstate(vioapic, pin, false);
		break;
	case IRQSTATE_PULSE:
		vioapic_set_pinstate(vioapic, pin, true);
		vioapic_set_pinstate(vioapic, pin, false);
		break;
	default:
		panic("vioapic_set_irqstate: invalid irqstate %d", irqstate);
	}
	VIOAPIC_UNLOCK(vioapic);

	return 0;
}

int
vioapic_assert_irq(struct vm *vm, uint32_t irq)
{
	return vioapic_set_irqstate(vm, irq, IRQSTATE_ASSERT);
}

int
vioapic_deassert_irq(struct vm *vm, uint32_t irq)
{
	return vioapic_set_irqstate(vm, irq, IRQSTATE_DEASSERT);
}

int
vioapic_pulse_irq(struct vm *vm, uint32_t irq)
{
	return vioapic_set_irqstate(vm, irq, IRQSTATE_PULSE);
}

/*
 * Reset the vlapic's trigger-mode register to reflect the ioapic pin
 * configuration.
 */
void
vioapic_update_tmr(struct vcpu *vcpu)
{
	struct vioapic *vioapic;
	struct vlapic *vlapic;
	uint32_t low, vector;
	int delmode;
	bool level;
	uint8_t pin, pincount;

	vlapic = vcpu->arch_vcpu.vlapic;
	vioapic = vm_ioapic(vcpu->vm);

	VIOAPIC_LOCK(vioapic);
	pincount = vioapic_pincount(vcpu->vm);
	for (pin = 0U; pin < pincount; pin++) {
		low = vioapic->rtbl[pin].reg;

		level = (low & IOAPIC_RTE_TRGRLVL) != 0U ? true : false;

		/*
		 * For a level-triggered 'pin' let the vlapic figure out if
		 * an assertion on this 'pin' would result in an interrupt
		 * being delivered to it. If yes, then it will modify the
		 * TMR bit associated with this vector to level-triggered.
		 */
		delmode = low & IOAPIC_RTE_DELMOD;
		vector = low & IOAPIC_RTE_INTVEC;
		vlapic_set_tmr_one_vec(vlapic, delmode, vector, level);
	}
	vlapic_apicv_batch_set_tmr(vlapic);
	VIOAPIC_UNLOCK(vioapic);
}

static uint32_t
vioapic_read(struct vioapic *vioapic, uint32_t addr)
{
	uint32_t regnum, rshift;
	uint8_t pin, pincount = vioapic_pincount(vioapic->vm);

	regnum = addr & 0xffU;
	switch (regnum) {
	case IOAPIC_ID:
		return vioapic->id;
	case IOAPIC_VER:
		return (((uint32_t)pincount - 1U) << MAX_RTE_SHIFT) | 0x11U;
	case IOAPIC_ARB:
		return vioapic->id;
	default:
		break;
	}

	/* redirection table entries */
	if (regnum >= IOAPIC_REDTBL &&
	    (regnum < IOAPIC_REDTBL + (uint32_t)pincount * 2U) != 0) {
		uint32_t addr_offset = regnum - IOAPIC_REDTBL;
		uint32_t rte_offset = addr_offset / 2U;
		pin = (uint8_t)rte_offset;
		if ((addr_offset % 2U) != 0U)
			rshift = 32U;
		else
			rshift = 0U;

		return vioapic->rtbl[pin].reg >> rshift;
	}

	return 0;
}

/*
 * version 0x20+ ioapic has EOI register. And cpu could write vector to this
 * register to clear related IRR.
 */
static void
vioapic_write_eoi(struct vioapic *vioapic, uint32_t vector)
{
	struct vm *vm = vioapic->vm;
	uint8_t pin, pincount;

	if (vector < VECTOR_FOR_INTR_START || vector > NR_MAX_VECTOR)
		pr_err("vioapic_process_eoi: invalid vector %u", vector);

	VIOAPIC_LOCK(vioapic);
	pincount = vioapic_pincount(vm);
	for (pin = 0U; pin < pincount; pin++) {
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_REM_IRR) == 0)
			continue;
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_INTVEC) !=
				(uint64_t)vector)
			continue;

		vioapic->rtbl[pin].reg &= ~IOAPIC_RTE_REM_IRR;
		if (vioapic->rtbl[pin].acnt > 0) {
			dev_dbg(ACRN_DBG_IOAPIC,
				"ioapic pin%hhu: asserted at eoi, acnt %d",
				pin, vioapic->rtbl[pin].acnt);
			vioapic_send_intr(vioapic, pin);
		}
	}
	VIOAPIC_UNLOCK(vioapic);
}

static void
vioapic_write(struct vioapic *vioapic, uint32_t addr, uint32_t data)
{
	uint64_t data64, mask64;
	uint64_t last, new, changed;
	uint32_t regnum, lshift;
	uint8_t pin, pincount = vioapic_pincount(vioapic->vm);

	regnum = addr & 0xffUL;
	switch (regnum) {
	case IOAPIC_ID:
		vioapic->id = data & APIC_ID_MASK;
		break;
	case IOAPIC_VER:
	case IOAPIC_ARB:
		/* readonly */
		break;
	default:
		break;
	}

	/* redirection table entries */
	if (regnum >= IOAPIC_REDTBL &&
	    (regnum < IOAPIC_REDTBL + (uint32_t)pincount * 2U) != 0U) {
		uint32_t addr_offset = regnum - IOAPIC_REDTBL;
		uint32_t rte_offset = addr_offset / 2U;
		pin = (uint8_t)rte_offset;
		if ((addr_offset % 2U) != 0U)
			lshift = 32;
		else
			lshift = 0;

		last = vioapic->rtbl[pin].reg;

		data64 = (uint64_t)data << lshift;
		mask64 = 0xffffffffUL << lshift;
		new = last & (~mask64 | RTBL_RO_BITS);
		new |= data64 & ~RTBL_RO_BITS;

		changed = last ^ new;
		/* pin0 from vpic mask/unmask */
		if (pin == 0U && (changed & IOAPIC_RTE_INTMASK) != 0U) {
			/* mask -> umask */
			if ((last & IOAPIC_RTE_INTMASK) != 0U &&
				((new & IOAPIC_RTE_INTMASK) == 0)) {
				if ((vioapic->vm->vpic_wire_mode ==
						VPIC_WIRE_NULL) ||
						(vioapic->vm->vpic_wire_mode ==
						VPIC_WIRE_INTR)) {
					vioapic->vm->vpic_wire_mode =
						VPIC_WIRE_IOAPIC;
					dev_dbg(ACRN_DBG_IOAPIC,
						"vpic wire mode -> IOAPIC");
				} else {
					pr_err("WARNING: invalid vpic wire mode change");
					return;
				}
			/* unmask -> mask */
			} else if (((last & IOAPIC_RTE_INTMASK) == 0) &&
				(new & IOAPIC_RTE_INTMASK) != 0U) {
				if (vioapic->vm->vpic_wire_mode ==
						VPIC_WIRE_IOAPIC) {
					vioapic->vm->vpic_wire_mode =
						VPIC_WIRE_INTR;
					dev_dbg(ACRN_DBG_IOAPIC,
						"vpic wire mode -> INTR");
				}
			}
		}
		vioapic->rtbl[pin].reg = new;
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%hhu: redir table entry %#lx",
		    pin, vioapic->rtbl[pin].reg);
		/*
		 * If any fields in the redirection table entry (except mask
		 * or polarity) have changed then rendezvous all the vcpus
		 * to update their vlapic trigger-mode registers.
		 */
		if ((changed & ~(IOAPIC_RTE_INTMASK | IOAPIC_RTE_INTPOL)) != 0U) {
			uint16_t i;
			struct vcpu *vcpu;

			dev_dbg(ACRN_DBG_IOAPIC,
			"ioapic pin%hhu: recalculate vlapic trigger-mode reg",
			pin);

			VIOAPIC_UNLOCK(vioapic);

			foreach_vcpu(i, vioapic->vm, vcpu) {
				vcpu_make_request(vcpu, ACRN_REQUEST_TMR_UPDATE);
			}
			VIOAPIC_LOCK(vioapic);
		}

		/*
		 * Generate an interrupt if the following conditions are met:
		 * - pin is not masked
		 * - previous interrupt has been EOIed
		 * - pin level is asserted
		 */
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_INTMASK) ==
			IOAPIC_RTE_INTMCLR &&
			(vioapic->rtbl[pin].reg & IOAPIC_RTE_REM_IRR) == 0 &&
			(vioapic->rtbl[pin].acnt > 0)) {
			dev_dbg(ACRN_DBG_IOAPIC,
				"ioapic pin%hhu: asserted at rtbl write, acnt %d",
				pin, vioapic->rtbl[pin].acnt);
			vioapic_send_intr(vioapic, pin);
		}

		/* remap for active: interrupt mask -> unmask
		 * remap for deactive: interrupt mask & vector set to 0
		 * remap for trigger mode change
		 * remap for polarity change
		 */
		if ( (changed & IOAPIC_RTE_INTMASK) != 0UL ||
		     (changed & IOAPIC_RTE_TRGRMOD) != 0UL ||
		     (changed & IOAPIC_RTE_INTPOL ) != 0UL ) {

			/* VM enable intr */
			struct ptdev_intx_info intx;

			/* NOTE: only support max 256 pin */
			intx.virt_pin = pin;
			intx.vpin_src = PTDEV_VPIN_IOAPIC;
			ptdev_intx_pin_remap(vioapic->vm, &intx);
		}
	}
}

static int
vioapic_mmio_rw(struct vioapic *vioapic, uint64_t gpa,
		uint64_t *data, int size, bool doread)
{
	uint64_t offset;

	offset = gpa - VIOAPIC_BASE;

	/*
	 * The IOAPIC specification allows 32-bit wide accesses to the
	 * IOREGSEL (offset 0) and IOWIN (offset 16) registers.
	 */
	if (size != 4 || (offset != IOREGSEL && offset != IOWIN &&
			offset != IOEOI)) {
		if (doread)
			*data = 0UL;
		return 0;
	}

	VIOAPIC_LOCK(vioapic);
	if (offset == IOREGSEL) {
		if (doread)
			*data = vioapic->ioregsel;
		else
			vioapic->ioregsel = *data;
	} else if (offset == IOEOI) {
		/* only need to handle write operation */
		if (!doread)
			vioapic_write_eoi(vioapic, *data);
	} else {
		if (doread) {
			*data = vioapic_read(vioapic, vioapic->ioregsel);
		} else {
			vioapic_write(vioapic, vioapic->ioregsel,
			    *data);
		}
	}
	VIOAPIC_UNLOCK(vioapic);

	return 0;
}

int
vioapic_mmio_read(void *vm, uint64_t gpa, uint64_t *rval,
		int size)
{
	int error;
	struct vioapic *vioapic;

	vioapic = vm_ioapic(vm);
	error = vioapic_mmio_rw(vioapic, gpa, rval, size, true);
	return error;
}

int
vioapic_mmio_write(void *vm, uint64_t gpa, uint64_t wval,
		int size)
{
	int error;
	struct vioapic *vioapic;

	vioapic = vm_ioapic(vm);
	error = vioapic_mmio_rw(vioapic, gpa, &wval, size, false);
	return error;
}

void
vioapic_process_eoi(struct vm *vm, uint32_t vector)
{
	struct vioapic *vioapic;
	uint8_t pin, pincount = vioapic_pincount(vm);

	if (vector < VECTOR_FOR_INTR_START || vector > NR_MAX_VECTOR)
		pr_err("vioapic_process_eoi: invalid vector %u", vector);

	vioapic = vm_ioapic(vm);
	dev_dbg(ACRN_DBG_IOAPIC, "ioapic processing eoi for vector %u", vector);

	/* notify device to ack if assigned pin */
	for (pin = 0U; pin < pincount; pin++) {
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_REM_IRR) == 0)
			continue;
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_INTVEC) !=
				(uint64_t)vector)
			continue;
		ptdev_intx_ack(vm, pin, PTDEV_VPIN_IOAPIC);
	}

	/*
	 * XXX keep track of the pins associated with this vector instead
	 * of iterating on every single pin each time.
	 */
	VIOAPIC_LOCK(vioapic);
	for (pin = 0U; pin < pincount; pin++) {
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_REM_IRR) == 0)
			continue;
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_INTVEC) !=
				(uint64_t)vector)
			continue;

		vioapic->rtbl[pin].reg &= ~IOAPIC_RTE_REM_IRR;
		if (vioapic->rtbl[pin].acnt > 0) {
			dev_dbg(ACRN_DBG_IOAPIC,
				"ioapic pin%hhu: asserted at eoi, acnt %d",
				pin, vioapic->rtbl[pin].acnt);
			vioapic_send_intr(vioapic, pin);
		}
	}
	VIOAPIC_UNLOCK(vioapic);
}

void
vioapic_reset(struct vioapic *vioapic)
{
	uint8_t pin, pincount;

	/* Initialize all redirection entries to mask all interrupts */
	pincount = vioapic_pincount(vioapic->vm);
	for (pin = 0U; pin < pincount; pin++)
		vioapic->rtbl[pin].reg = MASK_ALL_INTERRUPTS;
}

struct vioapic *
vioapic_init(struct vm *vm)
{
	struct vioapic *vioapic;

	vioapic = calloc(1, sizeof(struct vioapic));
	ASSERT(vioapic != NULL, "");

	vioapic->vm = vm;
	spinlock_init(&vioapic->mtx);

	vioapic_reset(vioapic);

	register_mmio_emulation_handler(vm,
			vioapic_mmio_access_handler,
			(uint64_t)VIOAPIC_BASE,
			(uint64_t)VIOAPIC_BASE + VIOAPIC_SIZE,
			(void *) 0);

	return vioapic;
}

void
vioapic_cleanup(struct vioapic *vioapic)
{
	unregister_mmio_emulation_handler(vioapic->vm,
		(uint64_t)VIOAPIC_BASE,
		(uint64_t)VIOAPIC_BASE + VIOAPIC_SIZE);
	free(vioapic);
}

uint8_t
vioapic_pincount(struct vm *vm)
{
	if (is_vm0(vm))
		return REDIR_ENTRIES_HW;
	else
		return VIOAPIC_RTE_NUM;
}

int vioapic_mmio_access_handler(struct vcpu *vcpu, struct mem_io *mmio,
		__unused void *handler_private_data)
{
	struct vm *vm = vcpu->vm;
	uint64_t gpa = mmio->paddr;
	int ret = 0;

	/* Note all RW to IOAPIC are 32-Bit in size */
	ASSERT(mmio->access_size == 4U,
			"All RW to LAPIC must be 32-bits in size");

	if (mmio->read_write == HV_MEM_IO_READ) {
		ret = vioapic_mmio_read(vm,
				gpa,
				&mmio->value,
				mmio->access_size);
		mmio->mmio_status = MMIO_TRANS_VALID;

	} else if (mmio->read_write == HV_MEM_IO_WRITE) {
		ret = vioapic_mmio_write(vm,
				gpa,
				mmio->value,
				mmio->access_size);

		mmio->mmio_status = MMIO_TRANS_VALID;
	}

	return ret;
}

bool vioapic_get_rte(struct vm *vm, uint8_t pin, void *rte)
{
	struct vioapic *vioapic;

	vioapic = vm_ioapic(vm);
	if ((vioapic != NULL) && (rte != NULL)) {
		*(uint64_t *)rte = vioapic->rtbl[pin].reg;
		return true;
	} else
		return false;
}

#ifdef HV_DEBUG
void get_vioapic_info(char *str, int str_max, int vmid)
{
	int len, size = str_max, delmode;
	uint64_t rte;
	uint32_t low, high, vector, dest;
	bool level, phys, remote_irr, mask;
	struct vm *vm = get_vm_from_vmid(vmid);
	uint8_t pin, pincount;

	if (vm == NULL) {
		len = snprintf(str, size,
			"\r\nvm is not exist for vmid %d", vmid);
		size -= len;
		str += len;
		goto END;
	}

	len = snprintf(str, size,
		"\r\nPIN\tVEC\tDM\tDEST\tTM\tDELM\tIRR\tMASK");
	size -= len;
	str += len;

	pincount = vioapic_pincount(vm);
	rte = 0UL;
	for (pin = 0U; pin < pincount; pin++) {
		vioapic_get_rte(vm, pin, (void *)&rte);
		low = rte;
		high = rte >> 32;
		mask = ((low & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET);
		remote_irr = ((low & IOAPIC_RTE_REM_IRR) == IOAPIC_RTE_REM_IRR);
		phys = ((low & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
		delmode = low & IOAPIC_RTE_DELMOD;
		level = ((low & IOAPIC_RTE_TRGRLVL) != 0U) ? true : false;
		vector = low & IOAPIC_RTE_INTVEC;
		dest = high >> APIC_ID_SHIFT;

		len = snprintf(str, size,
				"\r\n%hhu\t0x%X\t%s\t0x%X\t%s\t%d\t%d\t%d",
				pin, vector, phys ? "phys" : "logic",
				dest, level ? "level" : "edge",
				delmode >> 8, remote_irr, mask);
		size -= len;
		str += len;
	}
END:
	snprintf(str, size, "\r\n");
}
#endif /* HV_DEBUG */
