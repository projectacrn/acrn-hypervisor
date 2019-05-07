/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_IRQ_H
#define ARCH_IRQ_H

#include <acrn_common.h>
#include <util.h>
#include <spinlock.h>

/**
 * @file arch/x86/irq.h
 *
 * @brief public APIs for virtual IRQ
 */

#define ACRN_DBG_PTIRQ		6U
#define ACRN_DBG_IRQ		6U

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

#define DEFAULT_DEST_MODE	IOAPIC_RTE_DESTMODE_LOGICAL
#define DEFAULT_DELIVERY_MODE	IOAPIC_RTE_DELMODE_LOPRI
#define ALL_CPUS_MASK		(uint32_t) (((uint32_t)1U << (uint32_t) get_pcpu_nums()) - (uint32_t)1U)

#define IRQ_ALLOC_BITMAP_SIZE	INT_DIV_ROUNDUP(NR_IRQS, 64U)

#define INVALID_INTERRUPT_PIN	0xffffffffU

#define IRQF_NONE	(0U)
#define IRQF_LEVEL	(1U << 1U)	/* 1: level trigger; 0: edge trigger */
#define IRQF_PT		(1U << 2U)	/* 1: for passthrough dev */

struct acrn_vcpu;

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

void setup_notification(void);
void setup_posted_intr_notification(void);

typedef void (*spurious_handler_t)(uint32_t vector);
extern spurious_handler_t spurious_handler;

uint32_t alloc_irq_num(uint32_t req_irq);
uint32_t alloc_irq_vector(uint32_t irq);

/* RFLAGS */
#define HV_ARCH_VCPU_RFLAGS_IF              (1UL<<9U)
#define HV_ARCH_VCPU_RFLAGS_RF              (1UL<<16U)

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

void cancel_event_injection(struct acrn_vcpu *vcpu);

extern uint64_t irq_alloc_bitmap[IRQ_ALLOC_BITMAP_SIZE];

typedef void (*irq_action_t)(uint32_t irq, void *priv_data);

/**
 * @brief Interrupt descriptor
 *
 * Any field change in below required lock protection with irqsave
 */
struct irq_desc {
	uint32_t irq;		/**< index to irq_desc_base */
	uint32_t vector;	/**< assigned vector */

	irq_action_t action;	/**< callback registered from component */
	void *priv_data;	/**< irq_action private data */
	uint32_t flags;		/**< flags for trigger mode/ptdev */

	spinlock_t lock;
#ifdef PROFILING_ON
	uint64_t ctx_rip;
	uint64_t ctx_rflags;
	uint64_t ctx_cs;
#endif
};

/**
 * @defgroup phys_int_ext_apis Physical Interrupt External Interfaces
 *
 * This is a group that includes Physical Interrupt External Interfaces.
 *
 * @{
 */

/**
 * @brief Request an interrupt
 *
 * Request interrupt num if not specified, and register irq action for the
 * specified/allocated irq.
 *
 * @param[in]	req_irq	irq_num to request, if IRQ_INVALID, a free irq
 *		number will be allocated
 * @param[in]	action_fn	Function to be called when the IRQ occurs
 * @param[in]	priv_data	Private data for action function.
 * @param[in]	flags	Interrupt type flags, including:
 *			IRQF_NONE;
 *			IRQF_LEVEL - 1: level trigger; 0: edge trigger;
 *			IRQF_PT    - 1: for passthrough dev
 *
 * @retval >=0 on success
 * @retval IRQ_INVALID on failure
 */
int32_t request_irq(uint32_t req_irq, irq_action_t action_fn, void *priv_data,
			uint32_t flags);

/**
 * @brief Free an interrupt
 *
 * Free irq num and unregister the irq action.
 *
 * @param[in]	irq	irq_num to be freed
 */
void free_irq(uint32_t irq);

/**
 * @brief Set interrupt trigger mode
 *
 * Set the irq trigger mode: edge-triggered or level-triggered
 *
 * @param[in]	irq	irq_num of interrupt to be set
 * @param[in]	is_level_triggered	Trigger mode to set
 */
void set_irq_trigger_mode(uint32_t irq, bool is_level_triggered);

/**
 * @brief Get vector number of an interrupt from irq number
 *
 * @param[in]	irq	The irq_num to convert
 *
 * @return vector number
 */
uint32_t irq_to_vector(uint32_t irq);

/**
 * @brief Dispatch interrupt
 *
 * To dispatch an interrupt, an action callback will be called if registered.
 *
 * @param ctx Pointer to interrupt exception context
 */
void dispatch_interrupt(const struct intr_excp_ctx *ctx);

/**
 * @brief Initialize interrupt
 *
 * To do interrupt initialization for a cpu, will be called for each physical cpu.
 *
 * @param[in]	pcpu_id The id of physical cpu to initialize
 */
void init_interrupt(uint16_t pcpu_id);

/**
 * @}
 */
/* End of phys_int_ext_apis */

/**
 * @}
 */
/* End of acrn_virq */
#endif /* ARCH_IRQ_H */
