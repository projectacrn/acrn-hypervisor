/*-
 * Copyright (c) 2011 NetApp, Inc.
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

#ifndef _VLAPIC_H_
#define	_VLAPIC_H_

struct vlapic;

/* APIC write handlers */
void vlapic_set_cr8(struct vlapic *vlapic, uint64_t val);
uint64_t vlapic_get_cr8(struct vlapic *vlapic);

/*
 * Returns 0 if there is no eligible vector that can be delivered to the
 * guest at this time and non-zero otherwise.
 *
 * If an eligible vector number is found and 'vecptr' is not NULL then it will
 * be stored in the location pointed to by 'vecptr'.
 *
 * Note that the vector does not automatically transition to the ISR as a
 * result of calling this function.
 */
int vlapic_pending_intr(struct vlapic *vlapic, uint32_t *vecptr);

/*
 * Transition 'vector' from IRR to ISR. This function is called with the
 * vector returned by 'vlapic_pending_intr()' when the guest is able to
 * accept this interrupt (i.e. RFLAGS.IF = 1 and no conditions exist that
 * block interrupt delivery).
 */
void vlapic_intr_accepted(struct vlapic *vlapic, uint32_t vector);

struct vlapic *vm_lapic_from_vcpuid(struct vm *vm, int vcpu_id);
struct vlapic *vm_lapic_from_pcpuid(struct vm *vm, uint16_t pcpu_id);
bool vlapic_msr(uint32_t num);
int vlapic_rdmsr(struct vcpu *vcpu, uint32_t msr, uint64_t *rval);
int vlapic_wrmsr(struct vcpu *vcpu, uint32_t msr, uint64_t wval);

int vlapic_mmio_read(struct vcpu *vcpu, uint64_t gpa, uint64_t *rval, int size);
int vlapic_mmio_write(struct vcpu *vcpu, uint64_t gpa, uint64_t wval, int size);

/*
 * Signals to the LAPIC that an interrupt at 'vector' needs to be generated
 * to the 'cpu', the state is recorded in IRR.
 */
int vlapic_set_intr(struct vcpu *vcpu, uint32_t vector, bool trig);

#define	LAPIC_TRIG_LEVEL	true
#define	LAPIC_TRIG_EDGE		false
static inline int
vlapic_intr_level(struct vcpu *vcpu, uint32_t vector)
{
	return vlapic_set_intr(vcpu, vector, LAPIC_TRIG_LEVEL);
}

static inline int
vlapic_intr_edge(struct vcpu *vcpu, uint32_t vector)
{
	return vlapic_set_intr(vcpu, vector, LAPIC_TRIG_EDGE);
}

/*
 * Triggers the LAPIC local interrupt (LVT) 'vector' on 'cpu'.  'cpu' can
 * be set to -1 to trigger the interrupt on all CPUs.
 */
int vlapic_set_local_intr(struct vm *vm, int vcpu_id, uint32_t vector);

int vlapic_intr_msi(struct vm *vm, uint64_t addr, uint64_t msg);

void vlapic_deliver_intr(struct vm *vm, bool level, uint32_t dest,
		bool phys, int delmode, int vec);

/* Reset the trigger-mode bits for all vectors to be edge-triggered */
void vlapic_reset_tmr(struct vlapic *vlapic);

/*
 * Set the trigger-mode bit associated with 'vector' to level-triggered if
 * the (dest,phys,delmode) tuple resolves to an interrupt being delivered to
 * this 'vlapic'.
 */
void vlapic_set_tmr_one_vec(struct vlapic *vlapic, int delmode,
		uint32_t vector, bool level);

void
vlapic_apicv_batch_set_tmr(struct vlapic *vlapic);

int vlapic_mmio_access_handler(struct vcpu *vcpu, struct mem_io *mmio,
		void *handler_private_data);

uint32_t vlapic_get_id(struct vlapic *vlapic);
uint8_t vlapic_get_apicid(struct vlapic *vlapic);

int vlapic_create(struct vcpu *vcpu);
void vlapic_free(struct vcpu *vcpu);
void vlapic_init(struct vlapic *vlapic);
void vlapic_reset(struct vlapic *vlapic);
void vlapic_restore(struct vlapic *vlapic, struct lapic_regs *regs);
bool vlapic_enabled(struct vlapic *vlapic);
uint64_t apicv_get_apic_access_addr(struct vm *vm);
uint64_t apicv_get_apic_page_addr(struct vlapic *vlapic);
bool vlapic_apicv_enabled(struct vcpu *vcpu);
void apicv_inject_pir(struct vlapic *vlapic);
int apic_access_vmexit_handler(struct vcpu *vcpu);
int apic_write_vmexit_handler(struct vcpu *vcpu);
int veoi_vmexit_handler(struct vcpu *vcpu);
int tpr_below_threshold_vmexit_handler(struct vcpu *vcpu);

void calcvdest(struct vm *vm, uint64_t *dmask, uint32_t dest, bool phys);
#endif	/* _VLAPIC_H_ */
