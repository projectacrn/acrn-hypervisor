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

#define	VIOAPIC_BASE	0xFEC00000UL
#define	VIOAPIC_SIZE	4096UL

struct vioapic *vioapic_init(struct vm *vm);
void	vioapic_cleanup(struct vioapic *vioapic);

int	vioapic_assert_irq(struct vm *vm, uint32_t irq);
int	vioapic_deassert_irq(struct vm *vm, uint32_t irq);
int	vioapic_pulse_irq(struct vm *vm, uint32_t irq);
void	vioapic_update_tmr(struct vcpu *vcpu);

int	vioapic_mmio_write(void *vm, uint64_t gpa,
	    uint64_t wval, int size);
int	vioapic_mmio_read(void *vm, uint64_t gpa,
	    uint64_t *rval, int size);

int	vioapic_pincount(struct vm *vm);
void	vioapic_process_eoi(struct vm *vm, uint32_t vector);
bool	vioapic_get_rte(struct vm *vm, int pin, void *rte);
int	vioapic_mmio_access_handler(struct vcpu *vcpu, struct mem_io *mmio,
		void *handler_private_data);

void get_vioapic_info(char *str, int str_max, int vmid);
#endif
