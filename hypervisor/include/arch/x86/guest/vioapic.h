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

#ifndef _VIOAPIC_H_
#define	_VIOAPIC_H_

#include <apicreg.h>
#include <vm.h>

#define	VIOAPIC_BASE	0xFEC00000UL
#define	VIOAPIC_SIZE	4096UL

struct vioapic *vioapic_init(struct vm *vm);
void	vioapic_cleanup(struct vioapic *vioapic);
void	vioapic_reset(struct vioapic *vioapic);

int	vioapic_assert_irq(struct vm *vm, uint32_t irq);
int	vioapic_deassert_irq(struct vm *vm, uint32_t irq);
int	vioapic_pulse_irq(struct vm *vm, uint32_t irq);
void	vioapic_update_tmr(struct vcpu *vcpu);

void	vioapic_mmio_write(struct vm *vm, uint64_t gpa, uint32_t wval);
void	vioapic_mmio_read(struct vm *vm, uint64_t gpa, uint32_t *rval);

uint8_t	vioapic_pincount(struct vm *vm);
void	vioapic_process_eoi(struct vm *vm, uint32_t vector);
bool	vioapic_get_rte(struct vm *vm, uint8_t pin, union ioapic_rte *rte);
int	vioapic_mmio_access_handler(struct vcpu *vcpu, struct io_request *io_req,
		void *handler_private_data);

#ifdef HV_DEBUG
void get_vioapic_info(char *str, int str_max, uint16_t vmid);
#endif /* HV_DEBUG */

#endif
