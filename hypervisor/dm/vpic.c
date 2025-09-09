/*-
 * Copyright (c) 2014 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2017-2025 Intel Corporation.
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

#include <asm/guest/vm.h>
#include <asm/guest/virq.h>
#include <irq.h>
#include <asm/guest/assign.h>
#include <spinlock.h>
#include <logmsg.h>
#include <asm/ioapic.h>
#include <asm/irq.h>

#define DBG_LEVEL_PIC	6U

static void vpic_set_pinstate(struct acrn_vpic *vpic, uint32_t pin, uint8_t level);

static inline struct acrn_vm *vpic2vm(const struct acrn_vpic *vpic)
{
	return container_of(container_of(vpic, struct vm_arch, vpic), struct acrn_vm, arch_vm);
}

struct acrn_vpic *vm_pic(const struct acrn_vm *vm)
{
	return (struct acrn_vpic *)&(vm->arch_vm.vpic);
}

static inline bool primary_pic(const struct acrn_vpic *vpic, const struct i8259_reg_state *i8259)
{
	bool ret;

	if (i8259 == &vpic->i8259[0]) {
		ret = true;
	} else {
	        ret = false;
	}

	return ret;
}

static inline uint32_t vpic_get_highest_isrpin(const struct i8259_reg_state *i8259)
{
	uint32_t bit, pin, i;
	uint32_t found_pin = INVALID_INTERRUPT_PIN;

	pin = (i8259->lowprio + 1U) & 0x7U;

	for (i = 0U; i < NR_VPIC_PINS_PER_CHIP; i++) {
		bit = (1U << pin);

		if ((i8259->service & bit) != 0U) {
			/*
			 * An IS bit that is masked by an IMR bit will not be
			 * cleared by a non-specific EOI in Special Mask Mode.
			 */
			if ((i8259->smm != 0U) && ((i8259->mask & bit) != 0U)) {
				pin = (pin + 1U) & 0x7U;
				continue;
			} else {
				found_pin = pin;
				break;
			}
		}
		pin = (pin + 1U) & 0x7U;
	}

	return found_pin;
}

static inline uint32_t vpic_get_highest_irrpin(const struct i8259_reg_state *i8259)
{
	uint8_t serviced;
	uint32_t bit, pin, tmp;
	uint32_t found_pin = INVALID_INTERRUPT_PIN;

	/*
	 * In 'Special Fully-Nested Mode' when an interrupt request from
	 * a secondary PIC is in service, the secondary PIC is not locked out from the
	 * primary PIC's priority logic.
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
	 * First check the secondary vPIC.
	 */
	i8259 = &vpic->i8259[1];
	pin = vpic_get_highest_irrpin(i8259);
	if (!i8259->intr_raised && (pin < NR_VPIC_PINS_PER_CHIP)) {
		dev_dbg(DBG_LEVEL_PIC,
		"Secondary vPIC notify pin = %hhu (imr 0x%x irr 0x%x isr 0x%x)\n",
		pin, i8259->mask, i8259->request, i8259->service);

		/*
		 * Cascade the request from the secondary to the primary vPIC.
		 */
		i8259->intr_raised = true;
		vpic_set_pinstate(vpic, 2U, 1U);
		vpic_set_pinstate(vpic, 2U, 0U);
	} else {
		dev_dbg(DBG_LEVEL_PIC,
		"Secondary vPIC no eligible interrupt (imr 0x%x irr 0x%x isr 0x%x)",
		i8259->mask, i8259->request, i8259->service);
	}

	/*
	 * Then check the primary vPIC.
	 */
	i8259 = &vpic->i8259[0];
	pin = vpic_get_highest_irrpin(i8259);
	if (!i8259->intr_raised && (pin < NR_VPIC_PINS_PER_CHIP)) {
		struct acrn_vm *vm = vpic2vm(vpic);

		dev_dbg(DBG_LEVEL_PIC,
		"Primary PIC notify pin = %hhu (imr 0x%x irr 0x%x isr 0x%x)\n",
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
		if (vm->wire_mode == VPIC_WIRE_INTR) {
			struct acrn_vcpu *bsp = vcpu_from_vid(vm, BSP_CPU_ID);
			vcpu_inject_extint(bsp);
		} else {
			/*
			 * The input parameters here guarantee the return value of vlapic_set_local_intr is 0, means
			 * success.
			 */
			(void)vlapic_set_local_intr(vm, BROADCAST_CPU_ID, APIC_LVT_LINT0);
			/* notify vioapic pin0 if existing
			 * For vPIC + vIOAPIC mode, primary vPIC irq connected
			 * to vioapic pin0 (irq2)
			 * From MPSpec session 5.1
			 */
			vioapic_set_irqline_lock(vm, 0U, GSI_RAISING_PULSE);
		}
	} else {
		dev_dbg(DBG_LEVEL_PIC,
		"Primary vPIC has no eligible interrupt (imr 0x%x irr 0x%x isr 0x%x)",
		i8259->mask, i8259->request, i8259->service);
	}
}

static int32_t vpic_icw1(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	int32_t ret;

	dev_dbg(DBG_LEVEL_PIC, "vm 0x%x: i8259 icw1 0x%x\n", vpic2vm(vpic), val);

	i8259->ready = false;

	i8259->icw_num = 1U;
	i8259->request = 0U;
	i8259->mask = 0U;
	i8259->lowprio = 7U;
	i8259->rd_cmd_reg = 0U;
	i8259->poll = false;
	i8259->smm = 0U;

	if ((val & ICW1_SNGL) != 0U) {
		dev_dbg(DBG_LEVEL_PIC, "vpic cascade mode required\n");
		ret = -1;
	} else if ((val & ICW1_IC4) == 0U) {
		dev_dbg(DBG_LEVEL_PIC, "vpic icw4 required\n");
		ret = -1;
	} else {
		i8259->icw_num++;
		ret = 0;
	}

	return ret;
}

static int32_t vpic_icw2(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	dev_dbg(DBG_LEVEL_PIC, "vm 0x%x: i8259 icw2 0x%x\n", vpic2vm(vpic), val);

	i8259->irq_base = val & 0xf8U;

	i8259->icw_num++;

	return 0;
}

static int32_t vpic_icw3(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	dev_dbg(DBG_LEVEL_PIC, "vm 0x%x: i8259 icw3 0x%x\n", vpic2vm(vpic), val);

	i8259->icw_num++;

	return 0;
}

static int32_t vpic_icw4(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	int32_t ret;

	dev_dbg(DBG_LEVEL_PIC, "vm 0x%x: i8259 icw4 0x%x\n", vpic2vm(vpic), val);

	if ((val & ICW4_8086) == 0U) {
		dev_dbg(DBG_LEVEL_PIC,
			"vpic microprocessor mode required\n");
	        ret = -1;
	} else {
		if ((val & ICW4_AEOI) != 0U) {
			i8259->aeoi = true;
		}

		if ((val & ICW4_SFNM) != 0U) {
			if (primary_pic(vpic, i8259)) {
				i8259->sfn = true;
			} else {
				dev_dbg(DBG_LEVEL_PIC,
				"Ignoring special fully nested mode on secondary pic: %#x",
				val);
			}
		}

		i8259->icw_num = 0U;
		i8259->ready = true;
		ret = 0;
	}

	return ret;
}

static uint32_t vpin_to_vgsi(const struct acrn_vm *vm, uint32_t vpin)
{
	uint32_t vgsi = vpin;

	/*
	 * Remap depending on the type of VM
	 */

	if (is_service_vm(vm)) {
		/*
		 * For Service VM vPIC pin to GSI is same as the one
		 * that is used for platform
		 */
		vgsi = get_pic_pin_from_ioapic_pin(vpin);
	} else if (is_postlaunched_vm(vm)) {
		/*
		 * Devicemodel provides Interrupt Source Override Structure
		 * via ACPI to Post-Launched VM.
		 * 
		 * 1) Interrupt source connected to vPIC pin 0 is connected to vIOAPIC pin 2
		 * 2) Devicemodel, as of today, does not request to hold ptirq entry with vPIC as 
		 *    interrupt controller, for a Post-Launched VM.
		 */
		if (vpin == 0U) {
			vgsi = 2U;
		}
	} else {
		/*
		 * For Pre-launched VMs, Interrupt Source Override Structure
		 * and IO-APIC Structure are not provided in the VM's ACPI info.
		 * No remapping needed.
		 */
	}
	return vgsi;
}

static uint32_t vgsi_to_vpin(const struct acrn_vm *vm, uint32_t vgsi)
{
	uint32_t vpin = vgsi;

	/*
	 * Remap depending on the type of VM
	 */

	if (is_service_vm(vm)) {
		/*
		 * For Service VM vPIC pin to GSI is same as the one
		 * that is used for platform
		 */
		vpin = get_pic_pin_from_ioapic_pin(vgsi);
	} else if (is_postlaunched_vm(vm)) {
		/*
		 * Devicemodel provides Interrupt Source Override Structure
		 * via ACPI to Post-Launched VM.
		 * 
		 * 1) Interrupt source connected to vPIC pin 0 is connected to vIOAPIC pin 2
		 * 2) Devicemodel, as of today, does not request to hold ptirq entry with vPIC as 
		 *    interrupt controller, for a Post-Launched VM.
		 */
		if (vgsi == 2U) {
			vpin = 0U;
		}
	} else {
		/*
		 * For Pre-launched VMs, Interrupt Source Override Structure
		 * and IO-APIC Structure are not provided in the VM's ACPI info.
		 * No remapping needed.
		 */
	}
	return vpin;

}

static int32_t vpic_ocw1(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	uint32_t pin, i, bit;
	uint8_t old = i8259->mask;
	struct acrn_vm *vm = vpic2vm(vpic);

	dev_dbg(DBG_LEVEL_PIC, "vm 0x%x: i8259 ocw1 0x%x\n", vm, val);

	i8259->mask = val & 0xffU;
	pin = (i8259->lowprio + 1U) & 0x7U;

	/* query and setup if pin/irq is for passthrough device */
	for (i = 0U; i < NR_VPIC_PINS_PER_CHIP; i++) {
		bit = (1U << pin);

		/* remap for active: interrupt mask -> unmask
		 * remap for deactive: when vIOAPIC take it over
		 */
		if (((i8259->mask & bit) == 0U) && ((old & bit) != 0U)) {
			uint32_t virt_pin;
			uint32_t vgsi;

			/* Primary i8259 pin2 connect with secondary i8259,
			 * not device, so not need pt remap
			 */
			if ((pin == 2U) && primary_pic(vpic, i8259)) {
				pin = (pin + 1U) & 0x7U;
				continue;
			}

			virt_pin = (primary_pic(vpic, i8259)) ?
					pin : (pin + 8U);

			vgsi = vpin_to_vgsi(vm, virt_pin);
			(void)ptirq_intx_pin_remap(vm, vgsi, INTX_CTLR_PIC);
		}
		pin = (pin + 1U) & 0x7U;
	}

	return 0;
}

static int32_t vpic_ocw2(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	struct acrn_vm *vm = vpic2vm(vpic);

	dev_dbg(DBG_LEVEL_PIC, "vm 0x%x: i8259 ocw2 0x%x\n", vm, val);

	i8259->rotate = ((val & OCW2_R) != 0U);

	if ((val & OCW2_EOI) != 0U) {
		uint32_t isr_bit;
		uint32_t vgsi;

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
			vgsi = vpin_to_vgsi(vm, (primary_pic(vpic, i8259) ? isr_bit : (isr_bit + 8U)));
			ptirq_intx_ack(vm, vgsi, INTX_CTLR_PIC);
		}
	} else if (((val & OCW2_SL) != 0U) && i8259->rotate) {
		/* specific priority */
		i8259->lowprio = val & 0x7U;
	} else {
		/* TODO: Any action required in this case? */
	}

	return 0;
}

static int32_t vpic_ocw3(const struct acrn_vpic *vpic, struct i8259_reg_state *i8259, uint8_t val)
{
	dev_dbg(DBG_LEVEL_PIC, "vm 0x%x: i8259 ocw3 0x%x\n", vpic2vm(vpic), val);

	if ((val & OCW3_ESMM) != 0U) {
		i8259->smm = ((val & OCW3_SMM) != 0U) ? 1U : 0U;
		dev_dbg(DBG_LEVEL_PIC, "%s i8259 special mask mode %s\n",
		    primary_pic(vpic, i8259) ? "primary vPIC" : "secondary vPIC",
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
 * @brief Set vPIC IRQ line status.
 *
 * @param[in] vpic      Pointer to virtual pic structure
 * @param[in] irqline   Target IRQ number
 * @param[in] operation action options:GSI_SET_HIGH/GSI_SET_LOW/
 *			GSI_RAISING_PULSE/GSI_FALLING_PULSE
 */
void vpic_set_irqline(struct acrn_vpic *vpic, uint32_t vgsi, uint32_t operation)
{
	struct i8259_reg_state *i8259;
	uint32_t pin;
	uint64_t rflags;

	if (vgsi < NR_VPIC_PINS_TOTAL) {
		i8259 = &vpic->i8259[vgsi >> 3U];

		if (i8259->ready) {
			pin = vgsi_to_vpin(vpic2vm(vpic), vgsi);
			spinlock_irqsave_obtain(&(vpic->lock), &rflags);
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
			spinlock_irqrestore_release(&(vpic->lock), rflags);
		}
	}
}

uint32_t
vpic_pincount(void)
{
	return NR_VPIC_PINS_TOTAL;
}

/**
 * @pre vm->vpic != NULL
 * @pre irqline < NR_VPIC_PINS_TOTAL
 * @pre this function should be called after vpic_init()
 */
void vpic_get_irqline_trigger_mode(const struct acrn_vpic *vpic, uint32_t vgsi,
		enum vpic_trigger *trigger)
{
	uint32_t irqline = vgsi_to_vpin(vpic2vm(vpic), vgsi);
	
	if ((vpic->i8259[irqline >> 3U].elc & (1U << (irqline & 0x7U))) != 0U) {
		*trigger = LEVEL_TRIGGER;
	} else {
		*trigger = EDGE_TRIGGER;
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
 */
void vpic_pending_intr(struct acrn_vpic *vpic, uint32_t *vecptr)
{
	struct i8259_reg_state *i8259;
	uint32_t pin;
	uint64_t rflags;

	i8259 = &vpic->i8259[0];

	spinlock_irqsave_obtain(&(vpic->lock), &rflags);

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

	spinlock_irqrestore_release(&(vpic->lock), rflags);
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
 * @pre vm != NULL
 * @pre this function should be called after vpic_init()
 */
void vpic_intr_accepted(struct acrn_vpic *vpic, uint32_t vector)
{
	uint32_t pin;
	uint64_t rflags;

	spinlock_irqsave_obtain(&(vpic->lock), &rflags);

	pin = (vector & 0x7U);

	if ((vector & ~0x7U) == vpic->i8259[1].irq_base) {
		vpic_pin_accepted(&vpic->i8259[1], pin);
		/*
		 * If this vector originated from the secondary vPIC,
		 * accept the cascaded interrupt too.
		 */
		vpic_pin_accepted(&vpic->i8259[0], 2U);
	} else {
		vpic_pin_accepted(&vpic->i8259[0], pin);
	}

	vpic_notify_intr(vpic);

	spinlock_irqrestore_release(&(vpic->lock), rflags);
}

static int32_t vpic_read(struct acrn_vpic *vpic, struct i8259_reg_state *i8259,
		uint16_t port, uint32_t *eax)
{
	uint32_t pin;
	uint64_t rflags;

	spinlock_irqsave_obtain(&(vpic->lock), &rflags);

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

	spinlock_irqrestore_release(&(vpic->lock), rflags);

	return 0;
}

static int32_t vpic_write(struct acrn_vpic *vpic, struct i8259_reg_state *i8259,
		uint16_t port, uint32_t *eax)
{
	int32_t error;
	uint8_t val;
	uint64_t rflags;

	error = 0;
	val = (uint8_t)*eax;

	spinlock_irqsave_obtain(&(vpic->lock), &rflags);

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

	spinlock_irqrestore_release(&(vpic->lock), rflags);

	return error;
}

static int32_t vpic_primary_handler(struct acrn_vpic *vpic, bool in, uint16_t port,
		size_t bytes, uint32_t *eax)
{
	struct i8259_reg_state *i8259;
	int32_t ret;

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

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vpic_primary_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t width)
{
	struct acrn_pio_request *pio_req = &vcpu->req.reqs.pio_request;

	if (vpic_primary_handler(vm_pic(vcpu->vm), true, addr, width, &pio_req->value) < 0) {
		pr_err("Primary vPIC read port 0x%x width=%d failed\n",
				addr, width);
	}

	return true;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vpic_primary_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t width,
				uint32_t v)
{
	uint32_t val = v;

	if (vpic_primary_handler(vm_pic(vcpu->vm), false, addr, width, &val) < 0) {
		pr_err("%s: write port 0x%x width=%d value 0x%x failed\n",
				__func__, addr, width, val);
	}

	return true;
}

static int32_t vpic_secondary_handler(struct acrn_vpic *vpic, bool in, uint16_t port,
		size_t bytes, uint32_t *eax)
{
	struct i8259_reg_state *i8259;
	int32_t ret;

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

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vpic_secondary_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t width)
{
	struct acrn_pio_request *pio_req = &vcpu->req.reqs.pio_request;

	if (vpic_secondary_handler(vm_pic(vcpu->vm), true, addr, width, &pio_req->value) < 0) {
		pr_err("Secondary vPIC read port 0x%x width=%d failed\n",
				addr, width);
	}
	return true;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vpic_secondary_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t width,
				uint32_t v)
{
	uint32_t val = v;

	if (vpic_secondary_handler(vm_pic(vcpu->vm), false, addr, width, &val) < 0) {
		pr_err("%s: write port 0x%x width=%d value 0x%x failed\n",
				__func__, addr, width, val);
	}

	return true;
}

static int32_t vpic_elc_handler(struct acrn_vpic *vpic, bool in, uint16_t port, size_t bytes,
		uint32_t *eax)
{
	bool is_primary_vpic;
	int32_t ret;

	is_primary_vpic = (port == IO_ELCR1);

	if (bytes == 1U) {
		uint64_t rflags;

		spinlock_irqsave_obtain(&(vpic->lock), &rflags);

		if (in) {
			if (is_primary_vpic) {
				*eax = vpic->i8259[0].elc;
			} else {
				*eax = vpic->i8259[1].elc;
			}
		} else {
			/*
			 * For the primary vPIC the cascade channel (IRQ2), the
			 * heart beat timer (IRQ0), and the keyboard
			 * controller (IRQ1) cannot be programmed for level
			 * mode.
			 *
			 * For the secondary vPIC the real time clock (IRQ8) and
			 * the floating point error interrupt (IRQ13) cannot
			 * be programmed for level mode.
			 */
			if (is_primary_vpic) {
				vpic->i8259[0].elc = (uint8_t)(*eax & 0xf8U);
			} else {
				vpic->i8259[1].elc = (uint8_t)(*eax & 0xdeU);
			}
		}

		spinlock_irqrestore_release(&(vpic->lock), rflags);
		ret = 0;
	} else {
	        ret = -1;
	}

	return ret;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vpic_elc_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t width)
{
	struct acrn_pio_request *pio_req = &vcpu->req.reqs.pio_request;

	if (vpic_elc_handler(vm_pic(vcpu->vm), true, addr, width, &pio_req->value) < 0) {
		pr_err("pic elc read port 0x%x width=%d failed", addr, width);
	}

	return true;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vpic_elc_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t width,
				uint32_t v)
{
	uint32_t val = v;

	if (vpic_elc_handler(vm_pic(vcpu->vm), false, addr, width, &val) < 0) {
		pr_err("%s: write port 0x%x width=%d value 0x%x failed\n",
				__func__, addr, width, val);
	}

	return true;
}

static void vpic_register_io_handler(struct acrn_vm *vm)
{
	struct vm_io_range primary_vPIC_range = {
		.base = 0x20U,
		.len = 2U
	};
	struct vm_io_range secondary_vPIC_range = {
		.base = 0xa0U,
		.len = 2U
	};
	struct vm_io_range elcr_range = {
		.base = 0x4d0U,
		.len = 2U
	};

	register_pio_emulation_handler(vm, PIC_PRIMARY_PIO_IDX, &primary_vPIC_range,
			vpic_primary_io_read, vpic_primary_io_write);
	register_pio_emulation_handler(vm, PIC_SECONDARY_PIO_IDX, &secondary_vPIC_range,
			vpic_secondary_io_read, vpic_secondary_io_write);
	register_pio_emulation_handler(vm, PIC_ELC_PIO_IDX, &elcr_range,
			vpic_elc_io_read, vpic_elc_io_write);
}

void vpic_init(struct acrn_vm *vm)
{
	struct acrn_vpic *vpic = vm_pic(vm);
	vpic_register_io_handler(vm);
	vpic->i8259[0].mask = 0xffU;
	vpic->i8259[1].mask = 0xffU;

	spinlock_init(&(vpic->lock));
}
