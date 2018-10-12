/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef INTR_LAPIC_H
#define INTR_LAPIC_H

/* intr_lapic_icr_delivery_mode */
#define INTR_LAPIC_ICR_FIXED    0x0U
#define INTR_LAPIC_ICR_LP          0x1U
#define INTR_LAPIC_ICR_SMI        0x2U
#define INTR_LAPIC_ICR_NMI       0x4U
#define INTR_LAPIC_ICR_INIT       0x5U
#define INTR_LAPIC_ICR_STARTUP  0x6U

/* intr_lapic_icr_dest_mode */
#define INTR_LAPIC_ICR_PHYSICAL 0x0U
#define INTR_LAPIC_ICR_LOGICAL  0x1U

/* intr_lapic_icr_level */
#define INTR_LAPIC_ICR_DEASSERT  0x0U
#define INTR_LAPIC_ICR_ASSERT       0x1U

/* intr_lapic_icr_trigger */
#define INTR_LAPIC_ICR_EDGE         0x0U
#define INTR_LAPIC_ICR_LEVEL        0x1U

/* intr_lapic_icr_shorthand */
#define INTR_LAPIC_ICR_USE_DEST_ARRAY    0x0U
#define INTR_LAPIC_ICR_SELF                          0x1U
#define INTR_LAPIC_ICR_ALL_INC_SELF         0x2U
#define INTR_LAPIC_ICR_ALL_EX_SELF            0x3U

/* Default LAPIC base */
#define LAPIC_BASE                              0xFEE00000U

/* LAPIC register bit and bitmask definitions */
#define LAPIC_SVR_VECTOR                        0x000000FFU
#define LAPIC_SVR_APIC_ENABLE_MASK                   0x00000100U

#define LAPIC_LVT_MASK                          0x00010000U
#define LAPIC_DELIVERY_MODE_EXTINT_MASK         0x00000700U

/* LAPIC Timer bit and bitmask definitions */
#define LAPIC_TMR_ONESHOT                       ((uint32_t) 0x0U << 17U)
#define LAPIC_TMR_PERIODIC                      ((uint32_t) 0x1U << 17U)
#define LAPIC_TMR_TSC_DEADLINE                  ((uint32_t) 0x2U << 17U)

enum intr_cpu_startup_shorthand {
	INTR_CPU_STARTUP_USE_DEST,
	INTR_CPU_STARTUP_ALL_EX_SELF,
	INTR_CPU_STARTUP_UNKNOWN,
};

void save_lapic(struct lapic_regs *regs);
void early_init_lapic(void);
void init_lapic(void);
void send_lapic_eoi(void);
uint32_t get_cur_lapic_id(void);
void send_startup_ipi(enum intr_cpu_startup_shorthand cpu_startup_shorthand,
		uint16_t dest_pcpu_id,
		uint64_t cpu_startup_start_address);
/* API to send an IPI to multiple pCPUs*/
void send_dest_ipi_mask(uint32_t dest_mask, uint32_t vector);
/* API to send an IPI to a single pCPU */
void send_single_ipi(uint16_t pcpu_id, uint32_t vector);

void suspend_lapic(void);
void resume_lapic(void);

#endif /* INTR_LAPIC_H */
