/*-
 * Copyright (c) 2013 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * Copyright (c) 2017-2024 Intel Corporation.
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

#include <asm/guest/vm.h>
#include <errno.h>
#include <asm/irq.h>
#include <asm/guest/ept.h>
#include <asm/guest/assign.h>
#include <logmsg.h>
#include <asm/ioapic.h>

/**
 * @addtogroup vp-dm_vioapic
 *
 * @{
 */

/**
 * @file
 * @brief Implementation of the virtual I/O APIC.
 *
 * This file contains the implementation of the virtual I/O APIC. It also defines some helper functions to simulate
 * the virtual I/O APIC that are commonly used in this file.
 */

#define	RTBL_RO_BITS	((uint32_t)0x00004000U | (uint32_t)0x00001000U) /*Remote IRR and Delivery Status bits*/

#define DBG_LEVEL_VIOAPIC	6U
#define ACRN_IOAPIC_VERSION	0x11U

/**
 * @brief The default definition of a RTE.
 *
 * It emulates the virtual I/O APIC as a "82093AA I/O Advanced Programmable Interrupt Controller". Per spec, the default
 * value of the redirection table entry (RTE) is xxx1xxxxxxxxxxxxh. The 'mask' (bit 16) is also set to mask the
 * interrupt.
 */
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
				&& (old_lvl != level)) {
				vioapic_generate_intr(vioapic, pin);
			}
		} else {
			/* set pin_state and deliver intrrupt according to polarity */
			bitmap_set_nolock((uint16_t)(pin & 0x3FU), &vioapic->pin_state[pin >> 6U]);
			if ((rte.bits.intr_polarity == IOAPIC_RTE_INTPOL_AHI)
				&& (old_lvl != level)) {
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

	if (is_service_vm(vm)) {
		/*
		 * Utilize platform ioapic_info for Service VM
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
 * @brief Set virtual I/O APIC IRQ line status for the specified GSI.
 *
 * This function sets the corresponding IRQ line status but would not make sure the set be done with I/O APIC lock. It
 * will assert or deassert the IRQ.
 *
 * The operation option is limited to one of GSI_SET_HIGH/GSI_SET_LOW/GSI_RAISING_PULSE/GSI_FALLING_PULSE. Otherwise,
 * it does nothing.
 * It first gets the corresponding virtual I/O APIC and pin number (index in the Redirection Table) based on the input
 * GSI number.
 * - For the Service VM, it utilizes the platform I/O APIC information to get the virtual I/O APIC and the pin number.
 * - For other VMs, it directly uses virtual I/O APIC 0 and the GSI number as the pin number.
 * It sets the corresponding pin state based on the input operation option.
 * - GSI_SET_HIGH: Set corresponding pin state to high.
 * - GSI_SET_LOW: Set corresponding pin state to low.
 * - GSI_RAISING_PULSE: Set corresponding pin state to high and then to low.
 * - GSI_FALLING_PULSE: Set corresponding pin state to low and then to high.
 * After every set operation, it checks the polarity of the corresponding RTE and the other conditions to determine
 * whether to generate an interrupt. If the polarity is active, the RTE is not masked, and the remote irr bit is
 * cleared, it generates an interrupt and deliver the interrupt to the local APIC. For detailed local APIC operation,
 * see vlapic_receive_intr().
 *
 * @param[inout] vm Pointer to the VM for which the virtual I/O APIC IRQ line status is set.
 * @param[in] vgsi The GSI number to locate the virtual I/O APIC and the offset in the Redirection Table (RT).
 * @param[in] operation The action option to set the virtual I/O APIC IRQ line status.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre vgsi < get_vm_gsicount(vm)
 *
 * @post N/A
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
 * @brief Set IRQ line status with I/O APIC lock for the specified GSI.
 *
 * This function sets the corresponding IRQ line status with the I/O APIC lock. It will assert or deassert the IRQ.
 *
 * It first gets the corresponding virtual I/O APIC and pin number (index in the Redirection Table) based on the input
 * GSI number.
 * - For the Service VM, it utilizes the platform I/O APIC information to get the virtual I/O APIC and the pin number.
 * - For other VMs, it directly uses virtual I/O APIC 0 and the GSI number as the pin number.
 * It sets the IRQ line status witch the virtual I/O APIC lock. For detailed set operation, see
 * vioapic_set_irqline_nolock().
 *
 * @param[inout] vm Pointer to the VM for which the virtual I/O APIC IRQ line status is set.
 * @param[in] vgsi The GSI number to locate the virtual I/O APIC and the offset in the Redirection Table (RT).
 * @param[in] operation The action option to set the virtual I/O APIC IRQ line status.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre vgsi < get_vm_gsicount(vm)
 *
 * @post N/A
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
vioapic_indirect_read(struct acrn_single_vioapic *vioapic, uint32_t addr)
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
			/* RIRR is only used for level triggered interrupts and it's undefined for edge triggered. */
			if (is_lapic_pt_configured(vioapic->vm) &&
				(vioapic->rtbl[pin].bits.trigger_mode == IOAPIC_RTE_TRGRMODE_LEVEL)) {
				/*
				 * For local APIC passthrough case, EOI would not trigger VM-exit. So virtual
				 * 'Remote IRR' would not be updated. Needs to read physical IOxAPIC RTE to
				 * update virtual 'Remote IRR' field each time when guest wants to read I/O
				 * REDIRECTION TABLE REGISTERS
				 */
				struct ptirq_remapping_info *entry = NULL;
				union ioapic_rte phys_rte = {};
				DEFINE_INTX_SID(virt_sid, vioapic->rtbl[pin].bits.vector, INTX_CTLR_IOAPIC);

				entry = find_ptirq_entry(PTDEV_INTR_INTX, &virt_sid, vioapic->vm);
				if (entry != NULL) {
					ioapic_get_rte(entry->allocated_pirq, &phys_rte);
					vioapic->rtbl[pin].bits.remote_irr = phys_rte.bits.remote_irr;
				}
			}
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

	if ((uint32_t)pin < vioapic->chipinfo.nr_pins) {
		rte = vioapic->rtbl[pin];
		lvl = (uint32_t)bitmap_test(pin & 0x3FU, &vioapic->pin_state[pin >> 6U]);
		ret = !!(((rte.bits.intr_polarity == IOAPIC_RTE_INTPOL_ALO) && (lvl == 0U)) ||
			((rte.bits.intr_polarity == IOAPIC_RTE_INTPOL_AHI) && (lvl != 0U)));
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

/**
 * @brief Broadcasts End of Interrupt (EOI) message to all virtual I/O APICs in the VM.
 *
 * This function is used to emulate the behavior of broadcasting an EOI message from the Local APIC (LAPIC) to all I/O
 * APICs in the specified VM. It is typically called when the LAPIC receives an EOI command and broadcasts the EOI
 * message to all virtual I/O APICs for a level triggered interrupt.
 *
 * It iterates through all virtual I/O APICs in the specified VM, and EOI message is valid only for the RTEs with the
 * same vector as the specified vector and the remote IRR bit is set.
 * For a pass-through device, it deasserts the pin level to make the interrupt inactive and unmask the corresponding
 * RTE. For detailed operations, see ptirq_intx_ack().
 * It clears the remote IRR bit of the corresponding RTE and generates an interrupt if the pin level is asserted.
 *
 * @param[inout] vm Pointer to the VM for which the EOI message is broadcast to.
 * @param[in] vector The vector within the EOI message to be broadcast.
 *
 * @return None
 *
 * @pre vm != NULL
 *
 * @post N/A
 */
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

/**
 * @brief Reset all virtual I/O APICs for a given VM.
 *
 * This function resets all virtual I/O APICs for a given VM. It reinitializes the state of each virtual IOAPIC to its
 * default state. This function is typically called during VM reset.
 *
 * It iterates over all virtual I/O APICs of the specified VM and resets each virtual I/O APIC to its default state.
 * Per spec, it initializes every redirection table entry (RTE) to the default value which is 0x0001000000000000. And
 * it masks the interrupt by setting the 'mask' (bit 16).
 *
 * @param[inout] vm Pointer to the VM for which the virtual IOAPICs are to be reset.
 *
 * @return None
 *
 * @pre vm != NULL
 *
 * @post N/A
 */
void reset_vioapics(const struct acrn_vm *vm)
{
	struct acrn_vioapics *vioapics = vm_ioapics(vm);
	uint64_t rflags;
	uint8_t vioapic_index;

	for (vioapic_index = 0U; vioapic_index < vioapics->ioapic_num; vioapic_index++) {
		spinlock_irqsave_obtain(&(vioapics->vioapic_array[vioapic_index].lock), &rflags);
		reset_one_vioapic(&vioapics->vioapic_array[vioapic_index]);
		spinlock_irqrestore_release(&(vioapics->vioapic_array[vioapic_index].lock), rflags);
	}
}

/**
 * @brief Initialize all virtual I/O APICs for a given VM.
 *
 * This function initializes all virtual I/O APICs for a given VM. It sets up the necessary data structures and
 * resources for the virtual I/O APICs to function properly within the VM. This function is usually called during the
 * initialization of the VM.
 *
 * This function emulates every virtual I/O APIC as a "82093AA I/O Advanced Programmable Interrupt Controller" and all
 * virtual I/O APICs are stored in the VM data structure (vm->arch_vm.vioapics).
 * Different VMs, different emulated I/O APIC information:
 * - For the Service VM, the number of virtual I/O APICs is determined by the platform and the I/O APIC information is
 *   retrieved from the platform.
 * - Otherwise, only one virtual I/O APIC is emulated for the VM. The number of redirection table entries (RTEs) is set
 *   to 48 and the default base address of the virtual I/O APIC is set to 0xFEC00000.
 * And for every virtual I/O APIC:
 * - Per spec, it initializes every redirection table entry (RTE) to the default value which is 0x0001000000000000. And
 *   it masks the interrupt by setting the 'mask' (bit 16).
 * - It registers the MMIO emulation handler for all the registers and remove the EPT mapping for the registers address
 *   range.
 * The GSI of a RTE is computed as the GSI base of the I/O APIC plus the RTE index (IRQ number). It sets the maximum
 * number of GSI as the GSI base of the last I/O APIC plus the number of interrupt pins of that I/O APIC.
 *
 * @param[inout] vm Pointer to the VM for which the virtual I/O APICs are to be initialized.
 *
 * @return None
 *
 * @pre vm != NULL
 *
 * @post N/A
 */
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

	if (is_service_vm(vm)) {
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

/**
 * @brief Get the maximum number of GSI for a given VM.
 *
 * This function returns the maximum number of GSI that the VM can support.
 *
 * It returns the maximum number of GSI of the given VM. The value is computed during the initialization of all the
 * virtual I/O APICs for the VM. For details, see vioapic_init().
 *
 * @param[in] vm Pointer to the VM for which the GSI count is to be retrieved.
 *
 * @return An unsigned integer value that indicates the GSI count of the given VM.
 *
 * @pre vm != NULL
 *
 * @post retval >= 0
 */
uint32_t
get_vm_gsicount(const struct acrn_vm *vm)
{
	return vm->arch_vm.vioapics.nr_gsi;
}

/**
 * @brief Handle MMIO (Memory-Mapped I/O) access for the virtual I/O APIC.
 *
 * This function handles MMIO (Memory-Mapped I/O) read and write operations for the virtual I/O APIC registers. The I/O
 * APIC is accessed using an indirect addressing scheme. This function is typically called when the guest performs MMIO
 * operations on the virtual I/O APIC.
 *
 * All operations must follow the operation rules of the 82093AA I/O APIC spec.
 * It will do nothing and return -EINVAL if the access size is not 32 bits or the operation is not read or write.
 * - For a read operation:
 *   - If the offset is IOAPIC_REGSEL (Index Register), it will return the value of the index register.
 *   - If the offset is IOAPIC_WINDOW (Data Register), and according to the index register cached in the last write, the
 *     operation will have different behaviors:
 *     - If the index is in the range of ACPI Indriect Registers (including ID Register, Version Register, Arbitration
 *       ID Register, Redirection Table Register, and Redirection Table), the read operation is emulated and the return
 *       value is set to corresponding value. And if the trigger mode is level sensitive and LAPIC passthrough is
 *       configured, the remote irr bit is updated from physical IOAPIC RTE for a pass-through device.
 *     - Otherwise, the return value is set to 0.
 *   - For other offsets, the read operation is invalid and the return value is set to 0xFFFFFFFF.
 *   - It stores the return value of the read in input MMIO request structure.
 * - For a write operation:
 *   - If the offset is IOAPIC_REGSEL (Index Register), it will store the value as the register index to be accessed.
 *   - If the offset is IOAPIC_WINDOW (Data Register), and according to the index register cached in the last write,
 *     - it updates the virtual ID register if the index is IOAPIC_ID (ID Register);
 *     - it updates the redirection table entry if the index is in the range of IOAPIC_REDTBL (Redirection Table
 *       Register).
 *       - Some OSes simulate EOI message manually using mask+edge followed by unmask+level, so it clears the remote irr
 *         bit if the trigger mode is edge sensitive.
 *       - It adds the virtual GSI to physical GSI mapping for the Service VM if the write RTE is unmasked or the
 *         previous RTE is unmasked. For detailed operation, see ptirq_intx_pin_remap().
 *       - If the RTE is not masked, the remote irr bit is cleared and the pin level is asserted, it generates an
 *         interrupt and delivers it to the local APIC. For detailed local APIC operation, see vlapic_receive_intr().
 *   - For other offsets, it ignores the write operation to indicate that the offset is read-only or reserved.
 * Finally, the function returns 0 to indicate that the MMIO operation is successful.
 *
 * @param[inout] io_req Pointer to the I/O request structure that contains the MMIO request information. For a read
 * 			operation, the read value is stored in this structure.
 * @param[inout] handler_private_data Pointer to the acrn_single_vioapic structure that is treated as an I/O APIC.
 *
 * @return An integer value that indicates the status of the MMIO operation.
 *
 * @retval -EINVAL If the access size is not 32 bits or the operation is not read or write.
 * @retval 0 The MMIO operation is successful.
 *
 * @pre io_req != NULL
 * @pre handler_private_data != NULL
 *
 * @post retval <= 0
 */
int32_t vioapic_mmio_access_handler(struct io_request *io_req, void *handler_private_data)
{
	struct acrn_single_vioapic *vioapic = (struct acrn_single_vioapic *)handler_private_data;
	struct acrn_mmio_request *mmio = &io_req->reqs.mmio_request;
	uint64_t gpa = mmio->address;
	int32_t ret = 0;

	/* Note all RW to IOAPIC are 32-Bit in size */
	if (mmio->size == 4UL) {
		uint32_t data = (uint32_t)mmio->value;

		if (mmio->direction == ACRN_IOREQ_DIR_READ) {
			vioapic_mmio_rw(vioapic, gpa, &data, true);
			mmio->value = (uint64_t)data;
		} else if (mmio->direction == ACRN_IOREQ_DIR_WRITE) {
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
 * @brief Retrieves the Redirection Table Entry (RTE) for a specified GSI.
 *
 * This function is used to get the RTE for a specific GSI in the virtual I/O APIC. The RTE information is used to
 * translate the interrupt manifestation on the corresponding interrupt pin into an APIC message.
 *
 * It first gets the corresponding virtual I/O APIC and pin number (index in the Redirection Table) based on the input
 * GSI number.
 * - For the Service VM, it utilizes the platform I/O APIC information to get the virtual I/O APIC and the pin number.
 * - For other VMs, it directly uses virtual I/O APIC 0 and the GSI number as the pin number.
 * Finally it retrieves the RTE for the pin and stores it in the provided rte parameter.
 *
 * @param[inout] vm Pointer to the VM for which the RTE is to be retrieved.
 * @param[in] vgsi The GSI number to locate the virtual I/O APIC and the offset in the Redirection Table (RT).
 * @param[out] rte Pointer to the variable where the RTE will be stored.
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre vm->arch_vm.vioapics != NULL
 * @pre vgsi < get_vm_gsicount(vm)
 * @pre rte != NULL
 *
 * @post N/A
 */
void vioapic_get_rte(const struct acrn_vm *vm, uint32_t vgsi, union ioapic_rte *rte)
{
	struct acrn_single_vioapic *vioapic;
	uint32_t pin;
	vioapic = vgsi_to_vioapic_and_vpin(vm, vgsi, &pin);

	*rte = vioapic->rtbl[pin];
}

/**
 * @}
 */