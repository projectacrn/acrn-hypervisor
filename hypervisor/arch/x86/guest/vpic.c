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

#define	VPIC_LOCK_INIT(vpic)	spinlock_init(&((vpic)->lock))
#define	VPIC_LOCK(vpic)		spinlock_obtain(&((vpic)->lock))
#define	VPIC_UNLOCK(vpic)	spinlock_release(&((vpic)->lock))
/* TODO: add spinlock_locked support? */
/*#define VPIC_LOCKED(vpic)	spinlock_locked(&((vpic)->lock))*/

#define vm_pic(vm)	(vm->vpic)

#define ACRN_DBG_PIC	6U

enum irqstate {
	IRQSTATE_ASSERT,
	IRQSTATE_DEASSERT,
	IRQSTATE_PULSE
};

struct i8259_reg_state {
	bool		ready;
	uint8_t		icw_num;
	uint8_t		rd_cmd_reg;

	bool		aeoi;
	bool		poll;
	bool		rotate;
	bool		sfn;		/* special fully-nested mode */

	uint32_t	irq_base;
	uint8_t		request;	/* Interrupt Request Register (IIR) */
	uint8_t		service;	/* Interrupt Service (ISR) */
	uint8_t		mask;		/* Interrupt Mask Register (IMR) */
	uint8_t		smm;		/* special mask mode */

	int		acnt[8];	/* sum of pin asserts and deasserts */
	uint8_t		lowprio;	/* lowest priority irq */

	bool		intr_raised;
	uint8_t		elc;
};

struct acrn_vpic {
	struct vm		*vm;
	spinlock_t	lock;
	struct i8259_reg_state	i8259[2];
};

#define NR_VPIC_PINS_PER_CHIP	8U
#define NR_VPIC_PINS_TOTAL	16U
#define VPIC_INVALID_PIN	0xffU

/*
 * Loop over all the pins in priority order from highest to lowest.
 */
#define	PIC_PIN_FOREACH(pinvar, i8259, tmpvar)			\
	for (tmpvar = 0U, pinvar = (i8259->lowprio + 1U) & 0x7U;	\
	    tmpvar < NR_VPIC_PINS_PER_CHIP;				\
	    tmpvar++, pinvar = (pinvar + 1U) & 0x7U)

static void vpic_set_pinstate(struct acrn_vpic *vpic, uint8_t pin, bool newstate);

static inline bool master_pic(struct acrn_vpic *vpic, struct i8259_reg_state *i8259)
{

	if (i8259 == &vpic->i8259[0]) {
		return true;
	} else {
		return false;
	}
}

static inline uint8_t vpic_get_highest_isrpin(struct i8259_reg_state *i8259)
{
	uint8_t bit, pin, i;

	PIC_PIN_FOREACH(pin, i8259, i) {
		bit = (uint8_t)(1U << pin);

		if ((i8259->service & bit) != 0U) {
			/*
			 * An IS bit that is masked by an IMR bit will not be
			 * cleared by a non-specific EOI in Special Mask Mode.
			 */
			if ((i8259->smm != 0U) && (i8259->mask & bit) != 0U) {
				continue;
			} else {
				return pin;
			}
		}
	}

	return VPIC_INVALID_PIN;
}

static inline uint8_t vpic_get_highest_irrpin(struct i8259_reg_state *i8259)
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

	PIC_PIN_FOREACH(pin, i8259, tmp) {
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
		if ((i8259->request & bit) != 0 && (i8259->mask & bit) == 0) {
			return pin;
		}
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
	if (!i8259->intr_raised && pin < NR_VPIC_PINS_PER_CHIP) {
		dev_dbg(ACRN_DBG_PIC,
		"pic slave notify pin = %hhu (imr 0x%x irr 0x%x isr 0x%x)\n",
		pin, i8259->mask, i8259->request, i8259->service);

		/*
		 * Cascade the request from the slave to the master.
		 */
		i8259->intr_raised = true;
		vpic_set_pinstate(vpic, (uint8_t)2U, true);
		vpic_set_pinstate(vpic, (uint8_t)2U, false);
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
	if (!i8259->intr_raised && pin < NR_VPIC_PINS_PER_CHIP) {
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
			struct vcpu *vcpu = vcpu_from_vid(vpic->vm, 0U);

			ASSERT(vcpu != NULL, "vm%d, vcpu0", vpic->vm->vm_id);
			vcpu_inject_extint(vcpu);
		} else {
			vlapic_set_local_intr(vpic->vm, BROADCAST_CPU_ID, APIC_LVT_LINT0);
			/* notify vioapic pin0 if existing
			 * For vPIC + vIOAPIC mode, vpic master irq connected
			 * to vioapic pin0 (irq2)
			 * From MPSpec session 5.1
			 */
			vioapic_pulse_irq(vpic->vm, 0U);
		}
	} else {
		dev_dbg(ACRN_DBG_PIC,
		"pic master no eligible interrupt (imr 0x%x irr 0x%x isr 0x%x)",
		i8259->mask, i8259->request, i8259->service);
	}
}

static int vpic_icw1(struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
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
		return -1;
	}

	if ((val & ICW1_IC4) == 0U) {
		dev_dbg(ACRN_DBG_PIC, "vpic icw4 required\n");
		return -1;
	}

	i8259->icw_num++;

	return 0;
}

static int vpic_icw2(struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 icw2 0x%x\n",
		vpic->vm, val);

	i8259->irq_base = val & 0xf8U;

	i8259->icw_num++;

	return 0;
}

static int vpic_icw3(struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 icw3 0x%x\n",
		vpic->vm, val);

	i8259->icw_num++;

	return 0;
}

static int vpic_icw4(struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 icw4 0x%x\n",
		vpic->vm, val);

	if ((val & ICW4_8086) == 0U) {
		dev_dbg(ACRN_DBG_PIC,
			"vpic microprocessor mode required\n");
		return -1;
	}

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

	return 0;
}

bool vpic_is_pin_mask(struct acrn_vpic *vpic, uint8_t virt_pin_arg)
{
	struct i8259_reg_state *i8259;
	uint8_t virt_pin = virt_pin_arg;

	if (virt_pin < 8U) {
		i8259 = &vpic->i8259[0];
	} else if (virt_pin < 16U) {
		i8259 = &vpic->i8259[1];
		virt_pin -= 8U;
	} else {
		return true;
	}

	if ((i8259->mask & (1U << virt_pin)) != 0U) {
		return true;
	} else {
		return false;
	}
}

static int vpic_ocw1(struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	uint8_t pin, i, bit;
	uint8_t old = i8259->mask;

	dev_dbg(ACRN_DBG_PIC, "vm 0x%x: i8259 ocw1 0x%x\n",
		vpic->vm, val);

	i8259->mask = val & 0xffU;

	/* query and setup if pin/irq is for passthrough device */
	PIC_PIN_FOREACH(pin, i8259, i) {
		bit = (uint8_t)(1U << pin);

		/* remap for active: interrupt mask -> unmask
		 * remap for deactive: when vIOAPIC take it over
		 */
		if (((i8259->mask & bit) == 0U) && ((old & bit) != 0U)) {
			struct ptdev_intx_info intx;

			/* master i8259 pin2 connect with slave i8259,
			 * not device, so not need pt remap
			 */
			if ((pin == 2U) && master_pic(vpic, i8259)) {
				continue;
			}

			intx.virt_pin = pin;
			intx.vpin_src = PTDEV_VPIN_PIC;
			if (!master_pic(vpic, i8259)) {
				intx.virt_pin += 8U;
			}
			ptdev_intx_pin_remap(vpic->vm, &intx);
		}
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
			ptdev_intx_ack(vpic->vm,
				(master_pic(vpic, i8259) ? isr_bit : isr_bit + 8U),
				PTDEV_VPIN_PIC);
		}
	} else if ((val & OCW2_SL) != 0U && i8259->rotate) {
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

static void vpic_set_pinstate(struct acrn_vpic *vpic, uint8_t pin, bool newstate)
{
	struct i8259_reg_state *i8259;
	int oldcnt, newcnt;
	bool level;

	ASSERT(pin < NR_VPIC_PINS_TOTAL,
	    "vpic_set_pinstate: invalid pin number");

	i8259 = &vpic->i8259[pin >> 3U];

	oldcnt = i8259->acnt[pin & 0x7U];
	if (newstate) {
		i8259->acnt[pin & 0x7U]++;
	} else {
		i8259->acnt[pin & 0x7U]--;
	}
	newcnt = i8259->acnt[pin & 0x7U];

	if (newcnt < 0) {
		pr_warn("pic pin%hhu: bad acnt %d\n", pin, newcnt);
	}

	level = ((vpic->i8259[pin >> 3U].elc & (1U << (pin & 0x7U))) != 0);

	if ((oldcnt == 0 && newcnt == 1) || (newcnt > 0 && level == true)) {
		/* rising edge or level */
		dev_dbg(ACRN_DBG_PIC, "pic pin%hhu: asserted\n", pin);
		i8259->request |= (uint8_t)(1U << (pin & 0x7U));
	} else if (oldcnt == 1 && newcnt == 0) {
		/* falling edge */
		dev_dbg(ACRN_DBG_PIC, "pic pin%hhu: deasserted\n", pin);
		if (level) {
			i8259->request &= ~(uint8_t)(1U << (pin & 0x7U));
		}
	} else {
		dev_dbg(ACRN_DBG_PIC,
			"pic pin%hhu: %s, ignored, acnt %d\n",
			pin, newstate ? "asserted" : "deasserted", newcnt);
	}

	vpic_notify_intr(vpic);
}

static int vpic_set_irqstate(struct vm *vm, uint32_t irq, enum irqstate irqstate)
{
	struct acrn_vpic *vpic;
	struct i8259_reg_state *i8259;
	uint8_t pin;

	if (irq >= NR_VPIC_PINS_TOTAL) {
		return -EINVAL;
	}

	vpic = vm_pic(vm);
	i8259 = &vpic->i8259[irq >> 3U];
	pin = (uint8_t)irq;

	if (i8259->ready == false) {
		return 0;
	}

	VPIC_LOCK(vpic);
	switch (irqstate) {
	case IRQSTATE_ASSERT:
		vpic_set_pinstate(vpic, pin, true);
		break;
	case IRQSTATE_DEASSERT:
		vpic_set_pinstate(vpic, pin, false);
		break;
	case IRQSTATE_PULSE:
		vpic_set_pinstate(vpic, pin, true);
		vpic_set_pinstate(vpic, pin, false);
		break;
	default:
		ASSERT(false, "vpic_set_irqstate: invalid irqstate");
	}
	VPIC_UNLOCK(vpic);

	return 0;
}

/* hypervisor interface: assert/deassert/pulse irq */
int vpic_assert_irq(struct vm *vm, uint32_t irq)
{
	return vpic_set_irqstate(vm, irq, IRQSTATE_ASSERT);
}

int vpic_deassert_irq(struct vm *vm, uint32_t irq)
{
	return vpic_set_irqstate(vm, irq, IRQSTATE_DEASSERT);
}

int vpic_pulse_irq(struct vm *vm, uint32_t irq)
{
	return vpic_set_irqstate(vm, irq, IRQSTATE_PULSE);
}

int vpic_set_irq_trigger(struct vm *vm, uint32_t irq, enum vpic_trigger trigger)
{
	struct acrn_vpic *vpic;
	uint8_t pin_mask;

	if (irq >= NR_VPIC_PINS_TOTAL) {
		return -EINVAL;
	}

	/*
	 * See comment in vpic_elc_handler.  These IRQs must be
	 * edge triggered.
	 */
	if (trigger == LEVEL_TRIGGER) {
		switch (irq) {
		case 0U:
		case 1U:
		case 2U:
		case 8U:
		case 13U:
			return -EINVAL;
		}
	}

	vpic = vm_pic(vm);
	pin_mask = (uint8_t)(1U << (irq & 0x7U));

	VPIC_LOCK(vpic);

	if (trigger == LEVEL_TRIGGER) {
		vpic->i8259[irq >> 3U].elc |=  pin_mask;
	} else {
		vpic->i8259[irq >> 3U].elc &=  ~pin_mask;
	}

	VPIC_UNLOCK(vpic);

	return 0;
}

int vpic_get_irq_trigger(struct vm *vm, uint32_t irq, enum vpic_trigger *trigger)
{
	struct acrn_vpic *vpic;

	if (irq >= NR_VPIC_PINS_TOTAL) {
		return -EINVAL;
	}

	vpic = vm_pic(vm);
	if (vpic == NULL) {
		return -EINVAL;
	}

	if ((vpic->i8259[irq >> 3U].elc & (1U << (irq & 0x7U))) != 0U) {
		*trigger = LEVEL_TRIGGER;
	} else {
		*trigger = EDGE_TRIGGER;
	}
	return 0;
}

void vpic_pending_intr(struct vm *vm, uint32_t *vecptr)
{
	struct acrn_vpic *vpic;
	struct i8259_reg_state *i8259;
	uint8_t pin;

	vpic = vm_pic(vm);

	i8259 = &vpic->i8259[0];

	VPIC_LOCK(vpic);

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
		VPIC_UNLOCK(vpic);
		return;
	}

	*vecptr = i8259->irq_base + pin;

	dev_dbg(ACRN_DBG_PIC, "Got pending vector 0x%x\n", *vecptr);

	VPIC_UNLOCK(vpic);
}

static void vpic_pin_accepted(struct i8259_reg_state *i8259, uint8_t pin)
{
	i8259->intr_raised = false;

	if ((i8259->elc & (1U << pin)) == 0) {
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

void vpic_intr_accepted(struct vm *vm, uint32_t vector)
{
	struct acrn_vpic *vpic;
	uint8_t pin;

	vpic = vm_pic(vm);

	VPIC_LOCK(vpic);

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

	VPIC_UNLOCK(vpic);
}

static int vpic_read(struct acrn_vpic *vpic, struct i8259_reg_state *i8259,
		uint16_t port, uint32_t *eax)
{
	uint8_t pin;

	VPIC_LOCK(vpic);

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

	VPIC_UNLOCK(vpic);

	return 0;
}

static int vpic_write(struct acrn_vpic *vpic, struct i8259_reg_state *i8259,
		uint16_t port, uint32_t *eax)
{
	int error;
	uint8_t val;

	error = 0;
	val = (uint8_t)*eax;

	VPIC_LOCK(vpic);

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

	VPIC_UNLOCK(vpic);

	return error;
}

static int vpic_master_handler(struct vm *vm, bool in, uint16_t port,
		size_t bytes, uint32_t *eax)
{
	struct acrn_vpic *vpic;
	struct i8259_reg_state *i8259;

	vpic = vm_pic(vm);
	i8259 = &vpic->i8259[0];

	if (bytes != 1U) {
		return -1;
	}

	if (in) {
		return vpic_read(vpic, i8259, port, eax);
	}

	return vpic_write(vpic, i8259, port, eax);
}

static uint32_t vpic_master_io_read(__unused struct vm_io_handler *hdlr,
		struct vm *vm, uint16_t addr, size_t width)
{
	uint32_t val = 0U;

	if (vpic_master_handler(vm, true, addr, width, &val) < 0) {
		pr_err("pic master read port 0x%x width=%d failed\n",
				addr, width);
	}
	return val;
}

static void vpic_master_io_write(__unused struct vm_io_handler *hdlr,
		struct vm *vm, uint16_t addr, size_t width, uint32_t v)
{
	uint32_t val = v;

	if (vpic_master_handler(vm, false, addr, width, &val) < 0) {
		pr_err("%s: write port 0x%x width=%d value 0x%x failed\n",
				__func__, addr, width, val);
	}
}

static int vpic_slave_handler(struct vm *vm, bool in, uint16_t port,
		size_t bytes, uint32_t *eax)
{
	struct acrn_vpic *vpic;
	struct i8259_reg_state *i8259;

	vpic = vm_pic(vm);
	i8259 = &vpic->i8259[1];

	if (bytes != 1U) {
		return -1;
	}

	if (in) {
		return vpic_read(vpic, i8259, port, eax);
	}

	return vpic_write(vpic, i8259, port, eax);
}

static uint32_t vpic_slave_io_read(__unused struct vm_io_handler *hdlr,
		struct vm *vm, uint16_t addr, size_t width)
{
	uint32_t val = 0U;

	if (vpic_slave_handler(vm, true, addr, width, &val) < 0) {
		pr_err("pic slave read port 0x%x width=%d failed\n",
				addr, width);
	}
	return val;
}

static void vpic_slave_io_write(__unused struct vm_io_handler *hdlr,
		struct vm *vm, uint16_t addr, size_t width, uint32_t v)
{
	uint32_t val = v;

	if (vpic_slave_handler(vm, false, addr, width, &val) < 0) {
		pr_err("%s: write port 0x%x width=%d value 0x%x failed\n",
				__func__, addr, width, val);
	}
}

static int vpic_elc_handler(struct vm *vm, bool in, uint16_t port, size_t bytes,
		uint32_t *eax)
{
	struct acrn_vpic *vpic;
	bool is_master;

	vpic = vm_pic(vm);
	is_master = (port == IO_ELCR1);

	if (bytes != 1U) {
		return -1;
	}

	VPIC_LOCK(vpic);

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

	VPIC_UNLOCK(vpic);

	return 0;
}

static uint32_t vpic_elc_io_read(__unused struct vm_io_handler *hdlr,
		struct vm *vm, uint16_t addr, size_t width)
{
	uint32_t val = 0U;

	if (vpic_elc_handler(vm, true, addr, width, &val) < 0) {
		pr_err("pic elc read port 0x%x width=%d failed", addr, width);
	}
	return val;
}

static void vpic_elc_io_write(__unused struct vm_io_handler *hdlr,
		struct vm *vm, uint16_t addr, size_t width, uint32_t v)
{
	uint32_t val = v;

	if (vpic_elc_handler(vm, false, addr, width, &val) < 0) {
		pr_err("%s: write port 0x%x width=%d value 0x%x failed\n",
				__func__, addr, width, val);
	}
}

static void vpic_register_io_handler(struct vm *vm)
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

	register_io_emulation_handler(vm, &master_range,
			&vpic_master_io_read, &vpic_master_io_write);
	register_io_emulation_handler(vm, &slave_range,
			&vpic_slave_io_read, &vpic_slave_io_write);
	register_io_emulation_handler(vm, &elcr_range,
			&vpic_elc_io_read, &vpic_elc_io_write);
}

void *vpic_init(struct vm *vm)
{
	struct acrn_vpic *vpic;

	vpic_register_io_handler(vm);

	vpic = calloc(1U, sizeof(struct acrn_vpic));
	ASSERT(vpic != NULL, "");
	vpic->vm = vm;
	vpic->i8259[0].mask = 0xffU;
	vpic->i8259[1].mask = 0xffU;

	VPIC_LOCK_INIT(vpic);

	return vpic;
}

void vpic_cleanup(struct vm *vm)
{
	if (vm->vpic != NULL) {
		free(vm->vpic);
		vm->vpic = NULL;
	}
}
