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

#include <hypervisor.h>

static spinlock_t exception_spinlock = { .head = 0, .tail = 0, };

struct irq_request_info {
	/* vector set to 0xE0 ~ 0xFF for pri_register_handler
	 * and set to -1 for normal_register_handler
	 */
	int vector;
	dev_handler_t func;
	void *dev_data;
	bool share;
	bool lowpri;
	char *name;
};

/* any field change in below required irq_lock protection with irqsave */
struct irq_desc {
	int irq;		/* index to irq_desc_base */
	enum irq_state used;	/* this irq have assigned to device */
	enum irq_desc_state state; /* irq_desc status */
	int vector;		/* assigned vector */
	void *handler_data;	/* irq_handler private data */
	int (*irq_handler)(struct irq_desc *irq_desc, void *handler_data);
	struct dev_handler_node *dev_list;
	spinlock_t irq_lock;
	uint64_t *irq_cnt; /* this irq cnt happened on CPUs */
	uint64_t irq_lost_cnt;
};

static struct irq_desc *irq_desc_base;
static int vector_to_irq[NR_MAX_VECTOR + 1];

static DEFINE_CPU_DATA(uint64_t[NR_MAX_IRQS], irq_count);
static DEFINE_CPU_DATA(uint64_t, spurious);

spurious_handler_t spurious_handler;

static void init_irq_desc(void)
{
	int i, page_num = 0;
	int desc_size = NR_MAX_IRQS * sizeof(struct irq_desc);

	page_num = (desc_size + CPU_PAGE_SIZE-1) >> CPU_PAGE_SHIFT;

	irq_desc_base = alloc_pages(page_num);

	ASSERT(irq_desc_base, "page alloc failed!");
	memset(irq_desc_base, 0, page_num * CPU_PAGE_SIZE);

	for (i = 0; i < NR_MAX_IRQS; i++) {
		irq_desc_base[i].irq = i;
		irq_desc_base[i].vector = VECTOR_INVALID;
		spinlock_init(&irq_desc_base[i].irq_lock);
	}

	for (i = 0; i <= NR_MAX_VECTOR; i++)
		vector_to_irq[i] = IRQ_INVALID;

}

/*
 * alloc vector 0x20-0xDF for irq
 *	lowpri:  0x20-0x7F
 *	highpri: 0x80-0xDF
 */
static int find_available_vector(bool lowpri)
{
	int i, start, end;

	if (lowpri) {
		start = VECTOR_FOR_NOR_LOWPRI_START;
		end = VECTOR_FOR_NOR_LOWPRI_END;
	} else {
		start = VECTOR_FOR_NOR_HIGHPRI_START;
		end = VECTOR_FOR_NOR_HIGHPRI_END;
	}

	/* TODO: vector lock required */
	for (i = start; i < end; i++) {
		if (vector_to_irq[i] == IRQ_INVALID)
			return i;
	}
	return -1;
}

/*
 * check and set irq to be assigned
 * return: -1 if irq already assigned otherwise return irq
 */
int irq_mark_used(int irq)
{
	struct irq_desc *desc;

	spinlock_rflags;

	if (irq < 0)
		return -1;

	desc = irq_desc_base + irq;
	spinlock_irqsave_obtain(&desc->irq_lock);
	if (desc->used == IRQ_NOT_ASSIGNED)
		desc->used = IRQ_ASSIGNED_NOSHARE;
	spinlock_irqrestore_release(&desc->irq_lock);
	return irq;
}

/*
 * system find available irq and set assigned
 * return: irq, -1 not found
 */
static int alloc_irq(void)
{
	int i;
	struct irq_desc *desc;

	spinlock_rflags;

	for (i = irq_gsi_num(); i < NR_MAX_IRQS; i++) {
		desc = irq_desc_base + i;
		spinlock_irqsave_obtain(&desc->irq_lock);
		if (desc->used == IRQ_NOT_ASSIGNED) {
			desc->used = IRQ_ASSIGNED_NOSHARE;
			spinlock_irqrestore_release(&desc->irq_lock);
			break;
		}
		spinlock_irqrestore_release(&desc->irq_lock);
	}
	return (i == NR_MAX_IRQS) ? -1:i;
}

/* need irq_lock protection before use */
static void _irq_desc_set_vector(int irq, int vr)
{
	struct irq_desc *desc;

	desc = irq_desc_base + irq;
	vector_to_irq[vr] = irq;
	desc->vector = vr;
}

/* lock version of set vector */
static void irq_desc_set_vector(int irq, int vr)
{
	struct irq_desc *desc;

	spinlock_rflags;

	desc = irq_desc_base + irq;
	spinlock_irqsave_obtain(&desc->irq_lock);
	vector_to_irq[vr] = irq;
	desc->vector = vr;
	spinlock_irqrestore_release(&desc->irq_lock);
}

/* used with holding irq_lock outside */
static void _irq_desc_free_vector(int irq)
{
	struct irq_desc *desc;
	int vr;

	if (irq > NR_MAX_IRQS || irq < 0)
		return;

	desc = irq_desc_base + irq;

	vr = desc->vector;
	desc->used = IRQ_NOT_ASSIGNED;
	desc->state = IRQ_DESC_PENDING;
	desc->vector = VECTOR_INVALID;

	vr &= NR_MAX_VECTOR;
	if (vector_to_irq[vr] == irq)
		vector_to_irq[vr] = IRQ_INVALID;
}

static void disable_pic_irq(void)
{
	io_write_byte(0xff, 0xA1);
	io_write_byte(0xff, 0x21);
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
		if (!desc->irq_handler)
			desc->irq_handler = common_handler_edge;
	} else if (!share || desc->used == IRQ_ASSIGNED_NOSHARE) {
		/* dev node added failed */
		added = false;
	} else {
		/* dev_list point to last valid node */
		while (dev_list->next)
			dev_list = dev_list->next;
		/* add node */
		dev_list->next = node;
	}
	spinlock_irqrestore_release(&desc->irq_lock);

	return added;
}

static struct dev_handler_node*
common_register_handler(int irq,
		struct irq_request_info *info)
{
	struct dev_handler_node *node = NULL;
	struct irq_desc *desc;
	bool added = false;

	/* ======================================================
	 * This is low level ISR handler registering function
	 * case: irq = -1
	 *	caller did not know which irq to use, and want system to
	 *	allocate available irq for it. These irq are in range:
	 *	nr_gsi ~ NR_MAX_IRQS
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
	if (irq < 0)
		irq = alloc_irq();
	else
		irq = irq_mark_used(irq);

	if (irq < 0) {
		pr_err("failed to assign IRQ");
		goto OUT;
	}

	node = calloc(1, sizeof(struct dev_handler_node));
	if (node == NULL) {
		pr_err("failed to alloc node");
		irq_desc_try_free_vector(irq);
		goto OUT;
	}

	desc = irq_desc_base + irq;
	added = irq_desc_append_dev(desc, node, info->share);
	if (!added) {
		free(node);
		node = NULL;
		pr_err("failed to add node to non-shared irq");
	}
OUT:
	if (added) {
		/* it is safe to call irq_desc_alloc_vector multiple times*/
		if (info->vector >= VECTOR_FOR_PRI_START &&
			info->vector <= VECTOR_FOR_PRI_END)
			irq_desc_set_vector(irq, info->vector);
		else if (info->vector < 0)
			irq_desc_alloc_vector(irq, info->lowpri);
		else {
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
		strcpy_s(node->name, 32, info->name);
		dev_dbg(ACRN_DBG_IRQ, "[%s] %s irq%d vr:0x%x",
			__func__, node->name, irq, desc->vector);
	}

	return node;
}

/* it is safe to call irq_desc_alloc_vector multiple times*/
int irq_desc_alloc_vector(int irq, bool lowpri)
{
	int vr = -1;
	struct irq_desc *desc;

	spinlock_rflags;

	/* irq should be always available at this time */
	if (irq > NR_MAX_IRQS || irq < 0)
		return false;

	desc = irq_desc_base + irq;
	spinlock_irqsave_obtain(&desc->irq_lock);
	if (desc->vector != VECTOR_INVALID) {
		/* already allocated a vector */
		goto OUT;
	}

	/* FLAT mode, a irq connected to every cpu's same vector */
	vr = find_available_vector(lowpri);
	if (vr < 0) {
		pr_err("no vector found for irq[%d]", irq);
		goto OUT;
	}
	_irq_desc_set_vector(irq, vr);
OUT:
	spinlock_irqrestore_release(&desc->irq_lock);
	return vr;
}

void irq_desc_try_free_vector(int irq)
{
	struct irq_desc *desc;

	spinlock_rflags;

	/* legacy irq's vector is reserved and should not be freed */
	if (irq > NR_MAX_IRQS || irq < NR_LEGACY_IRQ)
		return;

	desc = irq_desc_base + irq;
	spinlock_irqsave_obtain(&desc->irq_lock);
	if (desc->dev_list == NULL)
		_irq_desc_free_vector(irq);

	spinlock_irqrestore_release(&desc->irq_lock);

}

int irq_to_vector(int irq)
{
	if (irq < NR_MAX_IRQS)
		return irq_desc_base[irq].vector;
	else
		return VECTOR_INVALID;
}

int dev_to_irq(struct dev_handler_node *node)
{
	return node->desc->irq;
}

int dev_to_vector(struct dev_handler_node *node)
{
	return node->desc->vector;
}

int init_default_irqs(unsigned int cpu_id)
{
	if (cpu_id > 0)
		return 0;

	init_irq_desc();

	/* we use ioapic only, disable legacy PIC */
	disable_pic_irq();
	setup_ioapic_irq();
	init_softirq();

	return 0;
}

void dispatch_exception(struct intr_excp_ctx *ctx)
{
	unsigned int cpu_id = get_cpu_id();

	/* Obtain lock to ensure exception dump doesn't get corrupted */
	spinlock_obtain(&exception_spinlock);

	/* Dump exception context */
	dump_exception(ctx, cpu_id);

	/* Release lock to let other CPUs handle exception */
	spinlock_release(&exception_spinlock);

	/* Halt the CPU */
	cpu_dead(cpu_id);
}

void handle_spurious_interrupt(int vector)
{
	send_lapic_eoi();

	get_cpu_var(spurious)++;

	pr_warn("Spurious vector: 0x%x.", vector);

	if (spurious_handler)
		spurious_handler(vector);
}

/* do_IRQ() */
void dispatch_interrupt(struct intr_excp_ctx *ctx)
{
	int vr = ctx->vector;
	int irq = vector_to_irq[vr];
	struct irq_desc *desc;

	if (irq == IRQ_INVALID)
		goto ERR;

	desc = irq_desc_base + irq;
	per_cpu(irq_count, get_cpu_id())[irq]++;

	if (vr != desc->vector)
		goto ERR;

	if (desc->used == IRQ_NOT_ASSIGNED || !desc->irq_handler) {
		/* mask irq if possible */
		goto ERR;
	}

	desc->irq_handler(desc, desc->handler_data);
	return;
ERR:
	handle_spurious_interrupt(vr);
	return;
}

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
	if (irq_is_gsi(desc->irq))
		GSI_MASK_IRQ(desc->irq);

	/* Send EOI to LAPIC/IOAPIC IRR */
	send_lapic_eoi();

	while (dev) {
		if (dev->dev_handler)
			dev->dev_handler(desc->irq, dev->dev_data);
		dev = dev->next;
	}

	if (irq_is_gsi(desc->irq))
		GSI_UNMASK_IRQ(desc->irq);

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

	while (dev) {
		if (dev->dev_handler)
			dev->dev_handler(desc->irq, dev->dev_data);
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
	if (irq_is_gsi(desc->irq))
		GSI_MASK_IRQ(desc->irq);

	/* Send EOI to LAPIC/IOAPIC IRR */
	send_lapic_eoi();

	while (dev) {
		if (dev->dev_handler)
			dev->dev_handler(desc->irq, dev->dev_data);
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

	while (dev) {
		if (dev->dev_handler)
			dev->dev_handler(desc->irq, dev->dev_data);
		dev = dev->next;
	}

	return 0;
}

void update_irq_handler(int irq, irq_handler_t func)
{
	struct irq_desc *desc;

	spinlock_rflags;

	if (irq >= NR_MAX_IRQS)
		return;

	desc = irq_desc_base + irq;
	spinlock_irqsave_obtain(&desc->irq_lock);
	desc->irq_handler = func;
	spinlock_irqrestore_release(&desc->irq_lock);
}

void unregister_handler_common(struct dev_handler_node *node)
{
	struct dev_handler_node *head;
	struct irq_desc *desc;

	spinlock_rflags;

	if (node == NULL)
		return;

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

	while (head->next) {
		if (head->next == node)
			break;
		head = head->next;
	}

	head->next = node->next;

UNLOCK_EXIT:
	spinlock_irqrestore_release(&desc->irq_lock);
	irq_desc_try_free_vector(desc->irq);
	free(node);
}

/*
 * Allocate IRQ with Vector from 0x20 ~ 0xDF
 */
struct dev_handler_node*
normal_register_handler(int irq,
		dev_handler_t func,
		void *dev_data,
		bool share,
		bool lowpri,
		const char *name)
{
	struct irq_request_info info;

	info.vector = -1;
	info.lowpri = lowpri;
	info.func = func;
	info.dev_data = dev_data;
	info.share = share;
	info.name = (char *)name;

	return common_register_handler(irq, &info);
}

/*
 * Allocate IRQ with vector from 0xE0 ~ 0xFF
 * Allocate a IRQ and install isr on that specific cpu
 * User can install same irq/isr on different CPU by call this function multiple
 * times
 */
struct dev_handler_node*
pri_register_handler(int irq,
		int vector,
		dev_handler_t func,
		void *dev_data,
		const char *name)
{
	struct irq_request_info info;

	if (vector < VECTOR_FOR_PRI_START || vector > VECTOR_FOR_PRI_END)
		return NULL;

	info.vector = vector;
	info.lowpri = false;
	info.func = func;
	info.dev_data = dev_data;
	info.share = true;
	info.name = (char *)name;

	return common_register_handler(irq, &info);
}

int get_cpu_interrupt_info(char *str, int str_max)
{
	int irq, vector, pcpu_id, len, size = str_max;
	struct irq_desc *desc;

	len = snprintf(str, size, "\r\nIRQ\tVECTOR");
	size -= len;
	str += len;
	for (pcpu_id = 0; pcpu_id < phy_cpu_num; pcpu_id++) {
		len = snprintf(str, size, "\tCPU%d", pcpu_id);
		size -= len;
		str += len;
	}
	len = snprintf(str, size, "\tLOST\tSHARE");
	size -= len;
	str += len;

	for (irq = 0; irq < NR_MAX_IRQS; irq++) {
		desc = irq_desc_base + irq;
		vector = irq_to_vector(irq);
		if (desc->used != IRQ_NOT_ASSIGNED &&
			vector != VECTOR_INVALID) {
			len = snprintf(str, size, "\r\n%d\t0x%X", irq, vector);
			size -= len;
			str += len;
			for (pcpu_id = 0; pcpu_id < phy_cpu_num; pcpu_id++) {
				len = snprintf(str, size, "\t%d",
					per_cpu(irq_count, pcpu_id)[irq]++);
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
	return 0;
}
