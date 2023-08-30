/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_X86_GUEST_VIRQ_H
#define ARCH_X86_GUEST_VIRQ_H

struct acrn_vcpu;
struct acrn_vm;

/**
 * @brief virtual IRQ
 *
 * @addtogroup acrn_virq ACRN vIRQ
 * @{
 */

/**
 * @brief Queue exception to guest.
 *
 * This exception may be injected immediately or later,
 * depends on the exeception class.
 *
 * @param[in] vcpu     Pointer to vCPU.
 * @param[in] vector_arg   Vector of the exeception.
 * @param[in] err_code_arg Error Code to be injected.
 *
 * @retval 0 on success
 * @retval -EINVAL on error that vector is invalid.
 *
 * @pre vcpu != NULL
 */
int32_t vcpu_queue_exception(struct acrn_vcpu *vcpu, uint32_t vector_arg, uint32_t err_code_arg);

/**
 * @brief Inject external interrupt to guest.
 *
 * @param[in] vcpu Pointer to vCPU.
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_extint(struct acrn_vcpu *vcpu);

/**
 * @brief Inject NMI to guest.
 *
 * @param[in] vcpu Pointer to vCPU.
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_nmi(struct acrn_vcpu *vcpu);

/**
 * @brief Inject general protection exeception(GP) to guest.
 *
 * @param[in] vcpu     Pointer to vCPU.
 * @param[in] err_code Error Code to be injected.
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_gp(struct acrn_vcpu *vcpu, uint32_t err_code);

/**
 * @brief Inject page fault exeception(PF) to guest.
 *
 * @param[in] vcpu     Pointer to vCPU.
 * @param[in] addr     Address that result in PF.
 * @param[in] err_code Error Code to be injected.
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_pf(struct acrn_vcpu *vcpu, uint64_t addr, uint32_t err_code);

/**
 * @brief Inject invalid opcode exeception(UD) to guest.
 *
 * @param[in] vcpu Pointer to vCPU.
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_ud(struct acrn_vcpu *vcpu);

/**
 * @brief Inject stack fault exeception(SS) to guest.
 *
 * @param[in] vcpu Pointer to vCPU.
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_ss(struct acrn_vcpu *vcpu);

/**
 * @brief Inject thermal sensor interrupt to guest.
 *
 * @param[in] vcpu Pointer to vCPU.
 *
 * @return None
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_thermal_interrupt(struct acrn_vcpu *vcpu);
void vcpu_make_request(struct acrn_vcpu *vcpu, uint16_t eventid);

/*
 * @pre vcpu != NULL
 */
int32_t exception_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t nmi_window_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t interrupt_window_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t external_interrupt_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t acrn_handle_pending_request(struct acrn_vcpu *vcpu);

/**
 * @}
 */
/* End of acrn_virq */


#endif /* ARCH_X86_GUEST_VIRQ_H */
