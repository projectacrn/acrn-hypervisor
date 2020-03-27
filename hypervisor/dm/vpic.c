/*-
 * Copyright (c) 2014 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define pr_prefix	"vpic: "

#include <vm.h>
#include <irq.h>
#include <assign.h>
#include <spinlock.h>
#include <logmsg.h>
#include <ioapic.h>

#define DBG_LEVEL_PIC	6U

static void vpic_set_pinstate(struct acrn_vpic *vpic, uint32_t pin, uint8_t level);

struct acrn_vpic *vm_pic(const struct acrn_vm *vm)
{
	return (struct acrn_vpic *)&(vm->arch_vm.vpic);
}

static inline uint32_t vpic_get_highest_irrpin(const struct i8259_reg_state *i8259)
{
	uint8_t serviced;
	uint32_t bit, pin, tmp;
	uint32_t found_pin = INVALID_INTERRUPT_PIN;

	/*
	 * In 'Special Fully-Nested Mode' when an interrupt request from
	 * a slave is in service, the slave is not locked out from the
	 * master's priority logic.
	 */
	serviced = i8259->service;
	if (i8259->sfn) {
		serviced &= ~(uint8_t)(1U << 2U);
	}

	/*
	 * In 'Special Mask Mode', when a mask bit is set in OCW1 it inhibits
	 * further interrupts at that level and enables interrupts from all
	 * other levels that are not masked. In other words the ISR has no
	 * bearing on the levels that can generate interrupts.
	 */
	if (i8259->smm != 0U) {
		serviced = 0U;
	}

	pin = (i8259->lowprio + 1U) & 0x7U;

	for (tmp = 0U; tmp < NR_VPIC_PINS_PER_CHIP; tmp++) {
		bit = (1U << pin);

		/*
		 * If there is already an interrupt in service at the same
		 * or higher priority then bail.
		 */
		if ((serviced & bit) != 0U) {
			break;
		}

		/*
		 * If an interrupt is asserted and not masked then return
		 * the corresponding 'pin' to the caller.
		 */
		if (((i8259->request & bit) != 0U) && ((i8259->mask & bit) == 0U)) {
			found_pin = pin;
			break;
		}

		pin = (pin + 1U) & 0x7U;
	}

	return found_pin;
}

static void vpic_notify_intr(struct acrn_vpic *vpic)
{
	struct i8259_reg_state *i8259;
	uint32_t pin;

	/*
	 * First check the slave.
	 */
	i8259 = &vpic->i8259[1];
	pin = vpic_get_highest_irrpin(i8259);
	if (!i8259->intr_raised && (pin < NR_VPIC_PINS_PER_CHIP)) {
		dev_dbg(DBG_LEVEL_PIC,
		"pic slave notify pin = %hhu (imr 0x%x irr 0x%x isr 0x%x)\n",
		pin, i8259->mask, i8259->request, i8259->service);

		/*
		 * Cascade the request from the slave to the master.
		 */
		i8259->intr_raised = true;
		vpic_set_pinstate(vpic, 2U, 1U);
		vpic_set_pinstate(vpic, 2U, 0U);
	} else {
		dev_dbg(DBG_LEVEL_PIC,
		"pic slave no eligible interrupt (imr 0x%x irr 0x%x isr 0x%x)",
		i8259->mask, i8259->request, i8259->service);
	}

	/*
	 * Then check the master.
	 */
	i8259 = &vpic->i8259[0];
	pin = vpic_get_highest_irrpin(i8259);
	if (!i8259->intr_raised && (pin < NR_VPIC_PINS_PER_CHIP)) {
		dev_dbg(DBG_LEVEL_PIC,
		"pic master notify pin = %hhu (imr 0x%x irr 0x%x isr 0x%x)\n",
		pin, i8259->mask, i8259->request, i8259->service);

		/*
		 * From Section 3.6.2, "Interrupt Modes", in the
		 * MPtable Specification, Version 1.4
		 *
		 * PIC interrupts are routed to both the Local APIC
		 * and the I/O APIC to support operation in 1 of 3
		 * modes.
		 *
		 * 1. Legacy PIC Mode: the PIC effectively bypasses
		 * all APIC components.  In this mode the local APIC is
		 * disabled and LINT0 is reconfigured as INTR to
		 * deliver the PIC interrupt directly to the CPU.
		 *
		 * 2. Virtual Wire Mode: the APIC is treated as a
		 * virtual wire which delivers interrupts from the PIC
		 * to the CPU.  In this mode LINT0 is programmed as
		 * ExtINT to indicate that the PIC is the source of
		 * the interrupt.
		 *
		 * 3. Virtual Wire Mode via I/O APIC: PIC interrupts are
		 * fielded by the I/O APIC and delivered to the appropriate
		 * CPU.  In this mode the I/O APIC input 0 is programmed
		 * as ExtINT to indicate that the PIC is the source of the
		 * interrupt.
		 */
		i8259->intr_raised = true;
		if (vpic->vm->wire_mode == VPIC_WIRE_INTR) {
			struct acrn_vcpu *bsp = vcpu_from_vid(vpic->vm, BSP_CPU_ID);
			vcpu_inject_extint(bsp);
		} else {
			/*
			 * The input parameters here guarantee the return value of vlapic_set_local_intr is 0, means
			 * success.
			 */
			(void)vlapic_set_local_intr(vpic->vm, BROADCAST_CPU_ID, APIC_LVT_LINT0);
			/* notify vioapic pin0 if existing
			 * For vPIC + vIOAPIC mode, vpic master irq connected
			 * to vioapic pin0 (irq2)
			 * From MPSpec session 5.1
			 */
			vioapic_set_irqline_lock(vpic->vm, 0U, GSI_RAISING_PULSE);
		}
	} else {
		dev_dbg(DBG_LEVEL_PIC,
		"pic master no eligible interrupt (imr 0x%x irr 0x%x isr 0x%x)",
		i8259->mask, i8259->request, i8259->service);
	}
}

/**
 * @pre pin < NR_VPIC_PINS_TOTAL
 */
static void vpic_set_pinstate(struct acrn_vpic *vpic, uint32_t pin, uint8_t level)
{
	struct i8259_reg_state *i8259;
	uint8_t old_lvl;
	bool lvl_trigger;

	if (pin < NR_VPIC_PINS_TOTAL) {
		i8259 = &vpic->i8259[pin >> 3U];
		old_lvl = i8259->pin_state[pin & 0x7U];
		if (level != 0U) {
			i8259->pin_state[pin & 0x7U] = 1U;
		} else {
			i8259->pin_state[pin & 0x7U] = 0U;
		}

		lvl_trigger = ((vpic->i8259[pin >> 3U].elc & (1U << (pin & 0x7U))) != 0U);

		if (((old_lvl == 0U) && (level == 1U)) || ((level == 1U) && lvl_trigger)) {
			/* raising edge or level */
			dev_dbg(DBG_LEVEL_PIC, "pic pin%hhu: asserted\n", pin);
			i8259->request |= (uint8_t)(1U << (pin & 0x7U));
		} else if ((old_lvl == 1U) && (level == 0U)) {
			/* falling edge */
			dev_dbg(DBG_LEVEL_PIC, "pic pin%hhu: deasserted\n", pin);
			if (lvl_trigger) {
				i8259->request &= ~(uint8_t)(1U << (pin & 0x7U));
			}
		} else {
			dev_dbg(DBG_LEVEL_PIC, "pic pin%hhu: %s, ignored\n",
				pin, (level != 0U) ? "asserted" : "deasserted");
		}
	}
}

/**
 * @brief Get pending virtual interrupts for vPIC.
 *
 * @param[in]    vpic   Pointer to target VM's vpic table
 * @param[inout] vecptr Pointer to vector buffer and will be filled
 *			with eligible vector if any.
 *
 * @pre this function should be called after vpic_init()
 * @return None
 */
void vpic_pending_intr(struct acrn_vpic *vpic, uint32_t *vecptr)
{
	struct i8259_reg_state *i8259;
	uint32_t pin;

	i8259 = &vpic->i8259[0];

	spinlock_obtain(&(vpic->lock));

	pin = vpic_get_highest_irrpin(i8259);
	if (pin == 2U) {
		i8259 = &vpic->i8259[1];
		pin = vpic_get_highest_irrpin(i8259);
	}

	/*
	 * If there are no pins active at this moment then return the spurious
	 * interrupt vector instead.
	 */
	if (pin >= NR_VPIC_PINS_PER_CHIP) {
		*vecptr = VECTOR_INVALID;
	} else {
		*vecptr = i8259->irq_base + pin;

		dev_dbg(DBG_LEVEL_PIC, "Got pending vector 0x%x\n", *vecptr);
	}

	spinlock_release(&(vpic->lock));
}

static void vpic_pin_accepted(struct i8259_reg_state *i8259, uint32_t pin)
{
	i8259->intr_raised = false;

	if ((i8259->elc & (1U << pin)) == 0U) {
		/*only used edge trigger mode*/
		i8259->request &= ~(uint8_t)(1U << pin);
	}

	if (i8259->aeoi) {
		if (i8259->rotate) {
			i8259->lowprio = pin;
		}
	} else {
		i8259->service |= (uint8_t)(1U << pin);
	}
}

/**
 * @brief Accept virtual interrupt for vPIC.
 *
 * @param[in] vm     Pointer to target VM
 * @param[in] vector Target virtual interrupt vector
 *
 * @return None
 *
 * @pre vm != NULL
 * @pre this function should be called after vpic_init()
 */
void vpic_intr_accepted(struct acrn_vpic *vpic, uint32_t vector)
{
	uint32_t pin;

	spinlock_obtain(&(vpic->lock));

	pin = (vector & 0x7U);

	if ((vector & ~0x7U) == vpic->i8259[1].irq_base) {
		vpic_pin_accepted(&vpic->i8259[1], pin);
		/*
		 * If this vector originated from the slave,
		 * accept the cascaded interrupt too.
		 */
		vpic_pin_accepted(&vpic->i8259[0], 2U);
	} else {
		vpic_pin_accepted(&vpic->i8259[0], pin);
	}

	vpic_notify_intr(vpic);

	spinlock_release(&(vpic->lock));
}

static void vpic_register_io_handler(struct acrn_vm *vm)
{
	struct vm_io_range master_range = {
		.base = 0x20U,
		.len = 2U
	};
	struct vm_io_range slave_range = {
		.base = 0xa0U,
		.len = 2U
	};
	struct vm_io_range elcr_range = {
		.base = 0x4d0U,
		.len = 2U
	};

	register_pio_emulation_handler(vm, PIC_MASTER_PIO_IDX, &master_range,
			pio_default_read, pio_default_write);
	register_pio_emulation_handler(vm, PIC_SLAVE_PIO_IDX, &slave_range,
			pio_default_read, pio_default_write);
	register_pio_emulation_handler(vm, PIC_ELC_PIO_IDX, &elcr_range,
			pio_default_read, pio_default_write);
}

void vpic_init(struct acrn_vm *vm)
{
	struct acrn_vpic *vpic = vm_pic(vm);
	vpic_register_io_handler(vm);
	vpic->vm = vm;
	vpic->i8259[0].mask = 0xffU;
	vpic->i8259[1].mask = 0xffU;

	spinlock_init(&(vpic->lock));
}
