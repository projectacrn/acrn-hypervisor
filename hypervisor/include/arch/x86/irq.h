/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IRQ_H
#define IRQ_H

/* vectors for normal, usually for devices */
#define VECTOR_FOR_NOR_LOWPRI_START	0x20
#define VECTOR_FOR_NOR_LOWPRI_END	0x7F
#define VECTOR_FOR_NOR_HIGHPRI_START	0x80
#define VECTOR_FOR_NOR_HIGHPRI_END	0xDF
#define VECTOR_FOR_NOR_END		VECTOR_FOR_NOR_HIGHPRI_END

#define VECTOR_FOR_INTR_START		VECTOR_FOR_NOR_LOWPRI_START

/* vectors for priority, usually for HV service */
#define VECTOR_FOR_PRI_START	0xE0
#define VECTOR_FOR_PRI_END	0xFF
#define VECTOR_TIMER		0xEF
#define VECTOR_NOTIFY_VCPU	0xF0
#define VECTOR_VIRT_IRQ_VHM	0xF7
#define VECTOR_SPURIOUS		0xFF

#define NR_MAX_VECTOR		0xFF
#define VECTOR_INVALID		(NR_MAX_VECTOR + 1)
#define IRQ_INVALID		(NR_MAX_IRQS+1)

#define NR_MAX_IRQS (256+16)
#define DEFAULT_DEST_MODE	IOAPIC_RTE_DESTLOG
#define DEFAULT_DELIVERY_MODE	IOAPIC_RTE_DELLOPRI
#define ALL_CPUS_MASK		((1 << phy_cpu_num) - 1)

struct irq_desc;

enum irq_mode {
	IRQ_PULSE,
	IRQ_ASSERT,
	IRQ_DEASSERT,
};

enum irq_state {
	IRQ_NOT_ASSIGNED = 0,
	IRQ_ASSIGNED_SHARED,
	IRQ_ASSIGNED_NOSHARE,
};

enum irq_desc_state {
	IRQ_DESC_PENDING,
	IRQ_DESC_IN_PROCESS,
};

typedef int (*dev_handler_t)(int irq, void*);
struct dev_handler_node {
	char name[32];
	void *dev_data;
	dev_handler_t dev_handler;
	struct dev_handler_node *next;
	struct irq_desc *desc;
};

struct irq_routing_entry {
	unsigned short bdf;	/* BDF */
	int irq;	/* PCI cfg offset 0x3C: IRQ pin */
	int intx;	/* PCI cfg offset 0x3D: 0-3 = INTA,INTB,INTC,INTD*/
};

/*
 * Definition of the stack frame layout
 */
struct intr_excp_ctx {
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rbx;
	uint64_t rbp;

	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;

	uint64_t vector;
	uint64_t error_code;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

int irq_mark_used(int irq);
int irq_alloc(void);

int irq_desc_alloc_vector(int irq, bool lowpri);
void irq_desc_try_free_vector(int irq);

int irq_to_vector(int irq);
int dev_to_irq(struct dev_handler_node *node);
int dev_to_vector(struct dev_handler_node *node);

int handle_level_interrupt_common(struct irq_desc *desc, void *handler_data);
int common_handler_edge(struct irq_desc *desc, void *handler_data);
int common_dev_handler_level(struct irq_desc *desc, void *handler_data);
int quick_handler_nolock(struct irq_desc *desc, void *handler_data);

typedef int (*irq_handler_t)(struct irq_desc*, void*);
void update_irq_handler(int irq, irq_handler_t func);

int init_default_irqs(unsigned int cpu);

void dispatch_interrupt(struct intr_excp_ctx *ctx);

struct dev_handler_node*
pri_register_handler(int irq,
		int vector,
		dev_handler_t func,
		void *dev_data,
		const char *name);

struct dev_handler_node*
normal_register_handler(int irq,
		dev_handler_t func,
		void *dev_data,
		bool share,
		bool lowpri,
		const char *name);
void unregister_handler_common(struct dev_handler_node *node);

int get_cpu_interrupt_info(char *str, int str_max);

void setup_notification(void);

typedef void (*spurious_handler_t)(int);
extern spurious_handler_t spurious_handler;

/*
 * Some MSI message definitions
 */
#define	MSI_ADDR_MASK	0xfff00000
#define	MSI_ADDR_BASE	0xfee00000
#define	MSI_ADDR_RH	0x00000008	/* Redirection Hint */
#define	MSI_ADDR_LOG	0x00000004	/* Destination Mode */

/* RFLAGS */
#define HV_ARCH_VCPU_RFLAGS_IF              (1<<9)

/* Interruptability State info */
#define HV_ARCH_VCPU_BLOCKED_BY_MOVSS       (1<<1)
#define HV_ARCH_VCPU_BLOCKED_BY_STI         (1<<0)

int vcpu_inject_extint(struct vcpu *vcpu);
int vcpu_inject_nmi(struct vcpu *vcpu);
int vcpu_inject_gp(struct vcpu *vcpu);
int vcpu_make_request(struct vcpu *vcpu, int eventid);
int vcpu_queue_exception(struct vcpu *vcpu, int32_t vector, uint32_t err_code);

int exception_vmexit_handler(struct vcpu *vcpu);
int interrupt_window_vmexit_handler(struct vcpu *vcpu);
int external_interrupt_vmexit_handler(struct vcpu *vcpu);
int acrn_handle_pending_request(struct vcpu *vcpu);
int interrupt_init(uint32_t logical_id);

void cancel_event_injection(struct vcpu *vcpu);
#endif /* IRQ_H */
