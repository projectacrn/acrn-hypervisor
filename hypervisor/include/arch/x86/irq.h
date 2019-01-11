/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_IRQ_H
#define ARCH_IRQ_H

/**
 * @file arch/x86/irq.h
 *
 * @brief public APIs for virtual IRQ
 */


#include <common/irq.h>

/* vectors range for dynamic allocation, usually for devices */
#define VECTOR_DYNAMIC_START	0x20U
#define VECTOR_DYNAMIC_END	0xDFU

/* vectors range for fixed vectors, usually for HV service */
#define VECTOR_FIXED_START	0xE0U
#define VECTOR_FIXED_END	0xFFU

#define VECTOR_TIMER		0xEFU
#define VECTOR_NOTIFY_VCPU	0xF0U
#define VECTOR_POSTED_INTR	0xF2U
#define VECTOR_VIRT_IRQ_VHM	0xF7U
#define VECTOR_SPURIOUS		0xFFU
#define VECTOR_HYPERVISOR_CALLBACK_VHM	0xF3U
#define VECTOR_PMI			0xF4U

/* the maximum number of msi entry is 2048 according to PCI
 * local bus specification
 */
#define MAX_MSI_ENTRY 0x800U

#define NR_MAX_VECTOR		0xFFU
#define VECTOR_INVALID		(NR_MAX_VECTOR + 1U)
#define NR_IRQS		256U
#define IRQ_INVALID		0xffffffffU

#define NR_STATIC_MAPPINGS     (4U)
#define TIMER_IRQ		(NR_IRQS - 1U)
#define NOTIFY_IRQ		(NR_IRQS - 2U)
#define POSTED_INTR_NOTIFY_IRQ	(NR_IRQS - 3U)
#define PMI_IRQ			(NR_IRQS - 4U)

#define DEFAULT_DEST_MODE	IOAPIC_RTE_DESTLOG
#define DEFAULT_DELIVERY_MODE	IOAPIC_RTE_DELLOPRI
#define ALL_CPUS_MASK		((1UL << (uint64_t)get_pcpu_nums()) - 1UL)

#define IRQ_ALLOC_BITMAP_SIZE	INT_DIV_ROUNDUP(NR_IRQS, 64U)

#define INVALID_INTERRUPT_PIN	0xffffffffU

/*
 * Definition of the stack frame layout
 */
struct intr_excp_ctx {
	struct acrn_gp_regs gp_regs;
	uint64_t vector;
	uint64_t error_code;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t ss;
};

typedef void (*smp_call_func_t)(void *data);
struct smp_call_info_data {
	smp_call_func_t func;
	void *data;
};

void smp_call_function(uint64_t mask, smp_call_func_t func, void *data);

void init_default_irqs(uint16_t cpu_id);

void dispatch_exception(struct intr_excp_ctx *ctx);
void dispatch_interrupt(const struct intr_excp_ctx *ctx);
#ifdef CONFIG_PARTITION_MODE
void partition_mode_dispatch_interrupt(struct intr_excp_ctx *ctx);
#endif

void setup_notification(void);
void setup_posted_intr_notification(void);

typedef void (*spurious_handler_t)(uint32_t vector);
extern spurious_handler_t spurious_handler;

uint32_t alloc_irq_num(uint32_t req_irq);
uint32_t alloc_irq_vector(uint32_t irq);


/**
 * @brief Get vector number of an interupt from irq number
 *
 * @param[in]	irq	The irq_num to convert
 */
uint32_t irq_to_vector(uint32_t irq);

/*
 * Some MSI message definitions
 */
#define	MSI_ADDR_MASK	0xfff00000UL
#define	MSI_ADDR_BASE	0xfee00000UL
#define	MSI_ADDR_RH	0x00000008UL	/* Redirection Hint */
#define	MSI_ADDR_LOG	0x00000004UL	/* Destination Mode */
#define	MSI_ADDR_DEST	0x000FF000UL	/* Destination Field */

#define	MSI_ADDR_DEST_SHIFT	(12U)

/* RFLAGS */
#define HV_ARCH_VCPU_RFLAGS_IF              (1UL<<9U)

/* Interruptability State info */
#define HV_ARCH_VCPU_BLOCKED_BY_MOVSS       (1UL<<1U)
#define HV_ARCH_VCPU_BLOCKED_BY_STI         (1UL<<0U)

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
 * @return None
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_extint(struct acrn_vcpu *vcpu);

/**
 * @brief Inject NMI to guest.
 *
 * @param[in] vcpu Pointer to vCPU.
 *
 * @return None
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
 * @return None
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
 * @return None
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_pf(struct acrn_vcpu *vcpu, uint64_t addr, uint32_t err_code);

/**
 * @brief Inject invalid opcode exeception(UD) to guest.
 *
 * @param[in] vcpu Pointer to vCPU.
 *
 * @return None
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_ud(struct acrn_vcpu *vcpu);

/**
 * @brief Inject alignment check exeception(AC) to guest.
 *
 * @param[in] vcpu Pointer to vCPU.
 *
 * @return None
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_ac(struct acrn_vcpu *vcpu);

/**
 * @brief Inject stack fault exeception(SS) to guest.
 *
 * @param[in] vcpu Pointer to vCPU.
 *
 * @return None
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_ss(struct acrn_vcpu *vcpu);
void vcpu_make_request(struct acrn_vcpu *vcpu, uint16_t eventid);

/*
 * @pre vcpu != NULL
 */
int32_t exception_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t interrupt_window_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t external_interrupt_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t acrn_handle_pending_request(struct acrn_vcpu *vcpu);

/**
 * @brief Initialize the interrupt
 *
 * To do interrupt initialization for a cpu, will be called for each physical cpu.
 *
 * @param[in]	pcpu_id The id of physical cpu to initialize
 */
void interrupt_init(uint16_t pcpu_id);

void cancel_event_injection(struct acrn_vcpu *vcpu);

extern uint32_t acrn_vhm_vector;
extern uint64_t irq_alloc_bitmap[IRQ_ALLOC_BITMAP_SIZE];

/**
 * @}
 */
/* End of acrn_virq */
#endif /* ARCH_IRQ_H */
