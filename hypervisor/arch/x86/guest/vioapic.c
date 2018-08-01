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

#define REDIR_ENTRIES_HW	120U /* SOS align with native ioapic */
#define	RTBL_RO_BITS	(uint32_t)(IOAPIC_RTE_REM_IRR | IOAPIC_RTE_DELIVS)
#define NEED_TMR_UPDATE (~(IOAPIC_RTE_INTMASK | IOAPIC_RTE_INTPOL))

#define ACRN_DBG_IOAPIC	6U

struct vioapic {
	struct vm	*vm;
	spinlock_t	mtx;
	uint32_t	id;
	uint32_t	ioregsel;
	union ioapic_rte rtbl[REDIR_ENTRIES_HW];
	/* sum of pin asserts (+1) and deasserts (-1) */
	int32_t acnt[REDIR_ENTRIES_HW];
};

#define	VIOAPIC_LOCK(vioapic)	spinlock_obtain(&((vioapic)->mtx))
#define	VIOAPIC_UNLOCK(vioapic)	 spinlock_release(&((vioapic)->mtx))

#define MASK_ALL_INTERRUPTS   0x0001000000010000UL
#define IOAPIC_RTE_LOW_INTVEC    ((uint32_t)IOAPIC_RTE_INTVEC)

static inline const char *pinstate_str(bool asserted)
{
	return (asserted) ? "asserted" : "deasserted";
}

static struct vioapic *
vm_ioapic(struct vm *vm)
{
	return (struct vioapic *)vm->arch_vm.virt_ioapic;
}

static void
vioapic_send_intr(struct vioapic *vioapic, uint8_t pin)
{
	uint32_t vector, dest, delmode;
	union ioapic_rte rte;
	bool level, phys;
	uint8_t pincount = vioapic_pincount(vioapic->vm);

	if (pin >= pincount) {
		pr_err("vioapic_send_intr: invalid pin number %hhu", pin);
	}

	rte = vioapic->rtbl[pin];

	if ((rte.full & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET) {
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%hhu: masked", pin);
		return;
	}

	phys = ((rte.full & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
	delmode = (uint32_t)(rte.full & IOAPIC_RTE_DELMOD);
	level = ((rte.full & IOAPIC_RTE_TRGRLVL) != 0UL);
	if (level) {
		vioapic->rtbl[pin].full |= IOAPIC_RTE_REM_IRR;
	}

	vector = rte.u.lo_32 & IOAPIC_RTE_LOW_INTVEC;
	dest = (uint32_t)(rte.full >> IOAPIC_RTE_DEST_SHIFT);
	vlapic_deliver_intr(vioapic->vm, level, dest, phys, delmode, vector, false);
}

static void
vioapic_set_pinstate(struct vioapic *vioapic, uint8_t pin, bool newstate)
{
	int oldcnt, newcnt;
	bool needintr;
	uint8_t pincount = vioapic_pincount(vioapic->vm);

	if (pin >= pincount) {
		pr_err("vioapic_set_pinstate: invalid pin number %hhu", pin);
	}

	oldcnt = vioapic->acnt[pin];
	if (newstate) {
		vioapic->acnt[pin]++;
	} else {
		vioapic->acnt[pin]--;
	}
	newcnt = vioapic->acnt[pin];

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

	if (needintr) {
		vioapic_send_intr(vioapic, pin);
	}
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

	if (pin >= vioapic_pincount(vm)) {
		return -EINVAL;
	}

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
	struct acrn_vlapic *vlapic;
	union ioapic_rte rte;
	uint32_t vector, delmode;
	bool level;
	uint8_t pin, pincount;

	vlapic = vcpu->arch_vcpu.vlapic;
	vioapic = vm_ioapic(vcpu->vm);

	VIOAPIC_LOCK(vioapic);
	pincount = vioapic_pincount(vcpu->vm);
	for (pin = 0U; pin < pincount; pin++) {
		rte = vioapic->rtbl[pin];

		level = ((rte.full & IOAPIC_RTE_TRGRLVL) != 0UL);

		/*
		 * For a level-triggered 'pin' let the vlapic figure out if
		 * an assertion on this 'pin' would result in an interrupt
		 * being delivered to it. If yes, then it will modify the
		 * TMR bit associated with this vector to level-triggered.
		 */
		delmode = (uint32_t)(rte.full & IOAPIC_RTE_DELMOD);
		vector = rte.u.lo_32 & IOAPIC_RTE_LOW_INTVEC;
		vlapic_set_tmr_one_vec(vlapic, delmode, vector, level);
	}
	vlapic_apicv_batch_set_tmr(vlapic);
	VIOAPIC_UNLOCK(vioapic);
}

static uint32_t
vioapic_indirect_read(struct vioapic *vioapic, uint32_t addr)
{
	uint32_t regnum;
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
	if ((regnum >= IOAPIC_REDTBL) &&
	    (regnum < (IOAPIC_REDTBL + ((uint32_t)pincount * 2U)))) {
		uint32_t addr_offset = regnum - IOAPIC_REDTBL;
		uint32_t rte_offset = addr_offset / 2U;
		pin = (uint8_t)rte_offset;
		if ((addr_offset % 2U) != 0U) {
			return vioapic->rtbl[pin].u.hi_32;
		} else {
			return vioapic->rtbl[pin].u.lo_32;
		}
	}

	return 0;
}

/*
 * version 0x20+ ioapic has EOI register. And cpu could write vector to this
 * register to clear related IRR.
 * Due to the race between vcpus, ensure to do VIOAPIC_LOCK(vioapic) &
 * VIOAPIC_UNLOCK(vioapic) by caller.
 */
static void
vioapic_write_eoi(struct vioapic *vioapic, uint32_t vector)
{
	struct vm *vm = vioapic->vm;
	union ioapic_rte rte;
	uint8_t pin, pincount;

	if (vector < VECTOR_FOR_INTR_START || vector > NR_MAX_VECTOR) {
		pr_err("vioapic_process_eoi: invalid vector %u", vector);
	}

	pincount = vioapic_pincount(vm);
	for (pin = 0U; pin < pincount; pin++) {
		rte = vioapic->rtbl[pin];

		if ((rte.full & IOAPIC_RTE_REM_IRR) == 0UL) {
			continue;
		}
		if ((rte.u.lo_32 & IOAPIC_RTE_LOW_INTVEC) != vector) {
			continue;
		}

		vioapic->rtbl[pin].full &= ~IOAPIC_RTE_REM_IRR;
		if (vioapic->acnt[pin] > 0) {
			dev_dbg(ACRN_DBG_IOAPIC,
				"ioapic pin%hhu: asserted at eoi, acnt %d",
				pin, vioapic->acnt[pin]);
			vioapic_send_intr(vioapic, pin);
		}
	}
}

/* Due to the race between vcpus, ensure to do VIOAPIC_LOCK(vioapic) &
 * VIOAPIC_UNLOCK(vioapic) by caller.
 */
static void
vioapic_indirect_write(struct vioapic *vioapic, uint32_t addr, uint32_t data)
{
	union ioapic_rte last, new;
	uint64_t changed;
	uint32_t regnum;
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
	if ((regnum >= IOAPIC_REDTBL) &&
	    (regnum < (IOAPIC_REDTBL + ((uint32_t)pincount * 2U)))) {
		uint32_t addr_offset = regnum - IOAPIC_REDTBL;
		uint32_t rte_offset = addr_offset / 2U;
		pin = (uint8_t)rte_offset;

		last = vioapic->rtbl[pin];
		new = last;
		if ((addr_offset % 2U) != 0U) {
			new.u.hi_32 = data;
		} else {
			new.u.lo_32 &= RTBL_RO_BITS;
			new.u.lo_32 |= (data & ~RTBL_RO_BITS);
		}

		changed = last.full ^ new.full;
		/* pin0 from vpic mask/unmask */
		if (pin == 0U && (changed & IOAPIC_RTE_INTMASK) != 0UL) {
			/* mask -> umask */
			if ((last.full & IOAPIC_RTE_INTMASK) != 0UL &&
				((new.full & IOAPIC_RTE_INTMASK) == 0UL)) {
				if ((vioapic->vm->wire_mode ==
						VPIC_WIRE_NULL) ||
						(vioapic->vm->wire_mode ==
						VPIC_WIRE_INTR)) {
					vioapic->vm->wire_mode =
						VPIC_WIRE_IOAPIC;
					dev_dbg(ACRN_DBG_IOAPIC,
						"vpic wire mode -> IOAPIC");
				} else {
					pr_err("WARNING: invalid vpic wire mode change");
					return;
				}
			/* unmask -> mask */
			} else if (((last.full & IOAPIC_RTE_INTMASK) == 0UL) &&
				(new.full & IOAPIC_RTE_INTMASK) != 0UL) {
				if (vioapic->vm->wire_mode ==
						VPIC_WIRE_IOAPIC) {
					vioapic->vm->wire_mode =
						VPIC_WIRE_INTR;
					dev_dbg(ACRN_DBG_IOAPIC,
						"vpic wire mode -> INTR");
				}
			} else {
				/* Can never happen since IOAPIC_RTE_INTMASK
				 * is changed. */
			}
		}
		vioapic->rtbl[pin] = new;
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%hhu: redir table entry %#lx",
		    pin, vioapic->rtbl[pin].full);
		/*
		 * If any fields in the redirection table entry (except mask
		 * or polarity) have changed then rendezvous all the vcpus
		 * to update their vlapic trigger-mode registers.
		 */
		if ((changed & NEED_TMR_UPDATE) != 0UL) {
			uint16_t i;
			struct vcpu *vcpu;

			dev_dbg(ACRN_DBG_IOAPIC,
			"ioapic pin%hhu: recalculate vlapic trigger-mode reg",
			pin);

			foreach_vcpu(i, vioapic->vm, vcpu) {
				vcpu_make_request(vcpu, ACRN_REQUEST_TMR_UPDATE);
			}
		}

		/*
		 * Generate an interrupt if the following conditions are met:
		 * - pin is not masked
		 * - previous interrupt has been EOIed
		 * - pin level is asserted
		 */
		if ((vioapic->rtbl[pin].full & IOAPIC_RTE_INTMASK) ==
			IOAPIC_RTE_INTMCLR &&
			(vioapic->rtbl[pin].full & IOAPIC_RTE_REM_IRR) == 0UL &&
			(vioapic->acnt[pin] > 0)) {
			dev_dbg(ACRN_DBG_IOAPIC,
				"ioapic pin%hhu: asserted at rtbl write, acnt %d",
				pin, vioapic->acnt[pin]);
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

static void
vioapic_mmio_rw(struct vioapic *vioapic, uint64_t gpa,
		uint32_t *data, bool doread)
{
	uint32_t offset;

	offset = (uint32_t)(gpa - VIOAPIC_BASE);

	/*
	 * The IOAPIC specification allows 32-bit wide accesses to the
	 * IOAPIC_REGSEL (offset 0) and IOAPIC_WINDOW (offset 16) registers.
	 */
	if (offset != IOAPIC_REGSEL &&
		offset != IOAPIC_WINDOW &&
		offset != IOAPIC_EOIR) {
		if (doread) {
			*data = 0U;
		}
	}

	VIOAPIC_LOCK(vioapic);
	if (offset == IOAPIC_REGSEL) {
		if (doread) {
			*data = vioapic->ioregsel;
		} else {
			vioapic->ioregsel = *data;
		}
	} else if (offset == IOAPIC_EOIR) {
		/* only need to handle write operation */
		if (!doread) {
			vioapic_write_eoi(vioapic, *data);
		}
	} else {
		if (doread) {
			*data = vioapic_indirect_read(vioapic,
							vioapic->ioregsel);
		} else {
			vioapic_indirect_write(vioapic, vioapic->ioregsel,
			    *data);
		}
	}
	VIOAPIC_UNLOCK(vioapic);
}

void
vioapic_process_eoi(struct vm *vm, uint32_t vector)
{
	struct vioapic *vioapic;
	uint8_t pin, pincount = vioapic_pincount(vm);
	union ioapic_rte rte;

	if (vector < VECTOR_FOR_INTR_START || vector > NR_MAX_VECTOR) {
		pr_err("vioapic_process_eoi: invalid vector %u", vector);
	}

	vioapic = vm_ioapic(vm);
	dev_dbg(ACRN_DBG_IOAPIC, "ioapic processing eoi for vector %u", vector);

	/* notify device to ack if assigned pin */
	for (pin = 0U; pin < pincount; pin++) {
		rte = vioapic->rtbl[pin];
		if ((rte.full & IOAPIC_RTE_REM_IRR) == 0UL) {
			continue;
		}
		if ((rte.u.lo_32 & IOAPIC_RTE_LOW_INTVEC) != vector) {
			continue;
		}
		ptdev_intx_ack(vm, pin, PTDEV_VPIN_IOAPIC);
	}

	/*
	 * XXX keep track of the pins associated with this vector instead
	 * of iterating on every single pin each time.
	 */
	VIOAPIC_LOCK(vioapic);
	for (pin = 0U; pin < pincount; pin++) {
		rte = vioapic->rtbl[pin];
		if ((rte.full & IOAPIC_RTE_REM_IRR) == 0UL) {
			continue;
		}
		if ((rte.u.lo_32 & IOAPIC_RTE_LOW_INTVEC) != vector) {
			continue;
		}

		vioapic->rtbl[pin].full &= (~IOAPIC_RTE_REM_IRR);
		if (vioapic->acnt[pin] > 0) {
			dev_dbg(ACRN_DBG_IOAPIC,
				"ioapic pin%hhu: asserted at eoi, acnt %d",
				pin, vioapic->acnt[pin]);
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
	for (pin = 0U; pin < pincount; pin++) {
		vioapic->rtbl[pin].full = MASK_ALL_INTERRUPTS;
	}
}

struct vioapic *
vioapic_init(struct vm *vm)
{
	struct vioapic *vioapic;

	vioapic = calloc(1U, sizeof(struct vioapic));
	ASSERT(vioapic != NULL, "");

	vioapic->vm = vm;
	spinlock_init(&vioapic->mtx);

	vioapic_reset(vioapic);

	register_mmio_emulation_handler(vm,
			vioapic_mmio_access_handler,
			(uint64_t)VIOAPIC_BASE,
			(uint64_t)VIOAPIC_BASE + VIOAPIC_SIZE,
			NULL);

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
	if (is_vm0(vm)) {
		return REDIR_ENTRIES_HW;
	} else {
		return VIOAPIC_RTE_NUM;
	}
}

int vioapic_mmio_access_handler(struct vcpu *vcpu, struct io_request *io_req,
		__unused void *handler_private_data)
{
	struct vm *vm = vcpu->vm;
	struct vioapic *vioapic;
	struct mmio_request *mmio = &io_req->reqs.mmio;
	uint64_t gpa = mmio->address;
	int ret = 0;

	vioapic = vm_ioapic(vm);

	/* Note all RW to IOAPIC are 32-Bit in size */
	if (mmio->size == 4UL) {
		uint32_t data = (uint32_t)mmio->value;

		if (mmio->direction == REQUEST_READ) {
			vioapic_mmio_rw(vioapic, gpa, &data, true);
			mmio->value = (uint64_t)data;

		} else if (mmio->direction == REQUEST_WRITE) {
			vioapic_mmio_rw(vioapic, gpa, &data, false);

		} else
			ret = -EINVAL;
	} else {
		pr_err("All RW to IOAPIC must be 32-bits in size");
		ret = -EINVAL;
	}

	io_req->processed = REQ_STATE_COMPLETE;
	return ret;
}

/**
 * @pre vm->arch_vm.virt_ioapic != NULL
 * @pre rte != NULL
 */
void vioapic_get_rte(struct vm *vm, uint8_t pin, union ioapic_rte *rte)
{
	struct vioapic *vioapic;

	vioapic = vm_ioapic(vm);
	*rte = vioapic->rtbl[pin];
}

#ifdef HV_DEBUG
void get_vioapic_info(char *str_arg, int str_max, uint16_t vmid)
{
	char *str = str_arg;
	int len, size = str_max;
	union ioapic_rte rte;
	uint32_t delmode, vector, dest;
	bool level, phys, remote_irr, mask;
	struct vm *vm = get_vm_from_vmid(vmid);
	uint8_t pin, pincount;

	if (vm == NULL) {
		len = snprintf(str, size,
			"\r\nvm is not exist for vmid %hu", vmid);
		size -= len;
		str += len;
		goto END;
	}

	len = snprintf(str, size,
		"\r\nPIN\tVEC\tDM\tDEST\tTM\tDELM\tIRR\tMASK");
	size -= len;
	str += len;

	pincount = vioapic_pincount(vm);
	rte.full = 0UL;
	for (pin = 0U; pin < pincount; pin++) {
		vioapic_get_rte(vm, pin, &rte);
		mask = ((rte.full & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET);
		remote_irr = ((rte.full & IOAPIC_RTE_REM_IRR) == IOAPIC_RTE_REM_IRR);
		phys = ((rte.full & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
		delmode = (uint32_t)(rte.full & IOAPIC_RTE_DELMOD);
		level = ((rte.full & IOAPIC_RTE_TRGRLVL) != 0UL);
		vector = rte.u.lo_32 & IOAPIC_RTE_LOW_INTVEC;
		dest = (uint32_t)(rte.full >> IOAPIC_RTE_DEST_SHIFT);

		len = snprintf(str, size,
				"\r\n%hhu\t0x%X\t%s\t0x%X\t%s\t%u\t%d\t%d",
				pin, vector, phys ? "phys" : "logic",
				dest, level ? "level" : "edge",
				delmode >> 8U, remote_irr, mask);
		size -= len;
		str += len;
	}
END:
	snprintf(str, size, "\r\n");
}
#endif /* HV_DEBUG */
