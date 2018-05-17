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
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>

#define ACTIVE_FLAG 0x1 /* any non zero should be okay */

/* SOFTIRQ_DEV_ASSIGN list for all CPUs */
static struct list_head softirq_dev_entry_list;
/* passthrough device link */
static struct list_head ptdev_list;
static spinlock_t ptdev_lock;

/* invalid_entry for error return */
static struct ptdev_remapping_info invalid_entry = {
	.type = PTDEV_INTR_INV,
};

/*
 * entry could both be in ptdev_list and softirq_dev_entry_list.
 * When release entry, we need make sure entry deleted from both
 * lists. We have to require two locks and the lock sequence is:
 *   ptdev_lock
 *     softirq_dev_lock
 */
static spinlock_t softirq_dev_lock;

static inline uint32_t
entry_id_from_msix(uint16_t bdf, int8_t index)
{
	uint32_t id = index;

	id = bdf | (id << 16) | (PTDEV_INTR_MSI << 24);
	return id;
}

static inline uint32_t
entry_id_from_intx(uint8_t pin)
{
	uint32_t id;

	id = pin | (PTDEV_INTR_INTX << 24);
	return id;
}

/* entry_id is used to identify a ptdev entry based on phys info */
static inline uint32_t
entry_id(struct ptdev_remapping_info *entry)
{
	uint32_t id;

	if (entry->type == PTDEV_INTR_INTX)
		id = entry_id_from_intx(entry->intx.phys_pin);
	else
		id = entry_id_from_msix(entry->phys_bdf,
				entry->msi.msix_entry_index);

	return id;
}

static inline bool
is_entry_invalid(struct ptdev_remapping_info *entry)
{
	return entry->type == PTDEV_INTR_INV;
}

static inline bool
is_entry_active(struct ptdev_remapping_info *entry)
{
	return atomic_load((int *)&entry->active) == ACTIVE_FLAG;
}

/* require ptdev_lock protect */
static inline struct ptdev_remapping_info *
_lookup_entry_by_id(uint32_t id)
{
	struct ptdev_remapping_info *entry;
	struct list_head *pos;

	list_for_each(pos, &ptdev_list) {
		entry = list_entry(pos, struct ptdev_remapping_info,
				entry_node);
		if (entry_id(entry) == id)
			return entry;
	}

	return NULL;
}

static inline struct ptdev_remapping_info *
lookp_entry_by_id(uint32_t id)
{
	struct ptdev_remapping_info *entry;

	spinlock_obtain(&ptdev_lock);
	entry = _lookup_entry_by_id(id);
	spinlock_release(&ptdev_lock);
	return entry;
}

/* require ptdev_lock protect */
static inline struct ptdev_remapping_info *
_lookup_entry_by_vmsi(struct vm *vm, uint16_t vbdf, int32_t index)
{
	struct ptdev_remapping_info *entry;
	struct list_head *pos;

	list_for_each(pos, &ptdev_list) {
		entry = list_entry(pos, struct ptdev_remapping_info,
				entry_node);
		if ((entry->type == PTDEV_INTR_MSI)
			&& (entry->vm == vm)
			&& (entry->virt_bdf == vbdf)
			&& (entry->msi.msix_entry_index == index))
			return entry;
	}

	return NULL;
}

static inline struct ptdev_remapping_info *
lookup_entry_by_vmsi(struct vm *vm, uint16_t vbdf, int32_t index)
{
	struct ptdev_remapping_info *entry;

	spinlock_obtain(&ptdev_lock);
	entry = _lookup_entry_by_vmsi(vm, vbdf, index);
	spinlock_release(&ptdev_lock);
	return entry;
}

/* require ptdev_lock protect */
static inline struct ptdev_remapping_info *
_lookup_entry_by_vintx(struct vm *vm, uint8_t vpin,
		enum ptdev_vpin_source vpin_src)
{
	struct ptdev_remapping_info *entry;
	struct list_head *pos;

	list_for_each(pos, &ptdev_list) {
		entry = list_entry(pos, struct ptdev_remapping_info,
				entry_node);
		if ((entry->type == PTDEV_INTR_INTX)
			&& (entry->vm == vm)
			&& (entry->intx.virt_pin == vpin)
			&& (entry->intx.vpin_src == vpin_src))
			return entry;
	}

	return NULL;
}

static inline struct ptdev_remapping_info *
lookup_entry_by_vintx(struct vm *vm, uint8_t vpin,
		enum ptdev_vpin_source vpin_src)
{
	struct ptdev_remapping_info *entry;

	spinlock_obtain(&ptdev_lock);
	entry = _lookup_entry_by_vintx(vm, vpin, vpin_src);
	spinlock_release(&ptdev_lock);
	return entry;
}

static void ptdev_enqueue_softirq(struct ptdev_remapping_info *entry)
{
	spinlock_rflags;
	/* enqueue request in order, SOFTIRQ_DEV_ASSIGN will pickup */
	spinlock_irqsave_obtain(&softirq_dev_lock);

	/* avoid adding recursively */
	list_del(&entry->softirq_node);
	/* TODO: assert if entry already in list */
	list_add_tail(&entry->softirq_node,
			&softirq_dev_entry_list);
	spinlock_irqrestore_release(&softirq_dev_lock);
	raise_softirq(SOFTIRQ_DEV_ASSIGN);
}

static struct ptdev_remapping_info*
ptdev_dequeue_softirq(void)
{
	struct ptdev_remapping_info *entry = NULL;

	spinlock_rflags;
	spinlock_irqsave_obtain(&softirq_dev_lock);

	if (!list_empty(&softirq_dev_entry_list)) {
		entry = get_first_item(&softirq_dev_entry_list,
			struct ptdev_remapping_info, softirq_node);
		list_del_init(&entry->softirq_node);
	}

	spinlock_irqrestore_release(&softirq_dev_lock);
	return entry;
}

/* require ptdev_lock protect */
static struct ptdev_remapping_info *
alloc_entry(struct vm *vm, enum ptdev_intr_type type)
{
	struct ptdev_remapping_info *entry;

	/* allocate */
	entry = calloc(1, sizeof(*entry));
	ASSERT(entry, "alloc memory failed");
	entry->type = type;
	entry->vm = vm;

	INIT_LIST_HEAD(&entry->softirq_node);
	INIT_LIST_HEAD(&entry->entry_node);

	atomic_clear_int(&entry->active, ACTIVE_FLAG);
	list_add(&entry->entry_node, &ptdev_list);

	return entry;
}

/* require ptdev_lock protect */
static void
release_entry(struct ptdev_remapping_info *entry)
{
	spinlock_rflags;

	/* remove entry from ptdev_list */
	list_del_init(&entry->entry_node);

	/*
	 * remove entry from softirq list.the ptdev_lock
	 * is required before calling release_entry.
	 */
	spinlock_irqsave_obtain(&softirq_dev_lock);
	list_del_init(&entry->softirq_node);
	spinlock_irqrestore_release(&softirq_dev_lock);

	free(entry);
}

/* require ptdev_lock protect */
static void
release_all_entries(struct vm *vm)
{
	struct ptdev_remapping_info *entry;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &ptdev_list) {
		entry = list_entry(pos, struct ptdev_remapping_info,
				entry_node);
		if (entry->vm == vm)
			release_entry(entry);
	}
}

/* interrupt context */
static int ptdev_interrupt_handler(__unused int irq, void *data)
{
	struct ptdev_remapping_info *entry =
		(struct ptdev_remapping_info *) data;

	ptdev_enqueue_softirq(entry);
	return 0;
}

static void
ptdev_update_irq_handler(struct vm *vm, struct ptdev_remapping_info *entry)
{
	int phys_irq = dev_to_irq(entry->node);

	if (entry->type == PTDEV_INTR_MSI) {
		/* all other MSI and normal maskable */
		update_irq_handler(phys_irq, common_handler_edge);
	}
	/* update irq handler for IOAPIC */
	if ((entry->type == PTDEV_INTR_INTX)
		&& (entry->intx.vpin_src == PTDEV_VPIN_IOAPIC)) {
		uint64_t rte;
		bool trigger_lvl = false;

		/* VPIN_IOAPIC src means we have vioapic enabled */
		vioapic_get_rte(vm, entry->intx.virt_pin, &rte);
		if ((rte & IOAPIC_RTE_TRGRMOD) == IOAPIC_RTE_TRGRLVL)
			trigger_lvl = true;

		if (trigger_lvl)
			update_irq_handler(phys_irq, common_dev_handler_level);
		else
			update_irq_handler(phys_irq, common_handler_edge);
	}
	/* update irq handler for PIC */
	if ((entry->type == PTDEV_INTR_INTX) && (phys_irq < NR_LEGACY_IRQ)
		&& (entry->intx.vpin_src == PTDEV_VPIN_PIC)) {
		enum vpic_trigger trigger;

		/* VPIN_PIC src means we have vpic enabled */
		vpic_get_irq_trigger(vm, entry->intx.virt_pin, &trigger);
		if (trigger == LEVEL_TRIGGER)
			update_irq_handler(phys_irq, common_dev_handler_level);
		else
			update_irq_handler(phys_irq, common_handler_edge);
	}
}

/* active intr with irq registering */
static struct ptdev_remapping_info *
ptdev_activate_entry(struct ptdev_remapping_info *entry, int phys_irq,
		bool lowpri)
{
	struct dev_handler_node *node;

	/* register and allocate host vector/irq */
	node = normal_register_handler(phys_irq, ptdev_interrupt_handler,
		(void *)entry, true, lowpri, "dev assign");

	ASSERT(node != NULL, "dev register failed");
	entry->node = node;

	atomic_set_int(&entry->active, ACTIVE_FLAG);
	return entry;
}

static void
ptdev_deactivate_entry(struct ptdev_remapping_info *entry)
{
	spinlock_rflags;

	atomic_clear_int(&entry->active, ACTIVE_FLAG);

	unregister_handler_common(entry->node);
	entry->node = NULL;

	/* remove from softirq list if added */
	spinlock_irqsave_obtain(&softirq_dev_lock);
	list_del_init(&entry->softirq_node);
	spinlock_irqrestore_release(&softirq_dev_lock);
}

static bool ptdev_hv_owned_intx(struct vm *vm, struct ptdev_intx_info *info)
{
	/* vm0 pin 4 (uart) is owned by hypervisor under debug version */
	if (is_vm0(vm) && vm->vuart && info->virt_pin == 4)
		return true;
	else
		return false;
}

static void ptdev_build_physical_msi(struct vm *vm, struct ptdev_msi_info *info,
		int vector)
{
	uint64_t vdmask, pdmask;
	uint32_t dest, delmode;
	bool phys;

	/* get physical destination cpu mask */
	dest = (info->vmsi_addr >> 12) & 0xff;
	phys = ((info->vmsi_addr &
			(MSI_ADDR_RH | MSI_ADDR_LOG)) !=
			(MSI_ADDR_RH | MSI_ADDR_LOG));
	calcvdest(vm, &vdmask, dest, phys);
	pdmask = vcpumask2pcpumask(vm, vdmask);

	/* get physical delivery mode */
	delmode = info->vmsi_data & APIC_DELMODE_MASK;
	if (delmode != APIC_DELMODE_FIXED && delmode != APIC_DELMODE_LOWPRIO)
		delmode = APIC_DELMODE_LOWPRIO;

	/* update physical delivery mode & vector */
	info->pmsi_data = info->vmsi_data;
	info->pmsi_data &= ~0x7FF;
	info->pmsi_data |= delmode | vector;

	/* update physical dest mode & dest field */
	info->pmsi_addr = info->vmsi_addr;
	info->pmsi_addr &= ~0xFF00C;
	info->pmsi_addr |= pdmask << 12 |
				MSI_ADDR_RH | MSI_ADDR_LOG;

	dev_dbg(ACRN_DBG_IRQ, "MSI addr:data = 0x%x:%x(V) -> 0x%x:%x(P)",
		info->vmsi_addr, info->vmsi_data,
		info->pmsi_addr, info->pmsi_data);
}

static uint64_t ptdev_build_physical_rte(struct vm *vm,
		struct ptdev_remapping_info *entry)
{
	uint64_t rte;
	int phys_irq = dev_to_irq(entry->node);
	int vector = dev_to_vector(entry->node);

	if (entry->intx.vpin_src == PTDEV_VPIN_IOAPIC) {
		uint64_t vdmask, pdmask;
		uint32_t dest, low, high, delmode;
		bool phys;

		vioapic_get_rte(vm, entry->intx.virt_pin, &rte);
		low = rte;
		high = rte >> 32;

		/* physical destination cpu mask */
		phys = ((low & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
		dest = high >> APIC_ID_SHIFT;
		calcvdest(vm, &vdmask, dest, phys);
		pdmask = vcpumask2pcpumask(vm, vdmask);

		/* physical delivery mode */
		delmode = low & IOAPIC_RTE_DELMOD;
		if ((delmode != IOAPIC_RTE_DELFIXED) &&
			(delmode != IOAPIC_RTE_DELLOPRI))
			delmode = IOAPIC_RTE_DELLOPRI;

		/* update physical delivery mode, dest mode(logical) & vector */
		low &= ~(IOAPIC_RTE_DESTMOD |
			IOAPIC_RTE_DELMOD | IOAPIC_RTE_INTVEC);
		low |= IOAPIC_RTE_DESTLOG | delmode | vector;

		/* update physical dest field */
		high &= ~IOAPIC_RTE_DEST;
		high |= pdmask << 24;

		dev_dbg(ACRN_DBG_IRQ, "IOAPIC RTE = 0x%x:%x(V) -> 0x%x:%x(P)",
				rte >> 32, (uint32_t)rte, high, low);

		rte = high;
		rte = rte << 32 | low;
	} else {
		enum vpic_trigger trigger;
		uint64_t physical_rte;

		/* just update trigger mode */
		ioapic_get_rte(phys_irq, &physical_rte);
		rte = physical_rte;
		rte &= ~IOAPIC_RTE_TRGRMOD;
		vpic_get_irq_trigger(vm, entry->intx.virt_pin, &trigger);
		if (trigger == LEVEL_TRIGGER)
			rte |= IOAPIC_RTE_TRGRLVL;

		dev_dbg(ACRN_DBG_IRQ, "IOAPIC RTE = 0x%x:%x(P) -> 0x%x:%x(P)",
				physical_rte >> 32, (uint32_t)physical_rte,
				rte >> 32, (uint32_t)rte);
	}

	return rte;
}

/* add msix entry for a vm, based on msi id (phys_bdf+msix_index)
 * - if the entry not be added by any vm, allocate it
 * - if the entry already be added by vm0, then change the owner to current vm
 * - if the entry already be added by other vm, return invalid_entry
 */
static struct ptdev_remapping_info *
add_msix_remapping(struct vm *vm, uint16_t virt_bdf, uint16_t phys_bdf,
		int msix_entry_index)
{
	struct ptdev_remapping_info *entry;

	spinlock_obtain(&ptdev_lock);
	entry = _lookup_entry_by_id(
		entry_id_from_msix(phys_bdf, msix_entry_index));
	if (!entry) {
		if (_lookup_entry_by_vmsi(vm, virt_bdf, msix_entry_index)) {
			pr_err("MSIX re-add vbdf%x", virt_bdf);

			spinlock_release(&ptdev_lock);
			return &invalid_entry;
		}
		entry = alloc_entry(vm, PTDEV_INTR_MSI);
		entry->virt_bdf = virt_bdf;
		entry->phys_bdf = phys_bdf;
		entry->msi.msix_entry_index = msix_entry_index;
	} else if ((entry->vm != vm) && is_vm0(entry->vm)) {
		entry->vm = vm;
		entry->virt_bdf = virt_bdf;
	} else if ((entry->vm != vm) && !is_vm0(entry->vm)) {
		pr_err("MSIX pbdf%x idx=%d already in vm%d with vbdf%x, not "
			"able to add into vm%d with vbdf%x", entry->phys_bdf,
			entry->msi.msix_entry_index, entry->vm->attr.id,
			entry->virt_bdf, vm->attr.id, virt_bdf);
		ASSERT(0, "msix entry pbdf%x idx%d already in vm%d",
			phys_bdf, msix_entry_index, entry->vm->attr.id);

		spinlock_release(&ptdev_lock);
		return &invalid_entry;
	}
	spinlock_release(&ptdev_lock);

	dev_dbg(ACRN_DBG_IRQ,
		"VM%d MSIX add vector mapping vbdf%x:pbdf%x idx=%d",
		entry->vm->attr.id, virt_bdf, phys_bdf, msix_entry_index);

	return entry;
}

/* deactive & remove mapping entry of vbdf:msix_entry_index for vm */
static void
remove_msix_remapping(struct vm *vm, uint16_t virt_bdf, int msix_entry_index)
{
	struct ptdev_remapping_info *entry;

	spinlock_obtain(&ptdev_lock);
	entry = _lookup_entry_by_vmsi(vm, virt_bdf, msix_entry_index);
	if (!entry)
		goto END;

	if (is_entry_active(entry))
		/*TODO: disable MSIX device when HV can in future */
		ptdev_deactivate_entry(entry);

	dev_dbg(ACRN_DBG_IRQ,
		"VM%d MSIX remove vector mapping vbdf-pbdf:0x%x-0x%x idx=%d",
		entry->vm->attr.id,
		entry->virt_bdf, entry->phys_bdf, msix_entry_index);

	release_entry(entry);

END:
	spinlock_release(&ptdev_lock);

}

/* add intx entry for a vm, based on intx id (phys_pin)
 * - if the entry not be added by any vm, allocate it
 * - if the entry already be added by vm0, then change the owner to current vm
 * - if the entry already be added by other vm, return invalid_entry
 */
static struct ptdev_remapping_info *
add_intx_remapping(struct vm *vm, uint8_t virt_pin,
		uint8_t phys_pin, bool pic_pin)
{
	struct ptdev_remapping_info *entry;
	enum ptdev_vpin_source vpin_src =
		pic_pin ? PTDEV_VPIN_PIC : PTDEV_VPIN_IOAPIC;

	spinlock_obtain(&ptdev_lock);
	entry = _lookup_entry_by_id(entry_id_from_intx(phys_pin));
	if (!entry) {
		if (_lookup_entry_by_vintx(vm, virt_pin, vpin_src)) {
			pr_err("INTX re-add vpin %d", virt_pin);
			spinlock_release(&ptdev_lock);
			return &invalid_entry;
		}
		entry = alloc_entry(vm, PTDEV_INTR_INTX);
		entry->intx.phys_pin = phys_pin;
		entry->intx.virt_pin = virt_pin;
		entry->intx.vpin_src = vpin_src;
	} else if ((entry->vm != vm) && is_vm0(entry->vm)) {
		entry->vm = vm;
		entry->intx.virt_pin = virt_pin;
		entry->intx.vpin_src = vpin_src;
	} else if ((entry->vm != vm) && !is_vm0(entry->vm)) {
		pr_err("INTX pin%d already in vm%d with vpin%d, not able to "
			"add into vm%d with vpin%d", entry->intx.phys_pin,
			entry->vm->attr.id, entry->intx.virt_pin,
			vm->attr.id, virt_pin);
		ASSERT(0, "intx entry pin%d already vm%d",
			phys_pin, entry->vm->attr.id);

		spinlock_release(&ptdev_lock);
		return &invalid_entry;
	}

	spinlock_release(&ptdev_lock);

	dev_dbg(ACRN_DBG_IRQ,
		"VM%d INTX add pin mapping vpin%d:ppin%d",
		entry->vm->attr.id, virt_pin, phys_pin);

	return entry;
}

/* deactive & remove mapping entry of vpin for vm */
void remove_intx_remapping(struct vm *vm, uint8_t virt_pin, bool pic_pin)
{
	int phys_irq;
	struct ptdev_remapping_info *entry;
	enum ptdev_vpin_source vpin_src =
		pic_pin ? PTDEV_VPIN_PIC : PTDEV_VPIN_IOAPIC;

	spinlock_obtain(&ptdev_lock);
	entry = _lookup_entry_by_vintx(vm, virt_pin, vpin_src);
	if (!entry)
		goto END;

	if (is_entry_active(entry)) {
		phys_irq = dev_to_irq(entry->node);
		if (!irq_is_gsi(phys_irq))
			goto END;

		/* disable interrupt */
		GSI_MASK_IRQ(phys_irq);

		ptdev_deactivate_entry(entry);
		dev_dbg(ACRN_DBG_IRQ,
			"deactive %s intx entry:ppin=%d, pirq=%d ",
			entry->intx.vpin_src == PTDEV_VPIN_PIC ?
			"vPIC" : "vIOAPIC", entry->intx.phys_pin, phys_irq);
		dev_dbg(ACRN_DBG_IRQ, "from vm%d vpin=%d\n",
			entry->vm->attr.id, entry->intx.virt_pin);
	}

	release_entry(entry);

END:
	spinlock_release(&ptdev_lock);
}

static void ptdev_intr_handle_irq(struct vm *vm,
		struct ptdev_remapping_info *entry)
{
	switch (entry->intx.vpin_src) {
	case PTDEV_VPIN_IOAPIC:
	{
		uint64_t rte;
		bool trigger_lvl = false;

		/* VPIN_IOAPIC src means we have vioapic enabled */
		vioapic_get_rte(vm, entry->intx.virt_pin, &rte);
		if ((rte & IOAPIC_RTE_TRGRMOD) == IOAPIC_RTE_TRGRLVL)
			trigger_lvl = true;

		if (trigger_lvl)
			vioapic_assert_irq(vm, entry->intx.virt_pin);
		else
			vioapic_pulse_irq(vm, entry->intx.virt_pin);

		dev_dbg(ACRN_DBG_PTIRQ,
			"dev-assign: irq=0x%x assert vr: 0x%x vRTE=0x%x",
			dev_to_irq(entry->node),
			irq_to_vector(dev_to_irq(entry->node)), rte);
		break;
	}
	case PTDEV_VPIN_PIC:
	{
		enum vpic_trigger trigger;

		/* VPIN_PIC src means we have vpic enabled */
		vpic_get_irq_trigger(vm, entry->intx.virt_pin, &trigger);
		if (trigger == LEVEL_TRIGGER)
			vpic_assert_irq(vm, entry->intx.virt_pin);
		else
			vpic_pulse_irq(vm, entry->intx.virt_pin);
		break;
	}
	default:
		break;
	}
}

void ptdev_softirq(__unused int cpu)
{
	while (1) {
		struct ptdev_remapping_info *entry = ptdev_dequeue_softirq();
		struct vm *vm;

		if (!entry)
			break;

		/* skip any inactive entry */
		if (!is_entry_active(entry)) {
			/* service next item */
			continue;
		}

		/* TBD: need valid vm */
		vm = entry->vm;

		/* handle real request */
		if (entry->type == PTDEV_INTR_INTX)
			ptdev_intr_handle_irq(vm, entry);
		else {
			/* TODO: msi destmode check required */
			vlapic_intr_msi(vm, entry->msi.vmsi_addr,
					entry->msi.vmsi_data);
			dev_dbg(ACRN_DBG_PTIRQ,
				"dev-assign: irq=0x%x MSI VR: 0x%x-0x%x",
				dev_to_irq(entry->node),
				entry->msi.virt_vector,
				irq_to_vector(dev_to_irq(entry->node)));
			dev_dbg(ACRN_DBG_PTIRQ,
				" vmsi_addr: 0x%x vmsi_data: 0x%x",
				entry->msi.vmsi_addr, entry->msi.vmsi_data);
		}
	}
}

void ptdev_intx_ack(struct vm *vm, int virt_pin,
		enum ptdev_vpin_source vpin_src)
{
	int phys_irq;
	struct ptdev_remapping_info *entry;
	int phys_pin;

	entry = lookup_entry_by_vintx(vm, virt_pin, vpin_src);
	if (!entry)
		return;

	phys_pin = entry->intx.phys_pin;
	phys_irq = pin_to_irq(phys_pin);
	if (!irq_is_gsi(phys_irq))
		return;

	/* NOTE: only Level trigger will process EOI/ACK and if we got here
	 * means we have this vioapic or vpic or both enabled
	 */
	switch (entry->intx.vpin_src) {
	case PTDEV_VPIN_IOAPIC:
		vioapic_deassert_irq(vm, virt_pin);
		break;
	case PTDEV_VPIN_PIC:
		vpic_deassert_irq(vm, virt_pin);
	default:
		break;
	}

	dev_dbg(ACRN_DBG_PTIRQ, "dev-assign: irq=0x%x acked vr: 0x%x",
			phys_irq, irq_to_vector(phys_irq));
	GSI_UNMASK_IRQ(phys_irq);
}

/* Main entry for PCI device assignment with MSI and MSI-X
 * MSI can up to 8 vectors and MSI-X can up to 1024 Vectors
 * We use msix_entry_index to indicate coming vectors
 * msix_entry_index = 0 means first vector
 * user must provide bdf and msix_entry_index
 *
 * This function is called by SOS pci MSI config routine through hcall
 */
int ptdev_msix_remap(struct vm *vm, uint16_t virt_bdf,
		struct ptdev_msi_info *info)
{
	struct ptdev_remapping_info *entry = NULL;
	bool lowpri = !is_vm0(vm);

	/*
	 * Device Model should pre-hold the mapping entries by calling
	 * ptdev_add_msix_remapping for UOS.
	 *
	 * For SOS(vm0), it adds the mapping entries at runtime, if the
	 * entry already be held by others, return error.
	 */

	entry = lookup_entry_by_vmsi(vm, virt_bdf, info->msix_entry_index);
	if (!entry) {
		/* VM0 we add mapping dynamically */
		if (is_vm0(vm)) {
			entry = add_msix_remapping(vm, virt_bdf, virt_bdf,
				info->msix_entry_index);
			if (is_entry_invalid(entry)) {
				pr_err("dev-assign: msi entry exist in others");
				return -ENODEV;
			}
		} else {
			/* ptdev_msix_remap is called by SOS on demand, if
			 * failed to find pre-hold mapping, return error to
			 * the caller.
			 */
			pr_err("dev-assign: msi entry not exist");
			return -ENODEV;
		}
	}

	/* handle destroy case */
	if (is_entry_active(entry) && info->vmsi_data == 0) {
		info->pmsi_data = 0;
		ptdev_deactivate_entry(entry);
		goto END;
	}

	if (!is_entry_active(entry)) {
		/* update msi source and active entry */
		ptdev_activate_entry(entry, -1, lowpri);
	}

	/* build physical config MSI, update to info->pmsi_xxx */
	ptdev_build_physical_msi(vm, info, dev_to_vector(entry->node));
	entry->msi = *info;
	entry->msi.virt_vector = info->vmsi_data & 0xFF;
	entry->msi.phys_vector = dev_to_vector(entry->node);

	/* update irq handler according to info in guest */
	ptdev_update_irq_handler(vm, entry);

	dev_dbg(ACRN_DBG_IRQ,
		"PCI %x:%x.%x MSI VR[%d] 0x%x->0x%x assigned to vm%d",
		(entry->virt_bdf >> 8) & 0xFF,
		(entry->virt_bdf >> 3) & 0x1F,
		(entry->virt_bdf) & 0x7, entry->msi.msix_entry_index,
		entry->msi.virt_vector, entry->msi.phys_vector,
		entry->vm->attr.id);
END:
	return 0;
}

static bool vpin_masked(struct vm *vm, uint8_t virt_pin,
	enum ptdev_vpin_source vpin_src)
{
	if (vpin_src == PTDEV_VPIN_IOAPIC) {
		uint64_t rte;

		vioapic_get_rte(vm, virt_pin, &rte);
		if ((rte & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET)
			return true;
		else
			return false;
	} else
		return vpic_is_pin_mask(vm->vpic, virt_pin);
}

static void activate_physical_ioapic(struct vm *vm,
		struct ptdev_remapping_info *entry)
{
	uint64_t rte;
	int phys_irq = dev_to_irq(entry->node);

	/* disable interrupt */
	GSI_MASK_IRQ(phys_irq);

	/* build physical IOAPIC RTE */
	rte = ptdev_build_physical_rte(vm, entry);

	/* set rte entry */
	GSI_SET_RTE(phys_irq, rte | IOAPIC_RTE_INTMSET);

	/* update irq handler according to info in guest */
	ptdev_update_irq_handler(vm, entry);

	/* enable interrupt */
	GSI_UNMASK_IRQ(phys_irq);
}

/* Main entry for PCI/Legacy device assignment with INTx, calling from vIOAPIC
 * or vPIC
 */
int ptdev_intx_pin_remap(struct vm *vm, struct ptdev_intx_info *info)
{
	struct ptdev_remapping_info *entry;
	uint64_t rte;
	int phys_irq;
	int phys_pin;
	bool lowpri = !is_vm0(vm);
	bool need_switch_vpin_src = false;

	/*
	 * virt pin could come from vpic master, vpic slave or vioapic
	 * while phys pin is always means for physical IOAPIC.
	 *
	 * Device Model should pre-hold the mapping entries by calling
	 * ptdev_add_intx_remapping for UOS.
	 *
	 * For SOS(vm0), it adds the mapping entries at runtime, if the
	 * entry already be held by others, return error.
	 */

	/* no remap for hypervisor owned intx */
	if (ptdev_hv_owned_intx(vm, info))
		goto END;

	/* query if we have virt to phys mapping */
	entry = lookup_entry_by_vintx(vm, info->virt_pin, info->vpin_src);
	if (!entry) {
		if (is_vm0(vm)) {
			bool pic_pin = (info->vpin_src == PTDEV_VPIN_PIC);

			/* for vm0, there is chance of vpin source switch
			 * between vPIC & vIOAPIC for one legacy phys_pin.
			 *
			 * here checks if there is already mapping entry from
			 * the other vpin source for legacy pin. If yes, then
			 * switch vpin source is needed
			 */
			if (info->virt_pin < NR_LEGACY_PIN) {
				entry = lookup_entry_by_vintx(vm,
					pic_ioapic_pin_map[info->virt_pin],
					pic_pin ? PTDEV_VPIN_IOAPIC
					: PTDEV_VPIN_PIC);
				if (entry)
					need_switch_vpin_src = true;
			}

			/* entry could be updated by above switch check */
			if (!entry) {
				/* allocate entry during first unmask */
				if (vpin_masked(vm, info->virt_pin,
						info->vpin_src))
					goto END;

				info->phys_pin = info->virt_pin;
				/* fix vPIC pin to correct native IOAPIC pin */
				if (pic_pin)
					info->phys_pin =
					pic_ioapic_pin_map[info->virt_pin];

				entry = add_intx_remapping(vm, info->virt_pin,
						info->phys_pin, pic_pin);
				if (is_entry_invalid(entry)) {
					pr_err("dev-assign: intx entry exist "
						"in others");
					return -ENODEV;
				}
			}
		} else {
			/* ptdev_intx_pin_remap is triggered by vPIC/vIOAPIC
			 * everytime a pin get unmask, here filter out pins
			 * not get mapped.
			 */
			goto END;
		}
	}

	/* no need update if vpin is masked && entry is not active */
	if (!is_entry_active(entry) &&
		vpin_masked(vm, info->virt_pin, info->vpin_src))
		goto END;

	/* phys_pin from physical IOAPIC */
	phys_pin = entry->intx.phys_pin;
	phys_irq = pin_to_irq(phys_pin);
	if (!irq_is_gsi(phys_irq))
		goto END;

	/* if vpin source need switch, make sure the entry is deactived */
	if (need_switch_vpin_src) {
		if (is_entry_active(entry)) {
			GSI_MASK_IRQ(phys_irq);
			ptdev_deactivate_entry(entry);
		}
		dev_dbg(ACRN_DBG_IRQ,
			"IOAPIC pin=%d pirq=%d vpin=%d switch from %s to %s "
			"vpin=%d for vm%d", phys_pin, phys_irq,
			entry->intx.virt_pin,
			entry->intx.vpin_src ? "vPIC" : "vIOAPIC",
			entry->intx.vpin_src ? "vIOPIC" : "vPIC",
			info->virt_pin,
			entry->vm->attr.id);
		entry->intx.vpin_src = info->vpin_src;
		entry->intx.virt_pin = info->virt_pin;
	}

	if (is_entry_active(entry)
		&& (entry->intx.vpin_src == PTDEV_VPIN_IOAPIC)) {
		vioapic_get_rte(vm, entry->intx.virt_pin, &rte);
		if (((uint32_t)rte) == 0x10000) {
			/* disable interrupt */
			GSI_MASK_IRQ(phys_irq);
			ptdev_deactivate_entry(entry);
			dev_dbg(ACRN_DBG_IRQ,
				"IOAPIC pin=%d pirq=%d deassigned ",
				phys_pin, phys_irq);
			dev_dbg(ACRN_DBG_IRQ, "from vm%d vIOAPIC vpin=%d",
				entry->vm->attr.id, entry->intx.virt_pin);
			goto END;
		} else {
			/*update rte*/
			activate_physical_ioapic(vm, entry);
		}
	} else if (is_entry_active(entry)
		&& (entry->intx.vpin_src == PTDEV_VPIN_PIC)) {
		/* only update here
		 * deactive vPIC entry when IOAPIC take it over
		 */
		activate_physical_ioapic(vm, entry);
	} else {
		/* active entry */
		ptdev_activate_entry(entry, phys_irq, lowpri);

		activate_physical_ioapic(vm, entry);

		dev_dbg(ACRN_DBG_IRQ,
			"IOAPIC pin=%d pirq=%d assigned to vm%d %s vpin=%d",
			phys_pin, phys_irq, entry->vm->attr.id,
			entry->intx.vpin_src == PTDEV_VPIN_PIC ?
			"vPIC" : "vIOAPIC", entry->intx.virt_pin);
	}
END:
	return 0;
}

/* except vm0, Device Model should call this function to pre-hold ptdev intx
 * entries:
 * - the entry is identified by phys_pin:
 *   one entry vs. one phys_pin
 * - currently, one phys_pin can only be held by one pin source (vPIC or
 *   vIOAPIC)
 */
int ptdev_add_intx_remapping(struct vm *vm,
			__unused uint16_t virt_bdf, __unused uint16_t phys_bdf,
			uint8_t virt_pin, uint8_t phys_pin, bool pic_pin)
{
	struct ptdev_remapping_info *entry;

	if (vm == NULL) {
		pr_err("ptdev_add_intx_remapping fails!\n");
		return -EINVAL;
	}

	entry = add_intx_remapping(vm, virt_pin, phys_pin, pic_pin);
	if (is_entry_invalid(entry))
		return -ENODEV;

	return 0;
}

void ptdev_remove_intx_remapping(struct vm *vm, uint8_t virt_pin, bool pic_pin)
{
	if (vm == NULL) {
		pr_err("ptdev_remove_intr_remapping fails!\n");
		return;
	}

	remove_intx_remapping(vm, virt_pin, pic_pin);
}

/* except vm0, Device Model should call this function to pre-hold ptdev msi
 * entries:
 * - the entry is identified by phys_bdf:msi_idx:
 *   one entry vs. one phys_bdf:msi_idx
 */
int ptdev_add_msix_remapping(struct vm *vm, uint16_t virt_bdf,
		uint16_t phys_bdf, int vector_count)
{
	struct ptdev_remapping_info *entry;
	int i;

	for (i = 0; i < vector_count; i++) {
		entry = add_msix_remapping(vm, virt_bdf, phys_bdf, i);
		if (is_entry_invalid(entry))
			return -ENODEV;
	}

	return 0;
}

void ptdev_remove_msix_remapping(struct vm *vm, uint16_t virt_bdf,
		int vector_count)
{
	int i;

	if (vm == NULL) {
		pr_err("ptdev_remove_msix_remapping fails!\n");
		return;
	}

	for (i = 0; i < vector_count; i++)
		remove_msix_remapping(vm, virt_bdf, i);
}

void ptdev_init(void)
{
	if (get_cpu_id() > 0)
		return;

	INIT_LIST_HEAD(&ptdev_list);
	spinlock_init(&ptdev_lock);
	INIT_LIST_HEAD(&softirq_dev_entry_list);
	spinlock_init(&softirq_dev_lock);
}

void ptdev_release_all_entries(struct vm *vm)
{
	/* VM already down */
	spinlock_obtain(&ptdev_lock);
	release_all_entries(vm);
	spinlock_release(&ptdev_lock);
}

static void get_entry_info(struct ptdev_remapping_info *entry, char *type,
		int *irq, int *vector, uint64_t *dest, bool *lvl_tm,
		int *pin, int *vpin, int *bdf, int *vbdf)
{
	if (is_entry_active(entry)) {
		if (entry->type == PTDEV_INTR_MSI) {
			strcpy_s(type, 16, "MSI");
			*dest = (entry->msi.pmsi_addr & 0xFF000) >> 12;
			if (entry->msi.pmsi_data & APIC_TRIGMOD_LEVEL)
				*lvl_tm = true;
			else
				*lvl_tm = false;
			*pin = -1;
			*vpin = -1;
			*bdf = entry->phys_bdf;
			*vbdf = entry->virt_bdf;
		} else {
			int phys_irq = pin_to_irq(entry->intx.phys_pin);
			uint64_t rte = 0;

			if (entry->intx.vpin_src == PTDEV_VPIN_IOAPIC)
				strcpy_s(type, 16, "IOAPIC");
			else
				strcpy_s(type, 16, "PIC");
			ioapic_get_rte(phys_irq, &rte);
			*dest = ((rte >> 32) & IOAPIC_RTE_DEST) >> 24;
			if (rte & IOAPIC_RTE_TRGRLVL)
				*lvl_tm = true;
			else
				*lvl_tm = false;
			*pin = entry->intx.phys_pin;
			*vpin = entry->intx.virt_pin;
			*bdf = 0;
			*vbdf = 0;
		}
		*irq = dev_to_irq(entry->node);
		*vector = dev_to_vector(entry->node);
	} else {
		strcpy_s(type, 16, "NONE");
		*irq = -1;
		*vector = 0;
		*dest = 0;
		*lvl_tm = 0;
		*pin = -1;
		*vpin = -1;
		*bdf = 0;
		*vbdf = 0;
	}
}

int get_ptdev_info(char *str, int str_max)
{
	struct ptdev_remapping_info *entry;
	int len, size = str_max, irq, vector;
	char type[16];
	uint64_t dest;
	bool lvl_tm;
	int pin, vpin, bdf, vbdf;
	struct list_head *pos;

	len = snprintf(str, size,
		"\r\nVM\tTYPE\tIRQ\tVEC\tDEST\tTM\tPIN\tVPIN\tBDF\tVBDF");
	size -= len;
	str += len;

	spinlock_obtain(&ptdev_lock);
	list_for_each(pos, &ptdev_list) {
		entry = list_entry(pos, struct ptdev_remapping_info,
				entry_node);
		if (is_entry_active(entry)) {
			get_entry_info(entry, type, &irq, &vector,
					&dest, &lvl_tm, &pin, &vpin,
					&bdf, &vbdf);
			len = snprintf(str, size,
					"\r\n%d\t%s\t%d\t0x%X\t0x%X",
					entry->vm->attr.id, type,
					irq, vector, dest);
			size -= len;
			str += len;

			len = snprintf(str, size,
					"\t%s\t%d\t%d\t%x:%x.%x\t%x:%x.%x",
					is_entry_active(entry) ?
					(lvl_tm ? "level" : "edge") : "none",
					pin, vpin,
					(bdf & 0xff00) >> 8,
					(bdf & 0xf8) >> 3, bdf & 0x7,
					(vbdf & 0xff00) >> 8,
					(vbdf & 0xf8) >> 3, vbdf & 0x7);
			size -= len;
			str += len;
		}
	}
	spinlock_release(&ptdev_lock);

	snprintf(str, size, "\r\n");
	return 0;
}
