/*-
 * Copyright (c) 2013 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
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

#ifndef VIOAPIC_H
#define VIOAPIC_H

#include <apicreg.h>

#define	VIOAPIC_BASE	0xFEC00000UL
#define	VIOAPIC_SIZE	4096UL

#define REDIR_ENTRIES_HW	120U /* SOS align with native ioapic */
#define STATE_BITMAP_SIZE	INT_DIV_ROUNDUP(REDIR_ENTRIES_HW, 64U)

struct acrn_vioapic {
	struct vm	*vm;
	spinlock_t	mtx;
	uint32_t	id;
	uint32_t	ioregsel;
	union ioapic_rte rtbl[REDIR_ENTRIES_HW];
	/* pin_state status bitmap: 1 - high, 0 - low */
	uint64_t pin_state[STATE_BITMAP_SIZE];
};

void    vioapic_init(struct vm *vm);
void	vioapic_cleanup(const struct acrn_vioapic *vioapic);
void	vioapic_reset(struct acrn_vioapic *vioapic);

void	vioapic_set_irq(struct vm *vm, uint32_t irq, uint32_t operation);
void	vioapic_set_irq_nolock(struct vm *vm, uint32_t irq, uint32_t operation);
void	vioapic_update_tmr(struct vcpu *vcpu);

uint32_t	vioapic_pincount(const struct vm *vm);
void	vioapic_process_eoi(struct vm *vm, uint32_t vector);
void	vioapic_get_rte(struct vm *vm, uint32_t pin, union ioapic_rte *rte);
int	vioapic_mmio_access_handler(struct vcpu *vcpu,
	struct io_request *io_req);

#ifdef HV_DEBUG
void get_vioapic_info(char *str_arg, size_t str_max, uint16_t vmid);
#endif /* HV_DEBUG */

#endif /* VIOAPIC_H */
