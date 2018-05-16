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

#define pr_fmt(fmt)	"vioapic: " fmt

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>

#define	IOREGSEL	0x00
#define	IOWIN		0x10
#define	IOEOI		0x40

#define REDIR_ENTRIES_HW	120 /* SOS align with native ioapic */
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
vioapic_send_intr(struct vioapic *vioapic, int pin)
{
	int vector, delmode;
	uint32_t low, high, dest;
	bool level, phys;

	if (pin < 0 || pin >= vioapic_pincount(vioapic->vm))
		pr_err("vioapic_send_intr: invalid pin number %d", pin);

	low = vioapic->rtbl[pin].reg;
	high = vioapic->rtbl[pin].reg >> 32;

	if ((low & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET) {
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%d: masked", pin);
		return;
	}

	phys = ((low & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
	delmode = low & IOAPIC_RTE_DELMOD;
	level = low & IOAPIC_RTE_TRGRLVL ? true : false;
	if (level)
		vioapic->rtbl[pin].reg |= IOAPIC_RTE_REM_IRR;

	vector = low & IOAPIC_RTE_INTVEC;
	dest = high >> APIC_ID_SHIFT;
	vlapic_deliver_intr(vioapic->vm, level, dest, phys, delmode, vector);
}

static void
vioapic_set_pinstate(struct vioapic *vioapic, int pin, bool newstate)
{
	int oldcnt, newcnt;
	bool needintr;

	if (pin < 0 || pin >= vioapic_pincount(vioapic->vm))
		pr_err("vioapic_set_pinstate: invalid pin number %d", pin);

	oldcnt = vioapic->rtbl[pin].acnt;
	if (newstate)
		vioapic->rtbl[pin].acnt++;
	else
		vioapic->rtbl[pin].acnt--;
	newcnt = vioapic->rtbl[pin].acnt;

	if (newcnt < 0) {
		pr_err("ioapic pin%d: bad acnt %d", pin, newcnt);
	}

	needintr = false;
	if (oldcnt == 0 && newcnt == 1) {
		needintr = true;
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%d: asserted", pin);
	} else if (oldcnt == 1 && newcnt == 0) {
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%d: deasserted", pin);
	} else {
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%d: %s, ignored, acnt %d",
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
vioapic_set_irqstate(struct vm *vm, int irq, enum irqstate irqstate)
{
	struct vioapic *vioapic;

	if (irq < 0 || irq >= vioapic_pincount(vm))
		return -EINVAL;

	vioapic = vm_ioapic(vm);

	VIOAPIC_LOCK(vioapic);
	switch (irqstate) {
	case IRQSTATE_ASSERT:
		vioapic_set_pinstate(vioapic, irq, true);
		break;
	case IRQSTATE_DEASSERT:
		vioapic_set_pinstate(vioapic, irq, false);
		break;
	case IRQSTATE_PULSE:
		vioapic_set_pinstate(vioapic, irq, true);
		vioapic_set_pinstate(vioapic, irq, false);
		break;
	default:
		panic("vioapic_set_irqstate: invalid irqstate %d", irqstate);
	}
	VIOAPIC_UNLOCK(vioapic);

	return 0;
}

int
vioapic_assert_irq(struct vm *vm, int irq)
{
	return vioapic_set_irqstate(vm, irq, IRQSTATE_ASSERT);
}

int
vioapic_deassert_irq(struct vm *vm, int irq)
{
	return vioapic_set_irqstate(vm, irq, IRQSTATE_DEASSERT);
}

int
vioapic_pulse_irq(struct vm *vm, int irq)
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
	uint32_t low;
	int delmode, pin, vector;
	bool level;

	vlapic = vcpu->arch_vcpu.vlapic;
	vioapic = vm_ioapic(vcpu->vm);

	VIOAPIC_LOCK(vioapic);
	for (pin = 0; pin < vioapic_pincount(vioapic->vm); pin++) {
		low = vioapic->rtbl[pin].reg;

		level = low & IOAPIC_RTE_TRGRLVL ? true : false;

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
	int regnum, pin, rshift;

	regnum = addr & 0xff;
	switch (regnum) {
	case IOAPIC_ID:
		return vioapic->id;
	case IOAPIC_VER:
		return ((vioapic_pincount(vioapic->vm) - 1) << MAX_RTE_SHIFT)
		       | 0x11;
	case IOAPIC_ARB:
		return vioapic->id;
	default:
		break;
	}

	/* redirection table entries */
	if (regnum >= IOAPIC_REDTBL &&
	    regnum < IOAPIC_REDTBL + vioapic_pincount(vioapic->vm) * 2) {
		pin = (regnum - IOAPIC_REDTBL) / 2;
		if ((regnum - IOAPIC_REDTBL) % 2)
			rshift = 32;
		else
			rshift = 0;

		return vioapic->rtbl[pin].reg >> rshift;
	}

	return 0;
}

/*
 * version 0x20+ ioapic has EOI register. And cpu could write vector to this
 * register to clear related IRR.
 */
static void
vioapic_write_eoi(struct vioapic *vioapic, int32_t vector)
{
	struct vm *vm = vioapic->vm;
	int pin;

	if (vector < VECTOR_FOR_INTR_START || vector > NR_MAX_VECTOR)
		pr_err("vioapic_process_eoi: invalid vector %d", vector);

	VIOAPIC_LOCK(vioapic);
	for (pin = 0; pin < vioapic_pincount(vm); pin++) {
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_REM_IRR) == 0)
			continue;
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_INTVEC) !=
				(uint64_t)vector)
			continue;

		vioapic->rtbl[pin].reg &= ~IOAPIC_RTE_REM_IRR;
		if (vioapic->rtbl[pin].acnt > 0) {
			dev_dbg(ACRN_DBG_IOAPIC,
				"ioapic pin%d: asserted at eoi, acnt %d",
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
	int regnum, pin, lshift;

	regnum = addr & 0xff;
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
	    regnum < IOAPIC_REDTBL + vioapic_pincount(vioapic->vm) * 2) {
		pin = (regnum - IOAPIC_REDTBL) / 2;
		if ((regnum - IOAPIC_REDTBL) % 2)
			lshift = 32;
		else
			lshift = 0;

		last = new = vioapic->rtbl[pin].reg;

		data64 = (uint64_t)data << lshift;
		mask64 = (uint64_t)0xffffffff << lshift;
		new &= ~mask64 | RTBL_RO_BITS;
		new |= data64 & ~RTBL_RO_BITS;

		changed = last ^ new;
		/* pin0 from vpic mask/unmask */
		if (pin == 0 && (changed & IOAPIC_RTE_INTMASK)) {
			/* mask -> umask */
			if ((last & IOAPIC_RTE_INTMASK) &&
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
				(new & IOAPIC_RTE_INTMASK)) {
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
		dev_dbg(ACRN_DBG_IOAPIC, "ioapic pin%d: redir table entry %#lx",
		    pin, vioapic->rtbl[pin].reg);
		/*
		 * If any fields in the redirection table entry (except mask
		 * or polarity) have changed then rendezvous all the vcpus
		 * to update their vlapic trigger-mode registers.
		 */
		if (changed & ~(IOAPIC_RTE_INTMASK | IOAPIC_RTE_INTPOL)) {
			int i;
			struct vcpu *vcpu;

			dev_dbg(ACRN_DBG_IOAPIC,
			"ioapic pin%d: recalculate vlapic trigger-mode reg",
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
				"ioapic pin%d: asserted at rtbl write, acnt %d",
				pin, vioapic->rtbl[pin].acnt);
			vioapic_send_intr(vioapic, pin);
		}

		/* remap for active: interrupt mask -> unmask
		 * remap for deactive: interrupt mask & vector set to 0
		 */
		data64 = vioapic->rtbl[pin].reg;
		if ((((data64 & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMCLR)
		  && ((last & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET))
		  || (((data64 & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET)
		  && ((vioapic->rtbl[pin].reg & IOAPIC_RTE_INTVEC) == 0))) {
			/* VM enable intr */
			struct ptdev_intx_info intx;

			/* NOTE: only support max 256 pin */
			intx.virt_pin = (uint8_t)pin;
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
			*data = 0;
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
vioapic_process_eoi(struct vm *vm, int vector)
{
	struct vioapic *vioapic;
	int pin;

	if (vector < VECTOR_FOR_INTR_START || vector > NR_MAX_VECTOR)
		pr_err("vioapic_process_eoi: invalid vector %d", vector);

	vioapic = vm_ioapic(vm);
	dev_dbg(ACRN_DBG_IOAPIC, "ioapic processing eoi for vector %d", vector);

	/* notify device to ack if assigned pin */
	for (pin = 0; pin < vioapic_pincount(vm); pin++) {
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
	for (pin = 0; pin < vioapic_pincount(vm); pin++) {
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_REM_IRR) == 0)
			continue;
		if ((vioapic->rtbl[pin].reg & IOAPIC_RTE_INTVEC) !=
				(uint64_t)vector)
			continue;

		vioapic->rtbl[pin].reg &= ~IOAPIC_RTE_REM_IRR;
		if (vioapic->rtbl[pin].acnt > 0) {
			dev_dbg(ACRN_DBG_IOAPIC,
				"ioapic pin%d: asserted at eoi, acnt %d",
				pin, vioapic->rtbl[pin].acnt);
			vioapic_send_intr(vioapic, pin);
		}
	}
	VIOAPIC_UNLOCK(vioapic);
}

struct vioapic *
vioapic_init(struct vm *vm)
{
	int i;
	struct vioapic *vioapic;

	vioapic = calloc(1, sizeof(struct vioapic));
	ASSERT(vioapic != NULL, "");

	vioapic->vm = vm;
	spinlock_init(&vioapic->mtx);

	/* Initialize all redirection entries to mask all interrupts */
	for (i = 0; i < vioapic_pincount(vioapic->vm); i++)
		vioapic->rtbl[i].reg = 0x0001000000010000UL;

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

int
vioapic_pincount(struct vm *vm)
{
	if (is_vm0(vm))
		return REDIR_ENTRIES_HW;
	else
		return VIOAPIC_RTE_NUM;
}

int vioapic_mmio_access_handler(struct vcpu *vcpu, struct mem_io *mmio,
		void *handler_private_data)
{
	struct vm *vm = vcpu->vm;
	uint64_t gpa = mmio->paddr;
	int ret = 0;

	(void)handler_private_data;

	/* Note all RW to IOAPIC are 32-Bit in size */
	ASSERT(mmio->access_size == 4,
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

bool vioapic_get_rte(struct vm *vm, int pin, void *rte)
{
	struct vioapic *vioapic;

	vioapic = vm_ioapic(vm);
	if (vioapic && rte) {
		*(uint64_t *)rte = vioapic->rtbl[pin].reg;
		return true;
	} else
		return false;
}

int get_vioapic_info(char *str, int str_max, int vmid)
{
	int pin, len, size = str_max, vector, delmode;
	uint64_t rte;
	uint32_t low, high, dest;
	bool level, phys, remote_irr, mask;
	struct vm *vm = get_vm_from_vmid(vmid);

	if (!vm) {
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

	rte = 0;
	for (pin = 0 ; pin < vioapic_pincount(vm); pin++) {
		vioapic_get_rte(vm, pin, (void *)&rte);
		low = rte;
		high = rte >> 32;
		mask = ((low & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET);
		remote_irr = ((low & IOAPIC_RTE_REM_IRR) == IOAPIC_RTE_REM_IRR);
		phys = ((low & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
		delmode = low & IOAPIC_RTE_DELMOD;
		level = low & IOAPIC_RTE_TRGRLVL ? true : false;
		vector = low & IOAPIC_RTE_INTVEC;
		dest = high >> APIC_ID_SHIFT;

		len = snprintf(str, size,
				"\r\n%d\t0x%X\t%s\t0x%X\t%s\t%d\t%d\t%d",
				pin, vector, phys ? "phys" : "logic",
				dest, level ? "level" : "edge",
				delmode >> 8, remote_irr, mask);
		size -= len;
		str += len;
	}
END:
	snprintf(str, size, "\r\n");
	return 0;
}
