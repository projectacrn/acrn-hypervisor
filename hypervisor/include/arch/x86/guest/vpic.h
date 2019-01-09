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
 *
 * $FreeBSD$
 */

#ifndef VPIC_H
#define VPIC_H

/**
 * @file vpic.h
 *
 * @brief public APIs for virtual PIC
 */


#define	ICU_IMR_OFFSET	1U

/* Initialization control word 1. Written to even address. */
#define	ICW1_IC4	0x01U		/* ICW4 present */
#define	ICW1_SNGL	0x02U		/* 1 = single, 0 = cascaded */
#define	ICW1_ADI	0x04U		/* 1 = 4, 0 = 8 byte vectors */
#define	ICW1_LTIM	0x08U		/* 1 = level trigger, 0 = edge */
#define	ICW1_RESET	0x10U		/* must be 1 */
/* 0x20 - 0x80 - in 8080/8085 mode only */

/* Initialization control word 2. Written to the odd address. */
/* No definitions, it is the base vector of the IDT for 8086 mode */

/* Initialization control word 3. Written to the odd address. */
/* For a master PIC, bitfield indicating a slave 8259 on given input */
/* For slave, lower 3 bits are the slave's ID binary id on master */

/* Initialization control word 4. Written to the odd address. */
#define	ICW4_8086	0x01U		/* 1 = 8086, 0 = 8080 */
#define	ICW4_AEOI	0x02U		/* 1 = Auto EOI */
#define	ICW4_MS		0x04U		/* 1 = buffered master, 0 = slave */
#define	ICW4_BUF	0x08U		/* 1 = enable buffer mode */
#define	ICW4_SFNM	0x10U		/* 1 = special fully nested mode */

/* Operation control words.  Written after initialization. */

/* Operation control word type 1 */
/*
 * No definitions.  Written to the odd address.  Bitmask for interrupts.
 * 1 = disabled.
 */

/* Operation control word type 2.  Bit 3 (0x08) must be zero. Even address. */
#define	OCW2_L0		0x01U		/* Level */
#define	OCW2_L1		0x02U
#define	OCW2_L2		0x04U
/* 0x08 must be 0 to select OCW2 vs OCW3 */
/* 0x10 must be 0 to select OCW2 vs ICW1 */
#define	OCW2_EOI	0x20U		/* 1 = EOI */
#define	OCW2_SL		0x40U		/* EOI mode */
#define	OCW2_R		0x80U		/* EOI mode */

/* Operation control word type 3.  Bit 3 (0x08) must be set. Even address. */
#define	OCW3_RIS	0x01U		/* 1 = read IS, 0 = read IR */
#define	OCW3_RR		0x02U		/* register read */
#define	OCW3_P		0x04U		/* poll mode command */
/* 0x08 must be 1 to select OCW3 vs OCW2 */
#define	OCW3_SEL	0x08U		/* must be 1 */
/* 0x10 must be 0 to select OCW3 vs ICW1 */
#define	OCW3_SMM	0x20U		/* special mode mask */
#define	OCW3_ESMM	0x40U		/* enable SMM */

#define	IO_ELCR1	0x4d0U
#define	IO_ELCR2	0x4d1U

#define NR_VPIC_PINS_PER_CHIP	8U
#define NR_VPIC_PINS_TOTAL	16U
#define VPIC_INVALID_PIN	0xffU

enum vpic_trigger {
	EDGE_TRIGGER,
	LEVEL_TRIGGER
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

	uint8_t		pin_state[8];	/* pin state for level */
	uint32_t	lowprio;	/* lowest priority irq */

	bool		intr_raised;
	uint8_t		elc;
};

struct acrn_vpic {
	struct acrn_vm		*vm;
	spinlock_t	lock;
	struct i8259_reg_state	i8259[2];
	struct ptirq_remapping_info *vpin_to_pt_entry[NR_VPIC_PINS_TOTAL];
};

void vpic_init(struct acrn_vm *vm);

/**
 * @brief virtual PIC
 *
 * @addtogroup acrn_vpic ACRN vPIC
 * @{
 */

/**
 * @brief Set vPIC IRQ line status.
 *
 * @param[in] vm        Pointer to target VM
 * @param[in] irqline   Target IRQ number
 * @param[in] operation action options:GSI_SET_HIGH/GSI_SET_LOW/
 *			GSI_RAISING_PULSE/GSI_FALLING_PULSE
 *
 * @return None
 */
void vpic_set_irqline(struct acrn_vm *vm, uint32_t irqline, uint32_t operation);

/**
 * @brief Get pending virtual interrupts for vPIC.
 *
 * @param[in]    vm     Pointer to target VM
 * @param[inout] vecptr Pointer to vector buffer and will be filled
 *			with eligible vector if any.
 *
 * @return None
 */
void vpic_pending_intr(struct acrn_vm *vm, uint32_t *vecptr);

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
void vpic_intr_accepted(struct acrn_vm *vm, uint32_t vector);
void vpic_get_irqline_trigger_mode(struct acrn_vm *vm, uint32_t irqline, enum vpic_trigger *trigger);
uint32_t vpic_pincount(void);

/**
 * @}
 */
/* End of acrn_vpic */

#endif /* VPIC_H */
