/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOAPIC_H
#define IOAPIC_H

/*
 * IOAPIC_MAX_LINES is architecturally defined.
 * The usable RTEs may be a subset of the total on a per IO APIC basis.
 */
#define IOAPIC_MAX_LINES	120U
#define NR_LEGACY_IRQ		16U
#define NR_LEGACY_PIN		NR_LEGACY_IRQ
#define NR_MAX_GSI		(NR_IOAPICS * IOAPIC_MAX_LINES)

void setup_ioapic_irqs(void);

bool irq_is_gsi(uint32_t irq);
uint8_t irq_to_pin(uint32_t irq);
uint32_t pin_to_irq(uint8_t pin);
void ioapic_set_rte(uint32_t irq, union ioapic_rte rte);
void ioapic_get_rte(uint32_t irq, union ioapic_rte *rte);

void suspend_ioapic(void);
void resume_ioapic(void);

void gsi_mask_irq(uint32_t irq);
void gsi_unmask_irq(uint32_t irq);

extern uint8_t pic_ioapic_pin_map[NR_LEGACY_PIN];

#ifdef HV_DEBUG
int get_ioapic_info(char *str_arg, size_t str_max_len);
#endif /* HV_DEBUG */

#endif /* IOAPIC_H */
