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

#include <hypervisor.h>

#define ACRN_DBG_PIC	6U

static void vpic_set_pinstate(struct acrn_vpic *vpic, uint8_t pin, uint8_t level);

static inline bool master_pic(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259)
{
	bool ret;

	if (i8259 == &vpic->i8259[0]) {
		ret = true;
	} else {
	        ret = false;
	}

	return ret;
}

static inline uint8_t vpic_get_highest_isrpin(const struct i8259_reg_state *i8259)
{
	uint8_t bit, pin, i;

	pin = (i8259->lowprio + 1U) & 0x7U;

	for (i = 0U; i < NR_VPIC_PINS_PER_CHIP; i++) {
		bit = (uint8_t)(1U << pin);

		if ((i8259->service & bit) != 0U) {
			/*
			 * An IS bit that is masked by an IMR bit will not be
			 * cleared by a non-specific EOI in Special Mask Mode.
			 */
			if ((i8259->smm != 0U) && ((i8259->mask & bit) != 0U)) {
				pin = (pin + 1U) & 0x7U;
				continue;
			} else {
				return pin;
			}
		}
		pin = (pin + 1U) & 0x7U;
	}

	return VPIC_INVALID_PIN;
}

static inline uint8_t vpic_get_highest_irrpin(const struct i8259_reg_state *i8259)
{
	uint8_t serviced, bit, pin, tmp;

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
		bit = (uint8_t)(1U << pin);

		/*
		 * If there is already an interrupt in service at the same
		 * or higher priority then bail.
		 */
		if ((serviced & bit) != 0) {
			break;
		}

		/*
		 * If an interrupt is asserted and not masked then return
		 * the corresponding 'pin' to the caller.
		 */
		if (((i8259->request & bit) != 0) && ((i8259->mask & bit) == 0)) {
			return pin;
		}

		pin = (pin + 1U) & 0x7U;
	}

	return VPIC_INVALID_PIN;
}

static void vpic_notify_intr(struct acrn_vpic *vpic)
{
	struct i8259_reg_state *i8259;
	uint8_t pin;

	/*
	 * First check the slave.
	 */
	i8259 = &vpic->i8259[1];
	pin = vpic_get_highest_irrpin(i8259);
	if (!i8259->intr_raised && (pin < NR_VPIC_PINS_PER_CHIP)) {
		dev_dbg(ACRN_DBG_PIC,
		"pic slave notify pin = %hhu (imr 0x%x irr 0x%x isr 0x%x)\n",
		pin, i8259->mask, i8259->request, i8259->service);

		/*
		 * Cascade the request from the slave to the master.
		 */
		i8259->intr_raised = true;
		vpic_set_pinstate(vpic, (uint8_t)2U, 1U);
		vpic_set_pinstate(vpic, (uint8_t)2U, 0U);
	} else {
		dev_dbg(ACRN_DBG_PIC,
		"pic slave no eligible interrupt (imr 0x%x irr 0x%x isr 0x%x)",
		i8259->mask, i8259->request, i8259->service);
	}

	/*
	 * Then check the master.
	 */
	i8259 = &vpic->i8259[0];
	pin = vpic_get_highest_irrpin(i8259);
	if (!i8259->intr_raised && (pin < NR_VPIC_PINS_PER_CHIP)) {
		dev_dbg(ACRN_DBG_PIC,
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
			struct acrn_vcpu *vcpu = vcpu_from_vid(vpic->vm, 0U);
			vcpu_inject_extint(vcpu);
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
			vioapic_set_irq(vpic->vm, 0U, GSI_RAISING_PULSE);
		}
	} else {
		dev_dbg(ACRN_DBG_PIC,
		"pic master no eligible interrupt (imr 0x%x irr 0x%x isr 0x%x)",
		i8259->mask, i8259->request, i8259->service);
	}
}

static int32_t vpic_icw1(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	int32_t ret;

	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 icw1 0x%x\n",
		vpic->vm, val);

	i8259->ready = false;

	i8259->icw_num = 1U;
	i8259->request = 0U;
	i8259->mask = 0U;
	i8259->lowprio = 7U;
	i8259->rd_cmd_reg = 0U;
	i8259->poll = false;
	i8259->smm = 0U;

	if ((val & ICW1_SNGL) != 0) {
		dev_dbg(ACRN_DBG_PIC, "vpic cascade mode required\n");
		ret = -1;
	} else if ((val & ICW1_IC4) == 0U) {
		dev_dbg(ACRN_DBG_PIC, "vpic icw4 required\n");
		ret = -1;
	} else {
		i8259->icw_num++;
		ret = 0;
	}

	return ret;
}

static int vpic_icw2(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 icw2 0x%x\n",
		vpic->vm, val);

	i8259->irq_base = val & 0xf8U;

	i8259->icw_num++;

	return 0;
}

static int vpic_icw3(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 icw3 0x%x\n",
		vpic->vm, val);

	i8259->icw_num++;

	return 0;
}

static int32_t vpic_icw4(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	int32_t ret;

	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 icw4 0x%x\n",
		vpic->vm, val);

	if ((val & ICW4_8086) == 0U) {
		dev_dbg(ACRN_DBG_PIC,
			"vpic microprocessor mode required\n");
	        ret = -1;
	} else {
		if ((val & ICW4_AEOI) != 0U) {
			i8259->aeoi = true;
		}

		if ((val & ICW4_SFNM) != 0U) {
			if (master_pic(vpic, i8259)) {
				i8259->sfn = true;
			} else {
				dev_dbg(ACRN_DBG_PIC,
				"Ignoring special fully nested mode on slave pic: %#x",
				val);
			}
		}

		i8259->icw_num = 0U;
		i8259->ready = true;
		ret = 0;
	}

	return ret;
}

static int vpic_ocw1(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	uint8_t pin, i, bit;
	uint8_t old = i8259->mask;

	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 ocw1 0x%x\n",
		vpic->vm, val);

	i8259->mask = val & 0xffU;
	pin = (i8259->lowprio + 1U) & 0x7U;

	/* query and setup if pin/irq is for passthrough device */
	for (i = 0U; i < NR_VPIC_PINS_PER_CHIP; i++) {
		bit = (uint8_t)(1U << pin);

		/* remap for active: interrupt mask -> unmask
		 * remap for deactive: when vIOAPIC take it over
		 */
		if (((i8259->mask & bit) == 0U) && ((old & bit) != 0U)) {
			uint8_t virt_pin;

			/* master i8259 pin2 connect with slave i8259,
			 * not device, so not need pt remap
			 */
			if ((pin == 2U) && master_pic(vpic, i8259)) {
				pin = (pin + 1U) & 0x7U;
				continue;
			}

			virt_pin = (master_pic(vpic, i8259)) ?
					pin : (pin + 8U);
			(void)ptirq_intx_pin_remap(vpic->vm,
					virt_pin, PTDEV_VPIN_PIC);
		}
		pin = (pin + 1U) & 0x7U;
	}

	return 0;
}

static int vpic_ocw2(struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 ocw2 0x%x\n",
		vpic->vm, val);

	i8259->rotate = ((val & OCW2_R) != 0U);

	if ((val & OCW2_EOI) != 0U) {
		uint8_t isr_bit;

		if ((val & OCW2_SL) != 0U) {
			/* specific EOI */
			isr_bit = val & 0x7U;
		} else {
			/* non-specific EOI */
			isr_bit = vpic_get_highest_isrpin(i8259);
		}

		if (isr_bit < NR_VPIC_PINS_PER_CHIP) {
			i8259->service &= ~(uint8_t)(1U << isr_bit);

			if (i8259->rotate) {
				i8259->lowprio = isr_bit;
			}
		}

		/* if level ack PTDEV */
		if ((i8259->elc & (1U << (isr_bit & 0x7U))) != 0U) {
			ptirq_intx_ack(vpic->vm,
				(master_pic(vpic, i8259) ? isr_bit : isr_bit + 8U),
				PTDEV_VPIN_PIC);
		}
	} else if (((val & OCW2_SL) != 0U) && i8259->rotate) {
		/* specific priority */
		i8259->lowprio = val & 0x7U;
	} else {
		/* TODO: Any action required in this case? */
	}

	return 0;
}

static int vpic_ocw3(struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 ocw3 0x%x\n",
		vpic->vm, val);

	if ((val & OCW3_ESMM) != 0U) {
		i8259->smm = ((val & OCW3_SMM) != 0U) ? 1U : 0U;
		dev_dbg(ACRN_DBG_PIC, "%s i8259 special mask mode %s\n",
		    master_pic(vpic, i8259) ? "master" : "slave",
		    (i8259->smm != 0U) ?  "enabled" : "disabled");
	}

	if ((val & OCW3_RR) != 0U) {
		/* read register command */
		i8259->rd_cmd_reg = val & OCW3_RIS;

		/* Polling mode */
		i8259->poll = ((val & OCW3_P) != 0U);
	}

	return 0;
}

/**
 * @pre pin < NR_VPIC_PINS_TOTAL
 */
static void vpic_set_pinstate(struct acrn_vpic *vpic, uint8_t pin,
		uint8_t level)
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

		if (((old_lvl == 0U) && (level == 1U)) ||
				((level == 1U) && (lvl_trigger == true))) {
			/* raising edge or level */
			dev_dbg(ACRN_DBG_PIC, "pic pin%hhu: asserted\n", pin);
			i8259->request |= (uint8_t)(1U << (pin & 0x7U));
		} else if ((old_lvl == 1U) && (level == 0U)) {
			/* falling edge */
			dev_dbg(ACRN_DBG_PIC, "pic pin%hhu: deasserted\n", pin);
			if (lvl_trigger) {
				i8259->request &= ~(uint8_t)(1U << (pin & 0x7U));
			}
		} else {
			dev_dbg(ACRN_DBG_PIC, "pic pin%hhu: %s, ignored\n",
				pin, (level != 0U) ? "asserted" : "deasserted");
		}
	}
}

/**
 * @brief Set vPIC IRQ line status.
 *
 * @param[in] vm        Pointer to target VM
 * @param[in] irq       Target IRQ number
 * @param[in] operation action options:GSI_SET_HIGH/GSI_SET_LOW/
 *			GSI_RAISING_PULSE/GSI_FALLING_PULSE
 *
 * @return None
 */
void vpic_set_irq(struct acrn_vm *vm, uint32_t irq, uint32_t operation)
{
	struct acrn_vpic *vpic;
	struct i8259_reg_state *i8259;
	uint8_t pin;

	if (irq >= NR_VPIC_PINS_TOTAL) {
		return;
	}

	vpic = vm_pic(vm);
	i8259 = &vpic->i8259[irq >> 3U];
	pin = (uint8_t)irq;

	if (i8259->ready == false) {
		return;
	}

	spinlock_obtain(&(vpic->lock));
	switch (operation) {
	case GSI_SET_HIGH:
		vpic_set_pinstate(vpic, pin, 1U);
		break;
	case GSI_SET_LOW:
		vpic_set_pinstate(vpic, pin, 0U);
		break;
	case GSI_RAISING_PULSE:
		vpic_set_pinstate(vpic, pin, 1U);
		vpic_set_pinstate(vpic, pin, 0U);
		break;
	case GSI_FALLING_PULSE:
		vpic_set_pinstate(vpic, pin, 0U);
		vpic_set_pinstate(vpic, pin, 1U);
		break;
	default:
		/*
		 * The function caller could guarantee the pre condition.
		 */
		break;
	}
	vpic_notify_intr(vpic);
	spinlock_release(&(vpic->lock));
}

uint32_t
vpic_pincount(void)
{
	return NR_VPIC_PINS_TOTAL;
}

/**
 * @pre vm->vpic != NULL
 * @pre irq < NR_VPIC_PINS_TOTAL
 */
void vpic_get_irq_trigger(struct acrn_vm *vm, uint32_t irq,
		enum vpic_trigger *trigger)
{
	struct acrn_vpic *vpic;

	vpic = vm_pic(vm);

	if ((vpic->i8259[irq >> 3U].elc & (1U << (irq & 0x7U))) != 0U) {
		*trigger = LEVEL_TRIGGER;
	} else {
		*trigger = EDGE_TRIGGER;
	}
}

/**
 * @brief Get pending virtual interrupts for vPIC.
 *
 * @param[in]    vm     Pointer to target VM
 * @param[inout] vecptr Pointer to vector buffer and will be filled
 *			with eligible vector if any.
 *
 * @return None
 */
void vpic_pending_intr(struct acrn_vm *vm, uint32_t *vecptr)
{
	struct acrn_vpic *vpic;
	struct i8259_reg_state *i8259;
	uint8_t pin;

	vpic = vm_pic(vm);

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

		dev_dbg(ACRN_DBG_PIC, "Got pending vector 0x%x\n", *vecptr);
	}

	spinlock_release(&(vpic->lock));
}

static void vpic_pin_accepted(struct i8259_reg_state *i8259, uint8_t pin)
{
	i8259->intr_raised = false;

	if ((i8259->elc & (1U << pin)) == 0U) {
		/*only used edge trigger mode*/
		i8259->request &= ~(uint8_t)(1U << pin);
	}

	if (i8259->aeoi == true) {
		if (i8259->rotate == true) {
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
 */
void vpic_intr_accepted(struct acrn_vm *vm, uint32_t vector)
{
	struct acrn_vpic *vpic;
	uint8_t pin;

	vpic = vm_pic(vm);

	spinlock_obtain(&(vpic->lock));

	pin = (uint8_t)(vector & 0x7U);

	if ((vector & ~0x7U) == vpic->i8259[1].irq_base) {
		vpic_pin_accepted(&vpic->i8259[1], pin);
		/*
		 * If this vector originated from the slave,
		 * accept the cascaded interrupt too.
		 */
		vpic_pin_accepted(&vpic->i8259[0], (uint8_t)2U);
	} else {
		vpic_pin_accepted(&vpic->i8259[0], pin);
	}

	vpic_notify_intr(vpic);

	spinlock_release(&(vpic->lock));
}

static int vpic_read(struct acrn_vpic *vpic, struct i8259_reg_state *i8259,
		uint16_t port, uint32_t *eax)
{
	uint8_t pin;

	spinlock_obtain(&(vpic->lock));

	if (i8259->poll) {
		i8259->poll = false;
		pin = vpic_get_highest_irrpin(i8259);
		if (pin < NR_VPIC_PINS_PER_CHIP) {
			vpic_pin_accepted(i8259, pin);
			*eax = 0x80U | pin;
		} else {
			*eax = 0U;
		}
	} else {
		if ((port & ICU_IMR_OFFSET) != 0U) {
			/* read interrupt mask register */
			*eax = i8259->mask;
		} else {
			if (i8259->rd_cmd_reg == OCW3_RIS) {
				/* read interrupt service register */
				*eax = i8259->service;
			} else {
				/* read interrupt request register */
				*eax = i8259->request;
			}
		}
	}

	spinlock_release(&(vpic->lock));

	return 0;
}

static int vpic_write(struct acrn_vpic *vpic, struct i8259_reg_state *i8259,
		uint16_t port, uint32_t *eax)
{
	int error;
	uint8_t val;

	error = 0;
	val = (uint8_t)*eax;

	spinlock_obtain(&(vpic->lock));

	if ((port & ICU_IMR_OFFSET) != 0U) {
		switch (i8259->icw_num) {
		case 2U:
			error = vpic_icw2(vpic, i8259, val);
			break;
		case 3U:
			error = vpic_icw3(vpic, i8259, val);
			break;
		case 4U:
			error = vpic_icw4(vpic, i8259, val);
			break;
		default:
			error = vpic_ocw1(vpic, i8259, val);
			break;
		}
	} else {
		if ((val & (1U << 4U)) != 0U) {
			error = vpic_icw1(vpic, i8259, val);
		}

		if (i8259->ready) {
			if ((val & (1U << 3U)) != 0U) {
				error = vpic_ocw3(vpic, i8259, val);
			} else {
				error = vpic_ocw2(vpic, i8259, val);
			}
		}
	}

	if (i8259->ready) {
		vpic_notify_intr(vpic);
	}

	spinlock_release(&(vpic->lock));

	return error;
}

static int32_t vpic_master_handler(struct acrn_vm *vm, bool in, uint16_t port,
		size_t bytes, uint32_t *eax)
{
	struct acrn_vpic *vpic;
	struct i8259_reg_state *i8259;
	int32_t ret;

	vpic = vm_pic(vm);
	i8259 = &vpic->i8259[0];

	if (bytes != 1U) {
	        ret = -1;
	} else if (in) {
	        ret = vpic_read(vpic, i8259, port, eax);
	} else {
		ret = vpic_write(vpic, i8259, port, eax);
	}

	return ret;
}

static uint32_t vpic_master_io_read(struct acrn_vm *vm, uint16_t addr, size_t width)
{
	uint32_t val = 0U;

	if (vpic_master_handler(vm, true, addr, width, &val) < 0) {
		pr_err("pic master read port 0x%x width=%d failed\n",
				addr, width);
	}
	return val;
}

static void vpic_master_io_write(struct acrn_vm *vm, uint16_t addr, size_t width,
				uint32_t v)
{
	uint32_t val = v;

	if (vpic_master_handler(vm, false, addr, width, &val) < 0) {
		pr_err("%s: write port 0x%x width=%d value 0x%x failed\n",
				__func__, addr, width, val);
	}
}

static int32_t vpic_slave_handler(struct acrn_vm *vm, bool in, uint16_t port,
		size_t bytes, uint32_t *eax)
{
	struct acrn_vpic *vpic;
	struct i8259_reg_state *i8259;
	int32_t ret;

	vpic = vm_pic(vm);
	i8259 = &vpic->i8259[1];

	if (bytes != 1U) {
	        ret = -1;
	} else if (in) {
		ret = vpic_read(vpic, i8259, port, eax);
	} else {
		ret = vpic_write(vpic, i8259, port, eax);
	}

	return ret;
}

static uint32_t vpic_slave_io_read(struct acrn_vm *vm, uint16_t addr, size_t width)
{
	uint32_t val = 0U;

	if (vpic_slave_handler(vm, true, addr, width, &val) < 0) {
		pr_err("pic slave read port 0x%x width=%d failed\n",
				addr, width);
	}
	return val;
}

static void vpic_slave_io_write(struct acrn_vm *vm, uint16_t addr, size_t width,
				uint32_t v)
{
	uint32_t val = v;

	if (vpic_slave_handler(vm, false, addr, width, &val) < 0) {
		pr_err("%s: write port 0x%x width=%d value 0x%x failed\n",
				__func__, addr, width, val);
	}
}

static int32_t vpic_elc_handler(struct acrn_vm *vm, bool in, uint16_t port, size_t bytes,
		uint32_t *eax)
{
	struct acrn_vpic *vpic;
	bool is_master;
	int32_t ret;

	vpic = vm_pic(vm);
	is_master = (port == IO_ELCR1);

	if (bytes == 1U) {
		spinlock_obtain(&(vpic->lock));

		if (in) {
			if (is_master) {
				*eax = vpic->i8259[0].elc;
			} else {
				*eax = vpic->i8259[1].elc;
			}
		} else {
			/*
			 * For the master PIC the cascade channel (IRQ2), the
			 * heart beat timer (IRQ0), and the keyboard
			 * controller (IRQ1) cannot be programmed for level
			 * mode.
			 *
			 * For the slave PIC the real time clock (IRQ8) and
			 * the floating point error interrupt (IRQ13) cannot
			 * be programmed for level mode.
			 */
			if (is_master) {
				vpic->i8259[0].elc = (uint8_t)(*eax & 0xf8U);
			} else {
				vpic->i8259[1].elc = (uint8_t)(*eax & 0xdeU);
			}
		}

		spinlock_release(&(vpic->lock));
		ret = 0;
	} else {
	        ret = -1;
	}

	return ret;
}

static uint32_t vpic_elc_io_read(struct acrn_vm *vm, uint16_t addr, size_t width)
{
	uint32_t val = 0U;

	if (vpic_elc_handler(vm, true, addr, width, &val) < 0) {
		pr_err("pic elc read port 0x%x width=%d failed", addr, width);
	}
	return val;
}

static void vpic_elc_io_write(struct acrn_vm *vm, uint16_t addr, size_t width,
				uint32_t v)
{
	uint32_t val = v;

	if (vpic_elc_handler(vm, false, addr, width, &val) < 0) {
		pr_err("%s: write port 0x%x width=%d value 0x%x failed\n",
				__func__, addr, width, val);
	}
}

static void vpic_register_io_handler(struct acrn_vm *vm)
{
	struct vm_io_range master_range = {
		.flags = IO_ATTR_RW,
		.base = 0x20U,
		.len = 2U
	};
	struct vm_io_range slave_range = {
		.flags = IO_ATTR_RW,
		.base = 0xa0U,
		.len = 2U
	};
	struct vm_io_range elcr_range = {
		.flags = IO_ATTR_RW,
		.base = 0x4d0U,
		.len = 2U
	};

	register_io_emulation_handler(vm, PIC_MASTER_PIO_IDX, &master_range,
			vpic_master_io_read, vpic_master_io_write);
	register_io_emulation_handler(vm, PIC_SLAVE_PIO_IDX, &slave_range,
			vpic_slave_io_read, vpic_slave_io_write);
	register_io_emulation_handler(vm, PIC_ELC_PIO_IDX, &elcr_range,
			vpic_elc_io_read, vpic_elc_io_write);
}

void vpic_init(struct acrn_vm *vm)
{
	struct acrn_vpic *vpic = vm_pic(vm);
	vpic_register_io_handler(vm);
	vm->arch_vm.vpic.vm = vm;
	vm->arch_vm.vpic.i8259[0].mask = 0xffU;
	vm->arch_vm.vpic.i8259[1].mask = 0xffU;

	spinlock_init(&(vpic->lock));
}
