/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IOAPIC_H
#define IOAPIC_H

/* IOAPIC_MAX_LINES is architecturally defined.
 * The usable RTEs may be a subset of the total on a per IO APIC basis.
 */
#define IOAPIC_MAX_LINES	120
#define NR_LEGACY_IRQ		16
#define NR_MAX_GSI	(NR_IOAPICS*IOAPIC_MAX_LINES)

#define GSI_MASK_IRQ(irq) irq_gsi_mask_unmask((irq), true)
#define GSI_UNMASK_IRQ(irq) irq_gsi_mask_unmask((irq), false)
#define GSI_SET_RTE(irq, rte) ioapic_set_rte((irq), (rte))

void setup_ioapic_irq(void);
int get_ioapic_info(char *str, int str_max_len);

bool irq_is_gsi(int irq);
int irq_gsi_num(void);
int irq_to_pin(int irq);
int pin_to_irq(int pin);
void irq_gsi_mask_unmask(int irq, bool mask);
void ioapic_set_rte(int irq, uint64_t rte);
void ioapic_get_rte(int irq, uint64_t *rte);

extern uint16_t legacy_irq_to_pin[];
#endif /* IOAPIC_H */
