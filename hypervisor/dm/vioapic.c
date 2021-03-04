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

#include <x86/guest/vm.h>
#include <errno.h>
#include <x86/irq.h>
#include <x86/guest/ept.h>
#include <x86/guest/assign.h>
#include <logmsg.h>
#include <x86/ioapic.h>

#define	RTBL_RO_BITS	((uint32_t)0x00004000U | (uint32_t)0x00001000U) /*Remote IRR and Delivery Status bits*/

#define DBG_LEVEL_VIOAPIC	6U
#define ACRN_IOAPIC_VERSION	0x11U

#define MASK_ALL_INTERRUPTS   0x0001000000010000UL

static inline struct acrn_vioapics *vm_ioapics(const struct acrn_vm *vm)
{
	return (struct acrn_vioapics *)&(vm->arch_vm.vioapics);
}

/**
 * @pre pin < vioapic->chipinfo.nr_pins
 */
static void
vioapic_generate_intr(struct acrn_single_vioapic *vioapic, uint32_t pin)
{
	uint32_t vector, dest, delmode;
	union ioapic_rte rte;
	bool level, phys;

	rte = vioapic->rtbl[pin];

	if (rte.bits.intr_mask == IOAPIC_RTE_MASK_SET) {
		dev_dbg(DBG_LEVEL_VIOAPIC, "ioapic pin%hhu: masked", pin);
	} else {
		phys = (rte.bits.dest_mode == IOAPIC_RTE_DESTMODE_PHY);
		delmode = rte.bits.delivery_mode;
		level = (rte.bits.trigger_mode == IOAPIC_RTE_TRGRMODE_LEVEL);

		/* For level trigger irq, avoid send intr if
		 * previous one hasn't received EOI
		 */
		if (!level || (vioapic->rtbl[pin].bits.remote_irr == 0UL)) {
			if (level) {
				vioapic->rtbl[pin].bits.remote_irr = IOAPIC_RTE_REM_IRR;
			}
			vector = rte.bits.vector;
			dest = rte.bits.dest_field;
			vlapic_receive_intr(vioapic->vm, level, dest, phys, delmode, vector, false);
		}
	}
}

/**
 * @pre pin < vioapic->chipinfo.nr_pins
 */
static void
vioapic_set_pinstate(struct acrn_single_vioapic *vioapic, uint32_t pin, uint32_t level)
{
	uint32_t old_lvl;
	union ioapic_rte rte;

	if (pin < vioapic->chipinfo.nr_pins) {
		rte = vioapic->rtbl[pin];
		old_lvl = (uint32_t)bitmap_test((uint16_t)(pin & 0x3FU), &vioapic->pin_state[pin >> 6U]);
		if (level == 0U) {
			/* clear pin_state and deliver interrupt according to polarity */
			bitmap_clear_nolock((uint16_t)(pin & 0x3FU), &vioapic->pin_state[pin >> 6U]);
			if ((rte.bits.intr_polarity == IOAPIC_RTE_INTPOL_ALO)
				&& old_lvl != level) {
				vioapic_generate_intr(vioapic, pin);
			}
		} else {
			/* set pin_state and deliver intrrupt according to polarity */
			bitmap_set_nolock((uint16_t)(pin & 0x3FU), &vioapic->pin_state[pin >> 6U]);
			if ((rte.bits.intr_polarity == IOAPIC_RTE_INTPOL_AHI)
				&& old_lvl != level) {
				vioapic_generate_intr(vioapic, pin);
			}
		}
	}
}


struct acrn_single_vioapic *
vgsi_to_vioapic_and_vpin(const struct acrn_vm *vm, uint32_t vgsi, uint32_t *vpin)
{
	struct acrn_single_vioapic *vioapic;
	uint8_t vioapic_index = 0U;

	if (is_sos_vm(vm)) {
		/*
		 * Utilize platform ioapic_info for SOS VM
		 */	
		vioapic_index = get_gsi_to_ioapic_index(vgsi);
		if (vpin != NULL) {
			*vpin = gsi_to_ioapic_pin(vgsi);
		}
	} else {
		if (vpin != NULL) {
			*vpin = vgsi;
		}
	}
	vioapic = (struct acrn_single_vioapic *)&(vm->arch_vm.vioapics.vioapic_array[vioapic_index]);
	return vioapic;
}

/**
 * @brief Set vIOAPIC IRQ line status.
 *
 * Similar with vioapic_set_irqline_lock(),but would not make sure
 * operation be done with ioapic lock.
 *
 * @param[in] vm        Pointer to target VM
 * @param[in] vgsi   	Target GSI number
 * @param[in] operation Action options: GSI_SET_HIGH/GSI_SET_LOW/
 *			GSI_RAISING_PULSE/GSI_FALLING_PULSE
 *
 * @pre vgsi < get_vm_gsicount(vm)
 * @pre vm != NULL
 * @return None
 */
void
vioapic_set_irqline_nolock(const struct acrn_vm *vm, uint32_t vgsi, uint32_t operation)
{
	struct acrn_single_vioapic *vioapic;
	uint32_t pin;

	vioapic = vgsi_to_vioapic_and_vpin(vm, vgsi, &pin);

	switch (operation) {
	case GSI_SET_HIGH:
		vioapic_set_pinstate(vioapic, pin, 1U);
		break;
	case GSI_SET_LOW:
		vioapic_set_pinstate(vioapic, pin, 0U);
		break;
	case GSI_RAISING_PULSE:
		vioapic_set_pinstate(vioapic, pin, 1U);
		vioapic_set_pinstate(vioapic, pin, 0U);
		break;
	case GSI_FALLING_PULSE:
		vioapic_set_pinstate(vioapic, pin, 0U);
		vioapic_set_pinstate(vioapic, pin, 1U);
		break;
	default:
		/*
		 * The function caller could guarantee the pre condition.
		 */
		break;
	}
}

/**
 * @brief Set vIOAPIC IRQ line status.
 *
 * @param[in] vm        Pointer to target VM
 * @param[in] vgsi  	Target GSI number
 * @param[in] operation Action options: GSI_SET_HIGH/GSI_SET_LOW/
 *			GSI_RAISING_PULSE/GSI_FALLING_PULSE
 *
 * @pre vgsi < get_vm_gsicount(vm)
 * @pre vm != NULL
 *
 * @return None
 */
void
vioapic_set_irqline_lock(const struct acrn_vm *vm, uint32_t vgsi, uint32_t operation)
{
	uint64_t rflags;
	struct acrn_single_vioapic *vioapic;

	vioapic = vgsi_to_vioapic_and_vpin(vm, vgsi, NULL);
	spinlock_irqsave_obtain(&(vioapic->lock), &rflags);
	vioapic_set_irqline_nolock(vm, vgsi, operation);
	spinlock_irqrestore_release(&(vioapic->lock), rflags);
}

static uint32_t
vioapic_indirect_read(const struct acrn_single_vioapic *vioapic, uint32_t addr)
{
	uint32_t regnum, ret = 0U;
	uint32_t pin, pincount = vioapic->chipinfo.nr_pins;

	regnum = addr & 0xffU;
	switch (regnum) {
	case IOAPIC_ID:
		ret = (uint32_t)vioapic->chipinfo.id << IOAPIC_ID_SHIFT;
		break;
	case IOAPIC_VER:
		ret = ((pincount - 1U) << MAX_RTE_SHIFT) | ACRN_IOAPIC_VERSION;
		break;
	case IOAPIC_ARB:
		ret = (uint32_t)vioapic->chipinfo.id << IOAPIC_ID_SHIFT;
		break;
	default:
		/*
		 * In this switch statement, regnum shall either be IOAPIC_ID or
		 * IOAPIC_VER or IOAPIC_ARB.
		 * All the other cases will be handled properly later after this
		 * switch statement.
		 */
		break;
	}

	/* redirection table entries */
	if ((regnum >= IOAPIC_REDTBL) &&
	    (regnum < (IOAPIC_REDTBL + (pincount * 2U)))) {
		uint32_t addr_offset = regnum - IOAPIC_REDTBL;
		uint32_t rte_offset = addr_offset >> 1U;
		pin = rte_offset;
		if ((addr_offset & 0x1U) != 0U) {
			ret = vioapic->rtbl[pin].u.hi_32;
		} else {
			ret = vioapic->rtbl[pin].u.lo_32;
		}
	}

	return ret;
}

static inline bool vioapic_need_intr(const struct acrn_single_vioapic *vioapic, uint16_t pin)
{
	uint32_t lvl;
	union ioapic_rte rte;
	bool ret = false;

	if (pin < vioapic->chipinfo.nr_pins) {
		rte = vioapic->rtbl[pin];
		lvl = (uint32_t)bitmap_test(pin & 0x3FU, &vioapic->pin_state[pin >> 6U]);
		ret = !!(((rte.bits.intr_polarity == IOAPIC_RTE_INTPOL_ALO) && lvl == 0U) ||
			((rte.bits.intr_polarity == IOAPIC_RTE_INTPOL_AHI) && lvl != 0U));
	}

	return ret;
}

/*
 * Due to the race between vcpus and vioapic->lock could be accessed from softirq, ensure to do
 * spinlock_irqsave_obtain(&(vioapic->lock), &rflags) & spinlock_irqrestore_release(&(vioapic->lock), rflags)
 * by caller.
 */
static void vioapic_indirect_write(struct acrn_single_vioapic *vioapic, uint32_t addr, uint32_t data)
{
	union ioapic_rte last, new, changed;
	uint32_t regnum;
	uint32_t pin, pincount = vioapic->chipinfo.nr_pins;

	regnum = addr & 0xffUL;
	switch (regnum) {
	case IOAPIC_ID:
		vioapic->chipinfo.id = (uint8_t)((data & IOAPIC_ID_MASK) >> IOAPIC_ID_SHIFT);
		break;
	case IOAPIC_VER:
	case IOAPIC_ARB:
		/* readonly */
		break;
	default:
		/*
		 * In this switch statement, regnum shall either be IOAPIC_ID or
		 * IOAPIC_VER or IOAPIC_ARB.
		 * All the other cases will be handled properly later after this
		 * switch statement.
		 */
		break;
	}

	/* redirection table entries */
	if ((regnum >= IOAPIC_REDTBL) && (regnum < (IOAPIC_REDTBL + (pincount * 2U)))) {
		bool wire_mode_valid = true;
		uint32_t addr_offset = regnum - IOAPIC_REDTBL;
		uint32_t rte_offset = addr_offset >> 1U;
		pin = rte_offset;

		last = vioapic->rtbl[pin];
		new = last;
		if ((addr_offset & 1U) != 0U) {
			new.u.hi_32 = data;
		} else {
			new.u.lo_32 &= RTBL_RO_BITS;
			new.u.lo_32 |= (data & ~RTBL_RO_BITS);
		}

		/* In some special scenarios, the LAPIC somehow hasn't send
		 * EOI to IOAPIC which cause the Remote IRR bit can't be clear.
		 * To clear it, some OSes will use EOI Register to clear it for
		 * 0x20 version IOAPIC, otherwise use switch Trigger Mode to
		 * Edge Sensitive to clear it.
		 */
		if (new.bits.trigger_mode == IOAPIC_RTE_TRGRMODE_EDGE) {
			new.bits.remote_irr = 0U;
		}

		changed.full = last.full ^ new.full;
		/* pin0 from vpic mask/unmask */
		if ((pin == 0U) && (changed.bits.intr_mask != 0UL)) {
			/* mask -> umask */
			if (last.bits.intr_mask == IOAPIC_RTE_MASK_SET) {
				if ((vioapic->vm->wire_mode == VPIC_WIRE_NULL) ||
						(vioapic->vm->wire_mode == VPIC_WIRE_INTR)) {
					vioapic->vm->wire_mode = VPIC_WIRE_IOAPIC;
					dev_dbg(DBG_LEVEL_VIOAPIC, "vpic wire mode -> IOAPIC");
				} else {
					pr_err("WARNING: invalid vpic wire mode change");
					wire_mode_valid = false;
				}
			/* unmask -> mask */
			} else {
				if (vioapic->vm->wire_mode == VPIC_WIRE_IOAPIC) {
					vioapic->vm->wire_mode = VPIC_WIRE_INTR;
					dev_dbg(DBG_LEVEL_VIOAPIC, "vpic wire mode -> INTR");
				}
			}
		}

		if (wire_mode_valid) {
			vioapic->rtbl[pin] = new;
			dev_dbg(DBG_LEVEL_VIOAPIC, "ioapic pin%hhu: redir table entry %#lx",
				pin, vioapic->rtbl[pin].full);

			/* remap for ptdev */
			if ((new.bits.intr_mask == IOAPIC_RTE_MASK_CLR) || (last.bits.intr_mask  == IOAPIC_RTE_MASK_CLR)) {
				/* VM enable intr */
				/* NOTE: only support max 256 pin */
				
				(void)ptirq_intx_pin_remap(vioapic->vm, vioapic->chipinfo.gsi_base + pin, INTX_CTLR_IOAPIC);
			}

			/*
			 * Generate an interrupt if the following conditions are met:
			 * - pin is not masked
			 * - previous interrupt has been EOIed
			 * - pin level is asserted
			 */
			if ((vioapic->rtbl[pin].bits.intr_mask == IOAPIC_RTE_MASK_CLR) &&
				(vioapic->rtbl[pin].bits.remote_irr == 0UL) &&
				vioapic_need_intr(vioapic, (uint16_t)pin)) {
				dev_dbg(DBG_LEVEL_VIOAPIC, "ioapic pin%hhu: asserted at rtbl write", pin);
				vioapic_generate_intr(vioapic, pin);
			}
		}
	}
}

static void
vioapic_mmio_rw(struct acrn_single_vioapic *vioapic, uint64_t gpa,
		uint32_t *data, bool do_read)
{
	uint32_t offset;
	uint64_t rflags;

	offset = (uint32_t)(gpa - vioapic->chipinfo.addr);

	spinlock_irqsave_obtain(&(vioapic->lock), &rflags);

	/* The IOAPIC specification allows 32-bit wide accesses to the
	 * IOAPIC_REGSEL (offset 0) and IOAPIC_WINDOW (offset 16) registers.
	 */
	switch (offset) {
	case IOAPIC_REGSEL:
		if (do_read) {
			*data = vioapic->ioregsel;
		} else {
			vioapic->ioregsel = *data & 0xFFU;
		}
		break;
	case IOAPIC_WINDOW:
		if (do_read) {
			*data = vioapic_indirect_read(vioapic,
							vioapic->ioregsel);
		} else {
			vioapic_indirect_write(vioapic,
						 vioapic->ioregsel, *data);
		}
		break;
	default:
		if (do_read) {
			*data = 0xFFFFFFFFU;
		}
		break;
	}

	spinlock_irqrestore_release(&(vioapic->lock), rflags);
}

/*
 * @pre vm != NULL
 */
static void
vioapic_process_eoi(struct acrn_single_vioapic *vioapic, uint32_t vector)
{
	uint32_t pin, pincount = vioapic->chipinfo.nr_pins;
	union ioapic_rte rte;
	uint64_t rflags;

	if ((vector < VECTOR_DYNAMIC_START) || (vector > NR_MAX_VECTOR)) {
		pr_err("vioapic_process_eoi: invalid vector %u", vector);
	}

	dev_dbg(DBG_LEVEL_VIOAPIC, "ioapic processing eoi for vector %u", vector);

	/* notify device to ack if assigned pin */
	for (pin = 0U; pin < pincount; pin++) {
		rte = vioapic->rtbl[pin];
		if ((rte.bits.vector != vector) ||
			(rte.bits.remote_irr == 0U)) {
			continue;
		}

		ptirq_intx_ack(vioapic->vm, vioapic->chipinfo.gsi_base + pin, INTX_CTLR_IOAPIC);
	}

	/*
	 * XXX keep track of the pins associated with this vector instead
	 * of iterating on every single pin each time.
	 */
	spinlock_irqsave_obtain(&(vioapic->lock), &rflags);
	for (pin = 0U; pin < pincount; pin++) {
		rte = vioapic->rtbl[pin];
		if ((rte.bits.vector != vector) ||
			(rte.bits.remote_irr == 0U)) {
			continue;
		}

		vioapic->rtbl[pin].bits.remote_irr = 0U;
		if (vioapic_need_intr(vioapic, (uint16_t)pin)) {
			dev_dbg(DBG_LEVEL_VIOAPIC,
				"ioapic pin%hhu: asserted at eoi", pin);
			vioapic_generate_intr(vioapic, pin);
		}
	}
	spinlock_irqrestore_release(&(vioapic->lock), rflags);
}

void vioapic_broadcast_eoi(const struct acrn_vm *vm, uint32_t vector)
{
	struct acrn_single_vioapic *vioapic;
	uint8_t vioapic_index;

	/*
	 * For platforms with multiple IO-APICs, EOI message from LAPIC is
	 * broadcast to all IO-APICs. Emulating the same behavior here.
	 */

	for (vioapic_index = 0U; vioapic_index < vm->arch_vm.vioapics.ioapic_num; vioapic_index++) {
		vioapic = &(vm_ioapics(vm)->vioapic_array[vioapic_index]);
		vioapic_process_eoi(vioapic, vector);
	}
}

static void reset_one_vioapic(struct acrn_single_vioapic *vioapic)
{
	uint32_t pin, pincount;

	/* Initialize all redirection entries to mask all interrupts */
	pincount = vioapic->chipinfo.nr_pins;
	for (pin = 0U; pin < pincount; pin++) {
		vioapic->rtbl[pin].full = MASK_ALL_INTERRUPTS;
	}
	vioapic->chipinfo.id = 0U;
	vioapic->ioregsel = 0U;
}

void reset_vioapics(const struct acrn_vm *vm)
{
	struct acrn_vioapics *vioapics = vm_ioapics(vm);
	uint8_t vioapic_index;

	for (vioapic_index = 0U; vioapic_index < vioapics->ioapic_num; vioapic_index++) {
		reset_one_vioapic(&vioapics->vioapic_array[vioapic_index]);
	}
}

void
vioapic_init(struct acrn_vm *vm)
{
	static struct ioapic_info virt_ioapic_info = {
		.nr_pins = VIOAPIC_RTE_NUM,
		.addr = VIOAPIC_BASE
	};

	struct ioapic_info *vioapic_info;
	uint8_t vioapic_index;
	struct acrn_single_vioapic *vioapic = NULL;

	if (is_sos_vm(vm)) {
		vm->arch_vm.vioapics.ioapic_num = get_platform_ioapic_info(&vioapic_info);
	} else {
		vm->arch_vm.vioapics.ioapic_num = 1U;
		vioapic_info = &virt_ioapic_info;
	}

	for (vioapic_index = 0U; vioapic_index < vm->arch_vm.vioapics.ioapic_num; vioapic_index++) {
		vioapic = &vm->arch_vm.vioapics.vioapic_array[vioapic_index];
		spinlock_init(&(vioapic->lock));
		vioapic->chipinfo = vioapic_info[vioapic_index];

		vioapic->vm = vm;
		reset_one_vioapic(vioapic);

		register_mmio_emulation_handler(vm, vioapic_mmio_access_handler, (uint64_t)vioapic->chipinfo.addr,
					(uint64_t)vioapic->chipinfo.addr + VIOAPIC_SIZE, (void *)vioapic, false);
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, (uint64_t)vioapic->chipinfo.addr, VIOAPIC_SIZE);
	}

	/*
	 * Maximum number of GSI is computed as GSI base of the IOAPIC i.e. enumerated last in ACPI MADT
	 * plus the number of interrupt pins of that IOAPIC.
	 */
	if (vioapic != NULL) {
		vm->arch_vm.vioapics.nr_gsi = vioapic->chipinfo.gsi_base + vioapic->chipinfo.nr_pins;
	}
}

uint32_t
get_vm_gsicount(const struct acrn_vm *vm)
{
	return vm->arch_vm.vioapics.nr_gsi;
}

/*
 * @pre handler_private_data != NULL
 */
int32_t vioapic_mmio_access_handler(struct io_request *io_req, void *handler_private_data)
{
	struct acrn_single_vioapic *vioapic = (struct acrn_single_vioapic *)handler_private_data;
	struct mmio_request *mmio = &io_req->reqs.mmio;
	uint64_t gpa = mmio->address;
	int32_t ret = 0;

	/* Note all RW to IOAPIC are 32-Bit in size */
	if (mmio->size == 4UL) {
		uint32_t data = (uint32_t)mmio->value;

		if (mmio->direction == REQUEST_READ) {
			vioapic_mmio_rw(vioapic, gpa, &data, true);
			mmio->value = (uint64_t)data;
		} else if (mmio->direction == REQUEST_WRITE) {
			vioapic_mmio_rw(vioapic, gpa, &data, false);
		} else {
			ret = -EINVAL;
		}
	} else {
		pr_err("All RW to IOAPIC must be 32-bits in size");
		ret = -EINVAL;
	}

	return ret;
}

/**
 * @pre vm->arch_vm.vioapics != NULL
 * @pre vgsi < get_vm_gsicount(vm)
 * @pre rte != NULL
 */
void vioapic_get_rte(const struct acrn_vm *vm, uint32_t vgsi, union ioapic_rte *rte)
{
	struct acrn_single_vioapic *vioapic;
	uint32_t pin;
	vioapic = vgsi_to_vioapic_and_vpin(vm, vgsi, &pin);

	*rte = vioapic->rtbl[pin];
}
