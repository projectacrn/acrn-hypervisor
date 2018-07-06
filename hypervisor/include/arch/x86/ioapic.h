/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOAPIC_H
#define IOAPIC_H

/* IOAPIC_MAX_LINES is architecturally defined.
 * The usable RTEs may be a subset of the total on a per IO APIC basis.
 */
#define IOAPIC_MAX_LINES	120U
#define NR_LEGACY_IRQ		16U
#define NR_LEGACY_PIN	NR_LEGACY_IRQ
#define NR_MAX_GSI	(CONFIG_NR_IOAPICS*IOAPIC_MAX_LINES)

#define GSI_MASK_IRQ(irq) irq_gsi_mask_unmask((irq), true)
#define GSI_UNMASK_IRQ(irq) irq_gsi_mask_unmask((irq), false)
#define GSI_SET_RTE(irq, rte) ioapic_set_rte((irq), (rte))

void setup_ioapic_irq(void);

bool irq_is_gsi(uint32_t irq);
uint32_t irq_gsi_num(void);
uint8_t irq_to_pin(uint32_t irq);
uint32_t pin_to_irq(uint8_t pin);
void irq_gsi_mask_unmask(uint32_t irq, bool mask);
void ioapic_set_rte(uint32_t irq, uint64_t rte);
void ioapic_get_rte(uint32_t irq, uint64_t *rte);


void suspend_ioapic(void);
void resume_ioapic(void);

extern uint8_t legacy_irq_to_pin[];
extern uint8_t pic_ioapic_pin_map[];

#ifdef HV_DEBUG
int get_ioapic_info(char *str, int str_max_len);
#endif /* HV_DEBUG */

#endif /* IOAPIC_H */
