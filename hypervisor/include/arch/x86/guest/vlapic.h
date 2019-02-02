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

#ifndef VLAPIC_H
#define VLAPIC_H

#include <page.h>
#include <timer.h>
#include <apicreg.h>


/**
 * @file vlapic.h
 *
 * @brief public APIs for virtual LAPIC
 */


/*
 * 16 priority levels with at most one vector injected per level.
 */
#define	ISRVEC_STK_SIZE		(16U + 1U)

#define VLAPIC_MAXLVT_INDEX	APIC_LVT_CMCI

struct vlapic_pir_desc {
	uint64_t pir[4];
	uint64_t pending;
	uint64_t unused[3];
} __aligned(64);

struct vlapic_timer {
	struct hv_timer timer;
	uint32_t mode;
	uint32_t tmicr;
	uint32_t divisor_shift;
};

struct acrn_vlapic {
	/*
	 * Please keep 'apic_page' and 'pir_desc' be the first two fields in
	 * current structure, as below alignment restrictions are mandatory
	 * to support APICv features:
	 * - 'apic_page' MUST be 4KB aligned.
	 * - 'pir_desc' MUST be 64 bytes aligned.
	 */
	struct lapic_regs	apic_page;
	struct vlapic_pir_desc	pir_desc;

	struct acrn_vm		*vm;
	struct acrn_vcpu		*vcpu;

	uint32_t		esr_pending;
	int32_t			esr_firing;

	struct vlapic_timer	vtimer;

	/*
	 * The 'isrvec_stk' is a stack of vectors injected by the local apic.
	 * A vector is popped from the stack when the processor does an EOI.
	 * The vector on the top of the stack is used to compute the
	 * Processor Priority in conjunction with the TPR.
	 *
	 * Note: isrvec_stk_top is unsigned and always equal to the number of
	 * vectors in the stack.
	 *
	 * Operations:
	 *     init: isrvec_stk_top = 0;
	 *     push: isrvec_stk_top++; isrvec_stk[isrvec_stk_top] = x;
	 *     pop : isrvec_stk_top--;
	 */
	uint8_t		isrvec_stk[ISRVEC_STK_SIZE];
	uint32_t	isrvec_stk_top;

	uint64_t	msr_apicbase;

	/*
	 * Copies of some registers in the virtual APIC page. We do this for
	 * a couple of different reasons:
	 * - to be able to detect what changed (e.g. svr_last)
	 * - to maintain a coherent snapshot of the register (e.g. lvt_last)
	 */
	uint32_t	svr_last;
	uint32_t	lvt_last[VLAPIC_MAXLVT_INDEX + 1];
} __aligned(PAGE_SIZE);


/* APIC write handlers */
void vlapic_set_cr8(struct acrn_vlapic *vlapic, uint64_t val);
uint64_t vlapic_get_cr8(const struct acrn_vlapic *vlapic);

/**
 * @brief virtual LAPIC
 *
 * @addtogroup acrn_vlapic ACRN vLAPIC
 * @{
 */


/**
 * @brief Find a deliverable virtual interrupts for vLAPIC in irr.
 *
 * @param[in]    vlapic Pointer to target vLAPIC data structure
 * @param[inout] vecptr Pointer to vector buffer and will be filled
 *               with eligible vector if any.
 *
 * @retval false There is no deliverable pending vector.
 * @retval true There is deliverable vector.
 *
 * @remark The vector does not automatically transition to the ISR as a
 *	   result of calling this function.
 */
bool vlapic_find_deliverable_intr(const struct acrn_vlapic *vlapic, uint32_t *vecptr);

/**
 * @brief Get a deliverable virtual interrupt from irr to isr.
 *
 * Transition 'vector' from IRR to ISR. This function is called with the
 * vector returned by 'vlapic_find_deliverable_intr()' when the guest is able to
 * accept this interrupt (i.e. RFLAGS.IF = 1 and no conditions exist that
 * block interrupt delivery).
 *
 * @param[in] vlapic Pointer to target vLAPIC data structure
 * @param[in] vector Target virtual interrupt vector
 *
 * @return None
 *
 * @pre vlapic != NULL
 */
void vlapic_get_deliverable_intr(struct acrn_vlapic *vlapic, uint32_t vector);

/**
 * @brief Send notification vector to target pCPU.
 *
 * If APICv Posted-Interrupt is enabled and target pCPU is in non-root mode,
 * pCPU will sync pending virtual interrupts from PIR to vIRR automatically,
 * without VM exit.
 * If pCPU in root-mode, virtual interrupt will be injected in next VM entry.
 *
 * @param[in] dest_pcpu_id Target CPU ID.
 *
 * @return None
 */
void vlapic_post_intr(uint16_t dest_pcpu_id);

/**
 * @brief Get physical address to PIR description.
 *
 * If APICv Posted-interrupt is supported, this address will be configured
 * to VMCS "Posted-interrupt descriptor address" field.
 *
 * @param[in] vcpu Target vCPU
 *
 * @return physicall address to PIR
 *
 * @pre vcpu != NULL
 */
uint64_t apicv_get_pir_desc_paddr(struct acrn_vcpu *vcpu);

int32_t vlapic_rdmsr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *rval);
int32_t vlapic_wrmsr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t wval);

/*
 * Signals to the LAPIC that an interrupt at 'vector' needs to be generated
 * to the 'cpu', the state is recorded in IRR.
 *  @pre vcpu != NULL
 *  @pre vector <= 255U
 */
void vlapic_set_intr(struct acrn_vcpu *vcpu, uint32_t vector, bool level);

#define	LAPIC_TRIG_LEVEL	true
#define	LAPIC_TRIG_EDGE		false

/**
 * @brief Triggers LAPIC local interrupt(LVT).
 *
 * @param[in] vm           Pointer to VM data structure
 * @param[in] vcpu_id_arg  ID of vCPU, BROADCAST_CPU_ID means triggering
 *			   interrupt to all vCPUs.
 * @param[in] vector       Vector to be fired.
 *
 * @retval 0 on success.
 * @retval -EINVAL on error that vcpu_id_arg or vector is invalid.
 *
 * @pre vm != NULL
 */
int32_t vlapic_set_local_intr(struct acrn_vm *vm, uint16_t vcpu_id_arg, uint32_t vector);

/**
 * @brief Inject MSI to target VM.
 *
 * @param[in] vm   Pointer to VM data structure
 * @param[in] addr MSI address.
 * @param[in] msg  MSI data.
 *
 * @retval 0 on success.
 * @retval -1 on error that addr is invalid.
 *
 * @pre vm != NULL
 */
int32_t vlapic_intr_msi(struct acrn_vm *vm, uint64_t addr, uint64_t msg);

void vlapic_receive_intr(struct acrn_vm *vm, bool level, uint32_t dest,
		bool phys, uint32_t delmode, uint32_t vec, bool rh);

uint32_t vlapic_get_apicid(const struct acrn_vlapic *vlapic);
int32_t vlapic_create(struct acrn_vcpu *vcpu);
/*
 *  @pre vcpu != NULL
 */
void vlapic_free(struct acrn_vcpu *vcpu);
/**
 * @pre vlapic->vm != NULL
 * @pre vlapic->vcpu->vcpu_id < CONFIG_MAX_VCPUS_PER_VM
 */
void vlapic_init(struct acrn_vlapic *vlapic);
void vlapic_reset(struct acrn_vlapic *vlapic);
void vlapic_restore(struct acrn_vlapic *vlapic, const struct lapic_regs *regs);
bool vlapic_enabled(const struct acrn_vlapic *vlapic);
uint64_t vlapic_apicv_get_apic_access_addr(void);
uint64_t vlapic_apicv_get_apic_page_addr(struct acrn_vlapic *vlapic);
void vlapic_apicv_inject_pir(struct acrn_vlapic *vlapic);
int32_t apic_access_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t apic_write_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t veoi_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t tpr_below_threshold_vmexit_handler(__unused struct acrn_vcpu *vcpu);
void vlapic_calc_dest(struct acrn_vm *vm, uint64_t *dmask, uint32_t dest, bool phys, bool lowprio);
void vlapic_calc_dest_lapic_pt(struct acrn_vm *vm, uint64_t *dmask, uint32_t dest, bool phys);

/**
 * @}
 */
/* End of acrn_vlapic */
#endif /* VLAPIC_H */
