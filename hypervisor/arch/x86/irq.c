/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <softirq.h>

static spinlock_t exception_spinlock = { .head = 0U, .tail = 0U, };

static struct irq_desc irq_desc_array[NR_IRQS];
static uint32_t vector_to_irq[NR_MAX_VECTOR + 1];

spurious_handler_t spurious_handler;

static void init_irq_desc(void)
{
	uint32_t i;

	for (i = 0U; i < NR_IRQS; i++) {
		irq_desc_array[i].irq = i;
		irq_desc_array[i].vector = VECTOR_INVALID;
		spinlock_init(&irq_desc_array[i].irq_lock);
	}

	for (i = 0U; i <= NR_MAX_VECTOR; i++) {
		vector_to_irq[i] = IRQ_INVALID;
	}

}

/*
 * find available vector VECTOR_DYNAMIC_START ~ VECTOR_DYNAMIC_END
 */
static uint32_t find_available_vector()
{
	uint32_t i;

	/* TODO: vector lock required */
	for (i = VECTOR_DYNAMIC_START; i <= VECTOR_DYNAMIC_END; i++) {
		if (vector_to_irq[i] == IRQ_INVALID) {
			return i;
		}
	}
	return VECTOR_INVALID;
}

/*
 * check and set irq to be assigned
 * return: IRQ_INVALID if irq already assigned otherwise return irq
 */
uint32_t irq_mark_used(uint32_t irq)
{
	struct irq_desc *desc;

	spinlock_rflags;

	if (irq >= NR_IRQS) {
		return IRQ_INVALID;
	}

	desc = &irq_desc_array[irq];
	spinlock_irqsave_obtain(&desc->irq_lock);
	if (desc->used == IRQ_NOT_ASSIGNED) {
		desc->used = IRQ_ASSIGNED_NOSHARE;
	}
	spinlock_irqrestore_release(&desc->irq_lock);
	return irq;
}

/*
 * system find available irq and set assigned
 * return: irq, VECTOR_INVALID not found
 */
static uint32_t alloc_irq(void)
{
	uint32_t i;
	struct irq_desc *desc;

	spinlock_rflags;

	for (i = irq_gsi_num(); i < NR_IRQS; i++) {
		desc = &irq_desc_array[i];
		spinlock_irqsave_obtain(&desc->irq_lock);
		if (desc->used == IRQ_NOT_ASSIGNED) {
			desc->used = IRQ_ASSIGNED_NOSHARE;
			spinlock_irqrestore_release(&desc->irq_lock);
			break;
		}
		spinlock_irqrestore_release(&desc->irq_lock);
	}
	return (i == NR_IRQS) ? IRQ_INVALID : i;
}

/* need irq_lock protection before use */
static void local_irq_desc_set_vector(uint32_t irq, uint32_t vr)
{
	struct irq_desc *desc;

	desc = &irq_desc_array[irq];
	vector_to_irq[vr] = irq;
	desc->vector = vr;
}

/* lock version of set vector */
static void irq_desc_set_vector(uint32_t irq, uint32_t vr)
{
	struct irq_desc *desc;

	spinlock_rflags;

	desc = &irq_desc_array[irq];
	spinlock_irqsave_obtain(&desc->irq_lock);
	vector_to_irq[vr] = irq;
	desc->vector = vr;
	spinlock_irqrestore_release(&desc->irq_lock);
}

/* used with holding irq_lock outside */
static void _irq_desc_free_vector(uint32_t irq)
{
	struct irq_desc *desc;
	uint32_t vr;
	uint16_t pcpu_id;

	if (irq >= NR_IRQS) {
		return;
	}

	desc = &irq_desc_array[irq];

	vr = desc->vector;
	desc->used = IRQ_NOT_ASSIGNED;
	desc->state = IRQ_DESC_PENDING;
	desc->vector = VECTOR_INVALID;

	vr &= NR_MAX_VECTOR;
	if (vector_to_irq[vr] == irq) {
		vector_to_irq[vr] = IRQ_INVALID;
	}

	for (pcpu_id = 0U; pcpu_id < phys_cpu_num; pcpu_id++) {
		per_cpu(irq_count, pcpu_id)[irq] = 0UL;
	}
}

static void disable_pic_irq(void)
{
	pio_write8(0xffU, 0xA1U);
	pio_write8(0xffU, 0x21U);
}

static bool
irq_desc_append_dev(struct irq_desc *desc, void *node, bool share)
{
	struct dev_handler_node *dev_list;
	bool added = true;

	spinlock_rflags;

	spinlock_irqsave_obtain(&desc->irq_lock);
	dev_list = desc->dev_list;

	/* assign if first node */
	if (dev_list == NULL) {
		desc->dev_list = node;
		desc->used = (share)?IRQ_ASSIGNED_SHARED:IRQ_ASSIGNED_NOSHARE;

		/* Only GSI possible for Level and it already init during
		 * ioapic setup.
		 * caller can later update it with update_irq_handler()
		 */
		if (desc->irq_handler == NULL) {
			desc->irq_handler = common_handler_edge;
		}
	} else if (!share || desc->used == IRQ_ASSIGNED_NOSHARE) {
		/* dev node added failed */
		added = false;
	} else {
		/* dev_list point to last valid node */
		while (dev_list->next != NULL) {
			dev_list = dev_list->next;
		}
		/* add node */
		dev_list->next = node;
	}
	spinlock_irqrestore_release(&desc->irq_lock);

	return added;
}

static struct dev_handler_node*
common_register_handler(uint32_t irq_arg,
		struct irq_request_info *info)
{
	struct dev_handler_node *node = NULL;
	struct irq_desc *desc;
	bool added = false;
	uint32_t irq = irq_arg;

	/* ======================================================
	 * This is low level ISR handler registering function
	 * case: irq = IRQ_INVALID
	 *	caller did not know which irq to use, and want system to
	 *	allocate available irq for it. These irq are in range:
	 *	nr_gsi ~ NR_IRQS
	 *	a irq will be allocated and the vector will be assigned to this
	 *	irq automatically.
	 *
	 * case: irq >=0 and irq < nr_gsi
	 *	caller want to add device ISR handler into ioapic pins.
	 *	two kind of devices: legacy device and PCI device with INTx
	 *	a vector will automatically assigned.
	 *
	 * case: irq with speical type (not from IOAPIC/MSI)
	 *	These irq value are pre-defined for Timer, IPI, Spurious etc
	 *	vectors are pre-defined also
	 *
	 * return value: pinned irq and assigned vector for this irq.
	 *	caller can use this irq to enable/disable/mask/unmask interrupt
	 *	and if this irq is for:
	 *	GSI legacy: nothing to do for legacy irq, already initialized
	 *	GSI other: need to progam PCI INTx to match this irq pin
	 *	MSI: caller need program vector to PCI device
	 *
	 * =====================================================
	 */
	ASSERT(info != NULL, "Invalid param");

	/* HV select a irq for device if irq < 0
	 * this vector/irq match to APCI DSDT or PCI INTx/MSI
	 */
	if (irq == IRQ_INVALID) {
		irq = alloc_irq();
	} else {
		irq = irq_mark_used(irq);
	}

	if (irq >= NR_IRQS) {
		pr_err("failed to assign IRQ");
		goto OUT;
	}

	node = calloc(1U, sizeof(struct dev_handler_node));
	if (node == NULL) {
		pr_err("failed to alloc node");
		irq_desc_try_free_vector(irq);
		goto OUT;
	}

	desc = &irq_desc_array[irq];
	added = irq_desc_append_dev(desc, node, info->share);
	if (!added) {
		free(node);
		node = NULL;
		pr_err("failed to add node to non-shared irq");
	}
OUT:
	if (added) {
		/* it is safe to call irq_desc_alloc_vector multiple times*/
		if (info->vector >= VECTOR_FIXED_START &&
			info->vector <= VECTOR_FIXED_END) {
			irq_desc_set_vector(irq, info->vector);
		} else if (info->vector > NR_MAX_VECTOR) {
			irq_desc_alloc_vector(irq);
		} else {
			pr_err("the input vector is not correct");
			free(node);
			return NULL;
		}

		node->dev_handler = info->func;
		node->dev_data = info->dev_data;
		node->desc = desc;

		/* we are okay using strcpy_s here even with spinlock
		 * since no #PG in HV right now
		 */
		(void)strcpy_s(node->name, 32U, info->name);
		dev_dbg(ACRN_DBG_IRQ, "[%s] %s irq%d vr:0x%x",
			__func__, node->name, irq, desc->vector);
	}

	return node;
}

/* it is safe to call irq_desc_alloc_vector multiple times*/
uint32_t irq_desc_alloc_vector(uint32_t irq)
{
	uint32_t vr = VECTOR_INVALID;
	struct irq_desc *desc;

	spinlock_rflags;

	/* irq should be always available at this time */
	if (irq >= NR_IRQS) {
		return VECTOR_INVALID;
	}

	desc = &irq_desc_array[irq];
	spinlock_irqsave_obtain(&desc->irq_lock);
	if (desc->vector != VECTOR_INVALID) {
		/* already allocated a vector */
		goto OUT;
	}

	/* FLAT mode, a irq connected to every cpu's same vector */
	vr = find_available_vector();
	if (vr > NR_MAX_VECTOR) {
		pr_err("no vector found for irq[%d]", irq);
		goto OUT;
	}
	local_irq_desc_set_vector(irq, vr);
OUT:
	spinlock_irqrestore_release(&desc->irq_lock);
	return vr;
}

void irq_desc_try_free_vector(uint32_t irq)
{
	struct irq_desc *desc;

	spinlock_rflags;

	/* legacy irq's vector is reserved and should not be freed */
	if (irq >= NR_IRQS || irq < NR_LEGACY_IRQ) {
		return;
	}

	desc = &irq_desc_array[irq];
	spinlock_irqsave_obtain(&desc->irq_lock);
	if (desc->dev_list == NULL) {
		_irq_desc_free_vector(irq);
	}

	spinlock_irqrestore_release(&desc->irq_lock);

}

uint32_t irq_to_vector(uint32_t irq)
{
	if (irq < NR_IRQS) {
		return irq_desc_array[irq].vector;
	} else {
		return VECTOR_INVALID;
	}
}

uint32_t dev_to_irq(struct dev_handler_node *node)
{
	return node->desc->irq;
}

uint32_t dev_to_vector(struct dev_handler_node *node)
{
	return node->desc->vector;
}

void init_default_irqs(uint16_t cpu_id)
{
	if (cpu_id != BOOT_CPU_ID) {
		return;
	}

	init_irq_desc();

	/* we use ioapic only, disable legacy PIC */
	disable_pic_irq();
	setup_ioapic_irq();
	init_softirq();
}

void dispatch_exception(struct intr_excp_ctx *ctx)
{
	uint16_t pcpu_id = get_cpu_id();

	/* Obtain lock to ensure exception dump doesn't get corrupted */
	spinlock_obtain(&exception_spinlock);

	/* Dump exception context */
	dump_exception(ctx, pcpu_id);

	/* Release lock to let other CPUs handle exception */
	spinlock_release(&exception_spinlock);

	/* Halt the CPU */
	cpu_dead(pcpu_id);
}

static void handle_spurious_interrupt(uint32_t vector)
{
	send_lapic_eoi();

	get_cpu_var(spurious)++;

	pr_warn("Spurious vector: 0x%x.", vector);

	if (spurious_handler != NULL) {
		spurious_handler(vector);
	}
}

/* do_IRQ() */
void dispatch_interrupt(struct intr_excp_ctx *ctx)
{
	uint32_t vr = ctx->vector;
	uint32_t irq = vector_to_irq[vr];
	struct irq_desc *desc;

	if (irq == IRQ_INVALID) {
		goto ERR;
	}

	desc = &irq_desc_array[irq];
	per_cpu(irq_count, get_cpu_id())[irq]++;

	if (vr != desc->vector) {
		goto ERR;
	}

	if (desc->used == IRQ_NOT_ASSIGNED || desc->irq_handler == NULL) {
		/* mask irq if possible */
		goto ERR;
	}

	desc->irq_handler(desc, desc->handler_data);
	return;
ERR:
	handle_spurious_interrupt(vr);
	return;
}

#ifdef CONFIG_PARTITION_MODE
void partition_mode_dispatch_interrupt(struct intr_excp_ctx *ctx)
{
	uint8_t vr = ctx->vector;
	struct vcpu *vcpu;

	/*
	 * There is no vector and APIC ID remapping for VMs in
	 * ACRN partition mode. Device interrupts are injected with the same
	 * vector into vLAPIC of vCPU running on the pCPU. Vectors used for
	 * HV services are handled by HV using dispatch_interrupt.
	 */
	vcpu = per_cpu(vcpu, get_cpu_id());
	if (vr < VECTOR_FOR_PRI_START) {
		send_lapic_eoi();
		vlapic_intr_edge(vcpu, vr);
	} else {
		dispatch_interrupt(ctx);
	}
}
#endif

int handle_level_interrupt_common(struct irq_desc *desc,
			__unused void *handler_data)
{
	struct dev_handler_node *dev = desc->dev_list;
	spinlock_rflags;

	/*
	 * give other Core a try to return without hold irq_lock
	 * and record irq_lost count here
	 */
	if (desc->state != IRQ_DESC_PENDING) {
		send_lapic_eoi();
		desc->irq_lost_cnt++;
		return 0;
	}

	spinlock_irqsave_obtain(&desc->irq_lock);
	desc->state = IRQ_DESC_IN_PROCESS;

	/* mask iopaic pin */
	if (irq_is_gsi(desc->irq)) {
		GSI_MASK_IRQ(desc->irq);
	}

	/* Send EOI to LAPIC/IOAPIC IRR */
	send_lapic_eoi();

	while (dev != NULL) {
		if (dev->dev_handler != NULL) {
			dev->dev_handler(desc->irq, dev->dev_data);
		}
		dev = dev->next;
	}

	if (irq_is_gsi(desc->irq)) {
		GSI_UNMASK_IRQ(desc->irq);
	}

	desc->state = IRQ_DESC_PENDING;
	spinlock_irqrestore_release(&desc->irq_lock);

	return 0;
}

int common_handler_edge(struct irq_desc *desc, __unused void *handler_data)
{
	struct dev_handler_node *dev = desc->dev_list;
	spinlock_rflags;

	/*
	 * give other Core a try to return without hold irq_lock
	 * and record irq_lost count here
	 */
	if (desc->state != IRQ_DESC_PENDING) {
		send_lapic_eoi();
		desc->irq_lost_cnt++;
		return 0;
	}

	spinlock_irqsave_obtain(&desc->irq_lock);
	desc->state = IRQ_DESC_IN_PROCESS;

	/* Send EOI to LAPIC/IOAPIC IRR */
	send_lapic_eoi();

	while (dev != NULL) {
		if (dev->dev_handler != NULL) {
			dev->dev_handler(desc->irq, dev->dev_data);
		}
		dev = dev->next;
	}

	desc->state = IRQ_DESC_PENDING;
	spinlock_irqrestore_release(&desc->irq_lock);

	return 0;
}

int common_dev_handler_level(struct irq_desc *desc, __unused void *handler_data)
{
	struct dev_handler_node *dev = desc->dev_list;
	spinlock_rflags;

	/*
	 * give other Core a try to return without hold irq_lock
	 * and record irq_lost count here
	 */
	if (desc->state != IRQ_DESC_PENDING) {
		send_lapic_eoi();
		desc->irq_lost_cnt++;
		return 0;
	}

	spinlock_irqsave_obtain(&desc->irq_lock);
	desc->state = IRQ_DESC_IN_PROCESS;

	/* mask iopaic pin */
	if (irq_is_gsi(desc->irq))  {
		GSI_MASK_IRQ(desc->irq);
	}

	/* Send EOI to LAPIC/IOAPIC IRR */
	send_lapic_eoi();

	while (dev != NULL) {
		if (dev->dev_handler != NULL) {
			dev->dev_handler(desc->irq, dev->dev_data);
		}
		dev = dev->next;
	}

	desc->state = IRQ_DESC_PENDING;
	spinlock_irqrestore_release(&desc->irq_lock);

	/* we did not unmask irq until guest EOI the vector */
	return 0;
}

/* no desc->irq_lock for quick handling local interrupt like lapic timer */
int quick_handler_nolock(struct irq_desc *desc, __unused void *handler_data)
{
	struct dev_handler_node *dev = desc->dev_list;

	/* Send EOI to LAPIC/IOAPIC IRR */
	send_lapic_eoi();

	while (dev != NULL) {
		if (dev->dev_handler != NULL) {
			dev->dev_handler(desc->irq, dev->dev_data);
		}
		dev = dev->next;
	}

	return 0;
}

void update_irq_handler(uint32_t irq, irq_handler_t func)
{
	struct irq_desc *desc;

	spinlock_rflags;

	if (irq >= NR_IRQS) {
		return;
	}

	desc = &irq_desc_array[irq];
	spinlock_irqsave_obtain(&desc->irq_lock);
	desc->irq_handler = func;
	spinlock_irqrestore_release(&desc->irq_lock);
}

void unregister_handler_common(struct dev_handler_node *node)
{
	struct dev_handler_node *head;
	struct irq_desc *desc;

	spinlock_rflags;

	if (node == NULL) {
		return;
	}

	dev_dbg(ACRN_DBG_IRQ, "[%s] %s irq%d vr:0x%x",
		__func__, node->name,
		dev_to_irq(node),
		dev_to_vector(node));

	desc = node->desc;
	spinlock_irqsave_obtain(&desc->irq_lock);

	head = desc->dev_list;
	if (head == node) {
		desc->dev_list = NULL;
		goto UNLOCK_EXIT;
	}

	while (head->next != NULL) {
		if (head->next == node) {
			break;
		}
		head = head->next;
	}

	head->next = node->next;

UNLOCK_EXIT:
	spinlock_irqrestore_release(&desc->irq_lock);
	irq_desc_try_free_vector(desc->irq);
	free(node);
}

/*
 * Allocate IRQ with Vector from VECTOR_DYNAMIC_START ~ VECTOR_DYNAMIC_END
 */
struct dev_handler_node*
normal_register_handler(uint32_t irq,
		dev_handler_t func,
		void *dev_data,
		bool share,
		const char *name)
{
	struct irq_request_info info;

	info.vector = VECTOR_INVALID;
	info.func = func;
	info.dev_data = dev_data;
	info.share = share;
	info.name = (char *)name;

	return common_register_handler(irq, &info);
}

/*
 * Allocate IRQ with vector from VECTOR_FIXED_START ~ VECTOR_FIXED_END
 * Allocate a IRQ and install isr on that specific cpu
 * User can install same irq/isr on different CPU by call this function multiple
 * times
 */
struct dev_handler_node*
pri_register_handler(uint32_t irq,
		uint32_t vector,
		dev_handler_t func,
		void *dev_data,
		const char *name)
{
	struct irq_request_info info;

	if (vector < VECTOR_FIXED_START || vector > VECTOR_FIXED_END) {
		return NULL;
	}

	info.vector = vector;
	info.func = func;
	info.dev_data = dev_data;
	info.share = true;
	info.name = (char *)name;

	return common_register_handler(irq, &info);
}

#ifdef HV_DEBUG
void get_cpu_interrupt_info(char *str_arg, int str_max)
{
	char *str = str_arg;
	uint16_t pcpu_id;
	uint32_t irq, vector;
	int len, size = str_max;
	struct irq_desc *desc;

	len = snprintf(str, size, "\r\nIRQ\tVECTOR");
	size -= len;
	str += len;
	for (pcpu_id = 0U; pcpu_id < phys_cpu_num; pcpu_id++) {
		len = snprintf(str, size, "\tCPU%d", pcpu_id);
		size -= len;
		str += len;
	}
	len = snprintf(str, size, "\tLOST\tSHARE");
	size -= len;
	str += len;

	for (irq = 0U; irq < NR_IRQS; irq++) {
		desc = &irq_desc_array[irq];
		vector = irq_to_vector(irq);
		if (desc->used != IRQ_NOT_ASSIGNED &&
			vector != VECTOR_INVALID) {
			len = snprintf(str, size, "\r\n%d\t0x%X", irq, vector);
			size -= len;
			str += len;
			for (pcpu_id = 0U; pcpu_id < phys_cpu_num; pcpu_id++) {
				len = snprintf(str, size, "\t%d",
					per_cpu(irq_count, pcpu_id)[irq]);
				size -= len;
				str += len;
			}
			len = snprintf(str, size, "\t%d\t%s",
					desc->irq_lost_cnt,
					desc->used == IRQ_ASSIGNED_SHARED ?
					"shared" : "no-shared");
			size -= len;
			str += len;
		}
	}
	snprintf(str, size, "\r\n");
}
#endif /* HV_DEBUG */

void interrupt_init(uint16_t pcpu_id)
{
	struct host_idt_descriptor *idtd = &HOST_IDTR;

	set_idt(idtd);
	init_lapic(pcpu_id);
	init_default_irqs(pcpu_id);
#ifndef CONFIG_EFI_STUB
	CPU_IRQ_ENABLE();
#endif
}
