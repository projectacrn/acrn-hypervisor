/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_IRQ_H
#define ARCH_IRQ_H

#include <common/irq.h>

/* vectors range for dynamic allocation, usually for devices */
#define VECTOR_DYNAMIC_START	0x20U
#define VECTOR_DYNAMIC_END	0xDFU

/* vectors range for fixed vectors, usually for HV service */
#define VECTOR_FIXED_START	0xE0U
#define VECTOR_FIXED_END	0xFFU

#define VECTOR_TIMER		0xEFU
#define VECTOR_NOTIFY_VCPU	0xF0U
#define VECTOR_VIRT_IRQ_VHM	0xF7U
#define VECTOR_SPURIOUS		0xFFU
#define VECTOR_HYPERVISOR_CALLBACK_VHM	0xF3U

/* the maximum number of msi entry is 2048 according to PCI
 * local bus specification
 */
#define MAX_MSI_ENTRY 0x800U

#define NR_MAX_VECTOR		0xFFU
#define VECTOR_INVALID		(NR_MAX_VECTOR + 1U)
#define NR_IRQS		256U
#define IRQ_INVALID		0xffffffffU

#define TIMER_IRQ		(NR_IRQS - 1U)
#define NOTIFY_IRQ		(NR_IRQS - 2U)

#define DEFAULT_DEST_MODE	IOAPIC_RTE_DESTLOG
#define DEFAULT_DELIVERY_MODE	IOAPIC_RTE_DELLOPRI
#define ALL_CPUS_MASK		((1U << phys_cpu_num) - 1U)

/*
 * Definition of the stack frame layout
 */
struct intr_excp_ctx {
	struct cpu_gp_regs gp_regs;
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
int handle_level_interrupt_common(struct irq_desc *desc,
	__unused void *handler_data);
int common_handler_edge(struct irq_desc *desc, __unused void *handler_data);
int common_dev_handler_level(struct irq_desc *desc,
	__unused void *handler_data);
int quick_handler_nolock(struct irq_desc *desc, __unused void *handler_data);

void init_default_irqs(uint16_t cpu_id);

void dispatch_exception(struct intr_excp_ctx *ctx);
void dispatch_interrupt(struct intr_excp_ctx *ctx);
#ifdef CONFIG_PARTITION_MODE
void partition_mode_dispatch_interrupt(struct intr_excp_ctx *ctx);
#endif

void setup_notification(void);

typedef void (*spurious_handler_t)(uint32_t vector);
extern spurious_handler_t spurious_handler;

uint32_t alloc_irq_num(uint32_t req_irq);
uint32_t alloc_irq_vector(uint32_t irq);

uint32_t irq_to_vector(uint32_t irq);

/*
 * Some MSI message definitions
 */
#define	MSI_ADDR_MASK	0xfff00000U
#define	MSI_ADDR_BASE	0xfee00000U
#define	MSI_ADDR_RH	0x00000008U	/* Redirection Hint */
#define	MSI_ADDR_LOG	0x00000004U	/* Destination Mode */

/* RFLAGS */
#define HV_ARCH_VCPU_RFLAGS_IF              (1U<<9)

/* Interruptability State info */
#define HV_ARCH_VCPU_BLOCKED_BY_MOVSS       (1U<<1)
#define HV_ARCH_VCPU_BLOCKED_BY_STI         (1U<<0)

void vcpu_inject_extint(struct vcpu *vcpu);
void vcpu_inject_nmi(struct vcpu *vcpu);
void vcpu_inject_gp(struct vcpu *vcpu, uint32_t err_code);
void vcpu_inject_pf(struct vcpu *vcpu, uint64_t addr, uint32_t err_code);
void vcpu_inject_ud(struct vcpu *vcpu);
void vcpu_inject_ac(struct vcpu *vcpu);
void vcpu_inject_ss(struct vcpu *vcpu);
void vcpu_make_request(struct vcpu *vcpu, uint16_t eventid);
int vcpu_queue_exception(struct vcpu *vcpu, uint32_t vector, uint32_t err_code);

int exception_vmexit_handler(struct vcpu *vcpu);
int interrupt_window_vmexit_handler(struct vcpu *vcpu);
int external_interrupt_vmexit_handler(struct vcpu *vcpu);
int acrn_handle_pending_request(struct vcpu *vcpu);
void interrupt_init(uint16_t pcpu_id);

void cancel_event_injection(struct vcpu *vcpu);

#ifdef HV_DEBUG
void get_cpu_interrupt_info(char *str_arg, int str_max);
#endif /* HV_DEBUG */

extern uint32_t acrn_vhm_vector;

#endif /* ARCH_IRQ_H */
