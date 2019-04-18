/*-
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
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

#ifndef VLAPIC_PRIV_H
#define VLAPIC_PRIV_H

/*
 * APIC Register:		Offset	Description
 */
#define APIC_OFFSET_ID		0x20U	/* Local APIC ID		*/
#define APIC_OFFSET_VER		0x30U	/* Local APIC Version		*/
#define APIC_OFFSET_TPR		0x80U	/* Task Priority Register	*/
#define APIC_OFFSET_APR		0x90U	/* Arbitration Priority		*/
#define APIC_OFFSET_PPR		0xA0U	/* Processor Priority Register	*/
#define APIC_OFFSET_EOI		0xB0U	/* EOI Register			*/
#define APIC_OFFSET_RRR		0xC0U	/* Remote read			*/
#define APIC_OFFSET_LDR		0xD0U	/* Logical Destination		*/
#define APIC_OFFSET_DFR		0xE0U	/* Destination Format Register	*/
#define APIC_OFFSET_SVR		0xF0U	/* Spurious Vector Register	*/
#define APIC_OFFSET_ISR0	0x100U	/* In Service Register		*/
#define APIC_OFFSET_ISR1	0x110U
#define APIC_OFFSET_ISR2	0x120U
#define APIC_OFFSET_ISR3	0x130U
#define APIC_OFFSET_ISR4	0x140U
#define APIC_OFFSET_ISR5	0x150U
#define APIC_OFFSET_ISR6	0x160U
#define APIC_OFFSET_ISR7	0x170U
#define APIC_OFFSET_TMR0	0x180U	/* Trigger Mode Register	*/
#define APIC_OFFSET_TMR1	0x190U
#define APIC_OFFSET_TMR2	0x1A0U
#define APIC_OFFSET_TMR3	0x1B0U
#define APIC_OFFSET_TMR4	0x1C0U
#define APIC_OFFSET_TMR5	0x1D0U
#define APIC_OFFSET_TMR6	0x1E0U
#define APIC_OFFSET_TMR7	0x1F0U
#define APIC_OFFSET_IRR0	0x200U	/* Interrupt Request Register	*/
#define APIC_OFFSET_IRR1	0x210U
#define APIC_OFFSET_IRR2	0x220U
#define APIC_OFFSET_IRR3	0x230U
#define APIC_OFFSET_IRR4	0x240U
#define APIC_OFFSET_IRR5	0x250U
#define APIC_OFFSET_IRR6	0x260U
#define APIC_OFFSET_IRR7	0x270U
#define APIC_OFFSET_ESR		0x280U	/* Error Status Register	*/
#define APIC_OFFSET_CMCI_LVT	0x2F0U	/* Local Vector Table (CMCI)	*/
#define APIC_OFFSET_ICR_LOW	0x300U	/* Interrupt Command Register	*/
#define APIC_OFFSET_ICR_HI	0x310U
#define APIC_OFFSET_TIMER_LVT	0x320U	/* Local Vector Table (Timer)	*/
#define APIC_OFFSET_THERM_LVT	0x330U	/* Local Vector Table (Thermal)	*/
#define APIC_OFFSET_PERF_LVT	0x340U	/* Local Vector Table (PMC)	*/
#define APIC_OFFSET_LINT0_LVT	0x350U	/* Local Vector Table (LINT0)	*/
#define APIC_OFFSET_LINT1_LVT	0x360U	/* Local Vector Table (LINT1)	*/
#define APIC_OFFSET_ERROR_LVT	0x370U	/* Local Vector Table (ERROR)	*/
#define APIC_OFFSET_TIMER_ICR	0x380U	/* Timer's Initial Count	*/
#define APIC_OFFSET_TIMER_CCR	0x390U	/* Timer's Current Count	*/
#define APIC_OFFSET_TIMER_DCR	0x3E0U	/* Timer's Divide Configuration	*/
#define APIC_OFFSET_SELF_IPI    0x3F0U  /* Self IPI Register */

struct acrn_apicv_ops {
	void (*accept_intr)(struct acrn_vlapic *vlapic, uint32_t vector, bool level);
	bool (*inject_intr)(struct acrn_vlapic *vlapic, bool guest_irq_enabled, bool injected);
	bool (*has_pending_delivery_intr)(struct acrn_vcpu *vcpu);
	bool (*apic_read_access_may_valid)(uint32_t offset);
	bool (*apic_write_access_may_valid)(uint32_t offset);
	bool (*x2apic_read_msr_may_valid)(uint32_t offset);
	bool (*x2apic_write_msr_may_valid)(uint32_t offset);
};

#endif /* VLAPIC_PRIV_H */
