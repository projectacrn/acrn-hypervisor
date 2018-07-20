/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static inline uint32_t
entry_id_from_msix(uint16_t bdf, uint32_t index)
{
	uint32_t id = index & 0xffU;

	id = (uint32_t)bdf | (id << 16U) | ((uint32_t)PTDEV_INTR_MSI << 24U);
	return id;
}

static inline uint32_t
entry_id_from_intx(uint8_t pin)
{
	uint32_t id;

	id = pin | ((uint32_t)PTDEV_INTR_INTX << 24U);
	return id;
}

/* entry_id is used to identify a ptdev entry based on phys info */
static inline uint32_t
entry_id(struct ptdev_remapping_info *entry)
{
	uint32_t id;
	struct ptdev_msi_info *msi = &entry->ptdev_intr_info.msi;
	struct ptdev_intx_info *intx = &entry->ptdev_intr_info.intx;

	if (entry->type == PTDEV_INTR_INTX) {
		id = entry_id_from_intx(intx->phys_pin);
	} else {
		id = entry_id_from_msix(entry->phys_bdf,
				msi->msix_entry_index);
	}

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
	return atomic_load32(&entry->active) == ACTIVE_FLAG;
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
		if (entry_id(entry) == id) {
			return entry;
		}
	}

	return NULL;
}

/* require ptdev_lock protect */
static inline struct ptdev_remapping_info *
_lookup_entry_by_vmsi(struct vm *vm, uint16_t vbdf, uint32_t index)
{
	struct ptdev_remapping_info *entry;
	struct list_head *pos;

	list_for_each(pos, &ptdev_list) {
		entry = list_entry(pos, struct ptdev_remapping_info,
				entry_node);
		if ((entry->type == PTDEV_INTR_MSI)
			&& (entry->vm == vm)
			&& (entry->virt_bdf == vbdf)
			&& (entry->ptdev_intr_info.msi.msix_entry_index
				== index)) {
			return entry;
		}
	}

	return NULL;
}

static inline struct ptdev_remapping_info *
lookup_entry_by_vmsi(struct vm *vm, uint16_t vbdf, uint32_t index)
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
			&& (entry->ptdev_intr_info.intx.virt_pin == vpin)
			&& (entry->ptdev_intr_info.intx.vpin_src == vpin_src)) {
			return entry;
		}
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

static void
ptdev_update_irq_handler(struct vm *vm, struct ptdev_remapping_info *entry)
{
	uint32_t phys_irq = dev_to_irq(entry->node);
	struct ptdev_intx_info *intx = &entry->ptdev_intr_info.intx;

	if (entry->type == PTDEV_INTR_MSI) {
		/* all other MSI and normal maskable */
		update_irq_handler(phys_irq, common_handler_edge);
	}
	/* update irq handler for IOAPIC */
	if ((entry->type == PTDEV_INTR_INTX)
		&& (intx->vpin_src
			== PTDEV_VPIN_IOAPIC)) {
		union ioapic_rte rte;
		bool trigger_lvl = false;

		/* VPIN_IOAPIC src means we have vioapic enabled */
		vioapic_get_rte(vm, intx->virt_pin, &rte);
		if ((rte.full & IOAPIC_RTE_TRGRMOD) == IOAPIC_RTE_TRGRLVL) {
			trigger_lvl = true;
		}

		if (trigger_lvl) {
			update_irq_handler(phys_irq, common_dev_handler_level);
		} else {
			update_irq_handler(phys_irq, common_handler_edge);
		}
	}
	/* update irq handler for PIC */
	if ((entry->type == PTDEV_INTR_INTX) && (phys_irq < NR_LEGACY_IRQ)
		&& (intx->vpin_src == PTDEV_VPIN_PIC)) {
		enum vpic_trigger trigger;

		/* VPIN_PIC src means we have vpic enabled */
		vpic_get_irq_trigger(vm,
		        intx->virt_pin, &trigger);
		if (trigger == LEVEL_TRIGGER) {
			update_irq_handler(phys_irq, common_dev_handler_level);
		} else {
			update_irq_handler(phys_irq, common_handler_edge);
		}
	}
}

static bool ptdev_hv_owned_intx(struct vm *vm, struct ptdev_intx_info *info)
{
	/* vm0 pin 4 (uart) is owned by hypervisor under debug version */
	if (is_vm0(vm) && (vm->vuart != NULL) && info->virt_pin == 4U) {
		return true;
	} else {
		return false;
	}
}

static void ptdev_build_physical_msi(struct vm *vm, struct ptdev_msi_info *info,
		uint32_t vector)
{
	uint64_t vdmask, pdmask;
	uint32_t dest, delmode;
	bool phys;

	/* get physical destination cpu mask */
	dest = (info->vmsi_addr >> 12) & 0xffU;
	phys = ((info->vmsi_addr & MSI_ADDR_LOG) != MSI_ADDR_LOG);

	calcvdest(vm, &vdmask, dest, phys);
	pdmask = vcpumask2pcpumask(vm, vdmask);

	/* get physical delivery mode */
	delmode = info->vmsi_data & APIC_DELMODE_MASK;
	if (delmode != APIC_DELMODE_FIXED && delmode != APIC_DELMODE_LOWPRIO) {
		delmode = APIC_DELMODE_LOWPRIO;
	}

	/* update physical delivery mode & vector */
	info->pmsi_data = info->vmsi_data;
	info->pmsi_data &= ~0x7FFU;
	info->pmsi_data |= delmode | vector;

	/* update physical dest mode & dest field */
	info->pmsi_addr = info->vmsi_addr;
	info->pmsi_addr &= ~0xFF00CU;
	info->pmsi_addr |= (uint32_t)(pdmask << 12U) |
				MSI_ADDR_RH | MSI_ADDR_LOG;

	dev_dbg(ACRN_DBG_IRQ, "MSI addr:data = 0x%x:%x(V) -> 0x%x:%x(P)",
		info->vmsi_addr, info->vmsi_data,
		info->pmsi_addr, info->pmsi_data);
}

static union ioapic_rte
ptdev_build_physical_rte(struct vm *vm,
		struct ptdev_remapping_info *entry)
{
	union ioapic_rte rte;
	uint32_t phys_irq = dev_to_irq(entry->node);
	uint32_t vector = dev_to_vector(entry->node);
	struct ptdev_intx_info *intx = &entry->ptdev_intr_info.intx;

	if (intx->vpin_src == PTDEV_VPIN_IOAPIC) {
		uint64_t vdmask, pdmask, delmode;
		uint32_t dest;
		union ioapic_rte virt_rte;
		bool phys;

		vioapic_get_rte(vm, intx->virt_pin,
			&virt_rte);
		rte = virt_rte;

		/* physical destination cpu mask */
		phys = ((virt_rte.full & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
		dest = (uint32_t)(virt_rte.full >> IOAPIC_RTE_DEST_SHIFT);
		calcvdest(vm, &vdmask, dest, phys);
		pdmask = vcpumask2pcpumask(vm, vdmask);

		/* physical delivery mode */
		delmode = virt_rte.full & IOAPIC_RTE_DELMOD;
		if ((delmode != IOAPIC_RTE_DELFIXED) &&
			(delmode != IOAPIC_RTE_DELLOPRI)) {
			delmode = IOAPIC_RTE_DELLOPRI;
		}

		/* update physical delivery mode, dest mode(logical) & vector */
		rte.full &= ~(IOAPIC_RTE_DESTMOD |
			IOAPIC_RTE_DELMOD | IOAPIC_RTE_INTVEC);
		rte.full |= IOAPIC_RTE_DESTLOG | delmode | (uint64_t)vector;

		/* update physical dest field */
		rte.full &= ~IOAPIC_RTE_DEST_MASK;
		rte.full |= pdmask << IOAPIC_RTE_DEST_SHIFT;

		dev_dbg(ACRN_DBG_IRQ, "IOAPIC RTE = 0x%x:%x(V) -> 0x%x:%x(P)",
			virt_rte.u.hi_32, virt_rte.u.lo_32,
			rte.u.hi_32, rte.u.lo_32);
	} else {
		enum vpic_trigger trigger;
		union ioapic_rte phys_rte;

		/* just update trigger mode */
		ioapic_get_rte(phys_irq, &phys_rte);
		rte.full = phys_rte.full & (~IOAPIC_RTE_TRGRMOD);
		vpic_get_irq_trigger(vm,
		        intx->virt_pin, &trigger);
		if (trigger == LEVEL_TRIGGER) {
			rte.full |= IOAPIC_RTE_TRGRLVL;
		}

		dev_dbg(ACRN_DBG_IRQ, "IOAPIC RTE = 0x%x:%x(P) -> 0x%x:%x(P)",
			phys_rte.u.hi_32, phys_rte.u.lo_32,
			rte.u.hi_32, rte.u.lo_32);
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
		uint32_t msix_entry_index)
{
	struct ptdev_remapping_info *entry;

	spinlock_obtain(&ptdev_lock);
	entry = _lookup_entry_by_id(
		entry_id_from_msix(phys_bdf, msix_entry_index));
	if (entry == NULL) {
		if (_lookup_entry_by_vmsi(vm, virt_bdf, msix_entry_index) != NULL) {
			pr_err("MSIX re-add vbdf%x", virt_bdf);

			spinlock_release(&ptdev_lock);
			return &invalid_entry;
		}
		entry = alloc_entry(vm, PTDEV_INTR_MSI);
		entry->virt_bdf = virt_bdf;
		entry->phys_bdf = phys_bdf;
		entry->ptdev_intr_info.msi.msix_entry_index = msix_entry_index;
	} else if (entry->vm != vm) {
		if (is_vm0(entry->vm)) {
			entry->vm = vm;
			entry->virt_bdf = virt_bdf;
		} else {
			pr_err("MSIX pbdf%x idx=%d already in vm%d with vbdf%x,"
				" not able to add into vm%d with vbdf%x",
				entry->phys_bdf,
				entry->ptdev_intr_info.msi.msix_entry_index,
				entry->vm->attr.id,
				entry->virt_bdf, vm->attr.id, virt_bdf);
			ASSERT(false, "msix entry pbdf%x idx%d already in vm%d",
			       phys_bdf, msix_entry_index, entry->vm->attr.id);

			spinlock_release(&ptdev_lock);
			return &invalid_entry;
		}
	} else {
		/* The mapping has already been added to the VM. No action
		 * required. */
	}
	spinlock_release(&ptdev_lock);

	dev_dbg(ACRN_DBG_IRQ,
		"VM%d MSIX add vector mapping vbdf%x:pbdf%x idx=%d",
		entry->vm->attr.id, virt_bdf, phys_bdf, msix_entry_index);

	return entry;
}

/* deactive & remove mapping entry of vbdf:msix_entry_index for vm */
static void
remove_msix_remapping(struct vm *vm, uint16_t virt_bdf, uint32_t msix_entry_index)
{
	struct ptdev_remapping_info *entry;

	spinlock_obtain(&ptdev_lock);
	entry = _lookup_entry_by_vmsi(vm, virt_bdf, msix_entry_index);
	if (entry == NULL) {
		goto END;
	}

	if (is_entry_active(entry)) {
		/*TODO: disable MSIX device when HV can in future */
		ptdev_deactivate_entry(entry);
	}

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
	if (entry == NULL) {
		if (_lookup_entry_by_vintx(vm, virt_pin, vpin_src) != NULL) {
			pr_err("INTX re-add vpin %d", virt_pin);
			spinlock_release(&ptdev_lock);
			return &invalid_entry;
		}
		entry = alloc_entry(vm, PTDEV_INTR_INTX);
		entry->ptdev_intr_info.intx.phys_pin = phys_pin;
		entry->ptdev_intr_info.intx.virt_pin = virt_pin;
		entry->ptdev_intr_info.intx.vpin_src = vpin_src;
	} else if (entry->vm != vm) {
		if (is_vm0(entry->vm)) {
			entry->vm = vm;
			entry->ptdev_intr_info.intx.virt_pin = virt_pin;
			entry->ptdev_intr_info.intx.vpin_src = vpin_src;
		} else {
			pr_err("INTX pin%d already in vm%d with vpin%d,"
			       " not able to add into vm%d with vpin%d",
			       entry->ptdev_intr_info.intx.phys_pin,
			       entry->vm->attr.id,
			       entry->ptdev_intr_info.intx.virt_pin,
			       vm->attr.id, virt_pin);
			ASSERT(false, "intx entry pin%d already vm%d",
			       phys_pin, entry->vm->attr.id);

			spinlock_release(&ptdev_lock);
			return &invalid_entry;
		}
	} else {
		/* The mapping has already been added to the VM. No action
		 * required. */
	}

	spinlock_release(&ptdev_lock);

	dev_dbg(ACRN_DBG_IRQ,
		"VM%d INTX add pin mapping vpin%d:ppin%d",
		entry->vm->attr.id, virt_pin, phys_pin);

	return entry;
}

/* deactive & remove mapping entry of vpin for vm */
static void remove_intx_remapping(struct vm *vm, uint8_t virt_pin, bool pic_pin)
{
	uint32_t phys_irq;
	struct ptdev_remapping_info *entry;
	enum ptdev_vpin_source vpin_src =
		pic_pin ? PTDEV_VPIN_PIC : PTDEV_VPIN_IOAPIC;

	spinlock_obtain(&ptdev_lock);
	entry = _lookup_entry_by_vintx(vm, virt_pin, vpin_src);
	if (entry == NULL) {
		goto END;
	}

	if (is_entry_active(entry)) {
		phys_irq = dev_to_irq(entry->node);
		if (!irq_is_gsi(phys_irq)) {
			goto END;
		}

		/* disable interrupt */
		GSI_MASK_IRQ(phys_irq);

		ptdev_deactivate_entry(entry);
		dev_dbg(ACRN_DBG_IRQ,
			"deactive %s intx entry:ppin=%d, pirq=%d ",
			entry->ptdev_intr_info.intx.vpin_src == PTDEV_VPIN_PIC ?
			"vPIC" : "vIOAPIC",
			entry->ptdev_intr_info.intx.phys_pin, phys_irq);
		dev_dbg(ACRN_DBG_IRQ, "from vm%d vpin=%d\n",
			entry->vm->attr.id,
			entry->ptdev_intr_info.intx.virt_pin);
	}

	release_entry(entry);

END:
	spinlock_release(&ptdev_lock);
}

static void ptdev_intr_handle_irq(struct vm *vm,
		struct ptdev_remapping_info *entry)
{
	struct ptdev_intx_info * intx = &entry->ptdev_intr_info.intx;
	switch (intx->vpin_src) {
	case PTDEV_VPIN_IOAPIC:
	{
		union ioapic_rte rte;
		bool trigger_lvl = false;

		/* VPIN_IOAPIC src means we have vioapic enabled */
		vioapic_get_rte(vm, intx->virt_pin,
			&rte);
		if ((rte.full & IOAPIC_RTE_TRGRMOD) == IOAPIC_RTE_TRGRLVL) {
			trigger_lvl = true;
		}

		if (trigger_lvl) {
			vioapic_assert_irq(vm,
			        intx->virt_pin);
		} else {
			vioapic_pulse_irq(vm,
			        intx->virt_pin);
		}

		dev_dbg(ACRN_DBG_PTIRQ,
			"dev-assign: irq=0x%x assert vr: 0x%x vRTE=0x%lx",
			dev_to_irq(entry->node),
			irq_to_vector(dev_to_irq(entry->node)),
			rte.full);
		break;
	}
	case PTDEV_VPIN_PIC:
	{
		enum vpic_trigger trigger;

		/* VPIN_PIC src means we have vpic enabled */
		vpic_get_irq_trigger(vm,
		        intx->virt_pin, &trigger);
		if (trigger == LEVEL_TRIGGER) {
			vpic_assert_irq(vm,
			        intx->virt_pin);
		} else {
			vpic_pulse_irq(vm,
			        intx->virt_pin);
		}
		break;
	}
	default:
		break;
	}
}

void ptdev_softirq(__unused uint16_t cpu_id)
{
	while (1) {
		struct ptdev_remapping_info *entry = ptdev_dequeue_softirq();
		struct ptdev_msi_info *msi = &entry->ptdev_intr_info.msi;
		struct vm *vm;

		if (entry == NULL) {
			break;
		}

		/* skip any inactive entry */
		if (!is_entry_active(entry)) {
			/* service next item */
			continue;
		}

		/* TBD: need valid vm */
		vm = entry->vm;

		/* handle real request */
		if (entry->type == PTDEV_INTR_INTX) {
			ptdev_intr_handle_irq(vm, entry);
		} else {
			/* TODO: msi destmode check required */
			vlapic_intr_msi(vm,
				        msi->vmsi_addr,
				        msi->vmsi_data);
			dev_dbg(ACRN_DBG_PTIRQ,
				"dev-assign: irq=0x%x MSI VR: 0x%x-0x%x",
				dev_to_irq(entry->node),
			        msi->virt_vector,
				irq_to_vector(dev_to_irq(entry->node)));
			dev_dbg(ACRN_DBG_PTIRQ,
				" vmsi_addr: 0x%x vmsi_data: 0x%x",
			        msi->vmsi_addr,
			        msi->vmsi_data);
		}
	}
}

void ptdev_intx_ack(struct vm *vm, uint8_t virt_pin,
		enum ptdev_vpin_source vpin_src)
{
	uint32_t phys_irq;
	struct ptdev_remapping_info *entry;
	uint8_t phys_pin;

	entry = lookup_entry_by_vintx(vm, virt_pin, vpin_src);
	if (entry == NULL) {
		return;
	}

	phys_pin = entry->ptdev_intr_info.intx.phys_pin;
	phys_irq = pin_to_irq(phys_pin);
	if (!irq_is_gsi(phys_irq)) {
		return;
	}

	/* NOTE: only Level trigger will process EOI/ACK and if we got here
	 * means we have this vioapic or vpic or both enabled
	 */
	switch (entry->ptdev_intr_info.intx.vpin_src) {
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
	struct ptdev_remapping_info *entry;
	bool lowpri = !is_vm0(vm);

	/*
	 * Device Model should pre-hold the mapping entries by calling
	 * ptdev_add_msix_remapping for UOS.
	 *
	 * For SOS(vm0), it adds the mapping entries at runtime, if the
	 * entry already be held by others, return error.
	 */

	entry = lookup_entry_by_vmsi(vm, virt_bdf, info->msix_entry_index);
	if (entry == NULL) {
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
	if (is_entry_active(entry) && info->vmsi_data == 0U) {
		info->pmsi_data = 0U;
		ptdev_deactivate_entry(entry);
		goto END;
	}

	if (!is_entry_active(entry)) {
		/* update msi source and active entry */
		ptdev_activate_entry(entry, IRQ_INVALID, lowpri);
	}

	/* build physical config MSI, update to info->pmsi_xxx */
	ptdev_build_physical_msi(vm, info, dev_to_vector(entry->node));
	entry->ptdev_intr_info.msi = *info;
	entry->ptdev_intr_info.msi.virt_vector = info->vmsi_data & 0xFFU;
	entry->ptdev_intr_info.msi.phys_vector = dev_to_vector(entry->node);

	/* update irq handler according to info in guest */
	ptdev_update_irq_handler(vm, entry);

	dev_dbg(ACRN_DBG_IRQ,
		"PCI %x:%x.%x MSI VR[%d] 0x%x->0x%x assigned to vm%d",
		(entry->virt_bdf >> 8) & 0xFFU,
		(entry->virt_bdf >> 3) & 0x1FU,
		(entry->virt_bdf) & 0x7U,
		entry->ptdev_intr_info.msi.msix_entry_index,
		entry->ptdev_intr_info.msi.virt_vector,
		entry->ptdev_intr_info.msi.phys_vector,
		entry->vm->attr.id);
END:
	return 0;
}

static bool vpin_masked(struct vm *vm, uint8_t virt_pin,
	enum ptdev_vpin_source vpin_src)
{
	if (vpin_src == PTDEV_VPIN_IOAPIC) {
		union ioapic_rte rte;

		vioapic_get_rte(vm, virt_pin, &rte);
		return ((rte.full & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET);
	} else {
		return vpic_is_pin_mask(vm->vpic, virt_pin);
	}
}

static void activate_physical_ioapic(struct vm *vm,
		struct ptdev_remapping_info *entry)
{
	union ioapic_rte rte;
	uint32_t phys_irq = dev_to_irq(entry->node);

	/* disable interrupt */
	GSI_MASK_IRQ(phys_irq);

	/* build physical IOAPIC RTE */
	rte = ptdev_build_physical_rte(vm, entry);

	/* set rte entry */
	rte.full |= IOAPIC_RTE_INTMSET;
	ioapic_set_rte(phys_irq, rte);

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
	union ioapic_rte rte;
	uint32_t phys_irq;
	uint8_t phys_pin;
	bool lowpri = !is_vm0(vm);
	bool need_switch_vpin_src = false;
	struct ptdev_intx_info *intx;

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
	if (ptdev_hv_owned_intx(vm, info)) {
		goto END;
	}

	/* query if we have virt to phys mapping */
	entry = lookup_entry_by_vintx(vm, info->virt_pin, info->vpin_src);
	if (entry == NULL) {
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
				if (entry != NULL) {
					need_switch_vpin_src = true;
				}
			}

			/* entry could be updated by above switch check */
			if (entry == NULL) {
				/* allocate entry during first unmask */
				if (vpin_masked(vm, info->virt_pin,
						info->vpin_src)) {
					goto END;
				}

				info->phys_pin = info->virt_pin;
				/* fix vPIC pin to correct native IOAPIC pin */
				if (pic_pin) {
					info->phys_pin =
					pic_ioapic_pin_map[info->virt_pin];
				}

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
	intx =  &entry->ptdev_intr_info.intx;

	/* no need update if vpin is masked && entry is not active */
	if (!is_entry_active(entry) &&
		vpin_masked(vm, info->virt_pin, info->vpin_src)) {
		goto END;
	}

	/* phys_pin from physical IOAPIC */
	phys_pin = entry->ptdev_intr_info.intx.phys_pin;
	phys_irq = pin_to_irq(phys_pin);
	if (!irq_is_gsi(phys_irq)) {
		goto END;
	}

	/* if vpin source need switch, make sure the entry is deactived */
	if (need_switch_vpin_src) {
		if (is_entry_active(entry)) {
			GSI_MASK_IRQ(phys_irq);
			ptdev_deactivate_entry(entry);
		}
		dev_dbg(ACRN_DBG_IRQ,
			"IOAPIC pin=%hhu pirq=%u vpin=%d switch from %s to %s "
			"vpin=%d for vm%d", phys_pin, phys_irq,
			entry->ptdev_intr_info.intx.virt_pin,
			(entry->ptdev_intr_info.intx.vpin_src != 0)?
				"vPIC" : "vIOAPIC",
			(entry->ptdev_intr_info.intx.vpin_src != 0)?
				"vIOPIC" : "vPIC",
			info->virt_pin,
			entry->vm->attr.id);
	        intx->vpin_src = info->vpin_src;
	        intx->virt_pin = info->virt_pin;
	}

	if (is_entry_active(entry)
		&& (intx->vpin_src
			== PTDEV_VPIN_IOAPIC)) {
		vioapic_get_rte(vm, intx->virt_pin, &rte);
		if (rte.u.lo_32 == 0x10000U) {
			/* disable interrupt */
			GSI_MASK_IRQ(phys_irq);
			ptdev_deactivate_entry(entry);
			dev_dbg(ACRN_DBG_IRQ,
				"IOAPIC pin=%hhu pirq=%u deassigned ",
				phys_pin, phys_irq);
			dev_dbg(ACRN_DBG_IRQ, "from vm%d vIOAPIC vpin=%d",
				entry->vm->attr.id,
			        intx->virt_pin);
			goto END;
		} else {
			/*update rte*/
			activate_physical_ioapic(vm, entry);
		}
	} else if (is_entry_active(entry)
		&& (intx->vpin_src == PTDEV_VPIN_PIC)) {
		/* only update here
		 * deactive vPIC entry when IOAPIC take it over
		 */
		activate_physical_ioapic(vm, entry);
	} else {
		/* active entry */
		ptdev_activate_entry(entry, phys_irq, lowpri);

		activate_physical_ioapic(vm, entry);

		dev_dbg(ACRN_DBG_IRQ,
			"IOAPIC pin=%hhu pirq=%u assigned to vm%d %s vpin=%d",
			phys_pin, phys_irq, entry->vm->attr.id,
		        intx->vpin_src == PTDEV_VPIN_PIC ?
			"vPIC" : "vIOAPIC",
		        intx->virt_pin);
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
	if (is_entry_invalid(entry)) {
		return -ENODEV;
	}

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
		uint16_t phys_bdf, uint32_t vector_count)
{
	struct ptdev_remapping_info *entry;
	uint32_t i;

	for (i = 0U; i < vector_count; i++) {
		entry = add_msix_remapping(vm, virt_bdf, phys_bdf, i);
		if (is_entry_invalid(entry)) {
			return -ENODEV;
		}
	}

	return 0;
}

void ptdev_remove_msix_remapping(struct vm *vm, uint16_t virt_bdf,
		uint32_t vector_count)
{
	uint32_t i;

	if (vm == NULL) {
		pr_err("ptdev_remove_msix_remapping fails!\n");
		return;
	}

	for (i = 0U; i < vector_count; i++) {
		remove_msix_remapping(vm, virt_bdf, i);
	}
}

#ifdef HV_DEBUG
static void get_entry_info(struct ptdev_remapping_info *entry, char *type,
		uint32_t *irq, uint32_t *vector, uint64_t *dest, bool *lvl_tm,
		int *pin, int *vpin, uint32_t *bdf, uint32_t *vbdf)
{
	struct ptdev_intx_info *intx = &entry->ptdev_intr_info.intx;
	if (is_entry_active(entry)) {
		if (entry->type == PTDEV_INTR_MSI) {
			(void)strcpy_s(type, 16U, "MSI");
			*dest = (entry->ptdev_intr_info.msi.pmsi_addr & 0xFF000U)
				>> 12;
			if ((entry->ptdev_intr_info.msi.pmsi_data &
				APIC_TRIGMOD_LEVEL) != 0U) {
				*lvl_tm = true;
			} else {
				*lvl_tm = false;
			}
			*pin = IRQ_INVALID;
			*vpin = -1;
			*bdf = entry->phys_bdf;
			*vbdf = entry->virt_bdf;
		} else {
			uint32_t phys_irq = pin_to_irq(
			        intx->phys_pin);
			union ioapic_rte rte;

			if (intx->vpin_src
				== PTDEV_VPIN_IOAPIC) {
				(void)strcpy_s(type, 16U, "IOAPIC");
			} else {
				(void)strcpy_s(type, 16U, "PIC");
			}
			ioapic_get_rte(phys_irq, &rte);
			*dest = rte.full >> IOAPIC_RTE_DEST_SHIFT;
			if ((rte.full & IOAPIC_RTE_TRGRLVL) != 0UL) {
				*lvl_tm = true;
			} else {
				*lvl_tm = false;
			}
			*pin = intx->phys_pin;
			*vpin = intx->virt_pin;
			*bdf = 0U;
			*vbdf = 0U;
		}
		*irq = dev_to_irq(entry->node);
		*vector = dev_to_vector(entry->node);
	} else {
		(void)strcpy_s(type, 16U, "NONE");
		*irq = IRQ_INVALID;
		*vector = 0U;
		*dest = 0UL;
		*lvl_tm = 0;
		*pin = -1;
		*vpin = -1;
		*bdf = 0U;
		*vbdf = 0U;
	}
}

void get_ptdev_info(char *str, int str_max)
{
	struct ptdev_remapping_info *entry;
	int len, size = str_max;
	uint32_t irq, vector;
	char type[16];
	uint64_t dest;
	bool lvl_tm;
	int32_t pin, vpin;
	uint32_t bdf, vbdf;
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
					(bdf & 0xff00U) >> 8,
					(bdf & 0xf8U) >> 3, bdf & 0x7U,
					(vbdf & 0xff00U) >> 8,
					(vbdf & 0xf8U) >> 3, vbdf & 0x7U);
			size -= len;
			str += len;
		}
	}
	spinlock_release(&ptdev_lock);

	snprintf(str, size, "\r\n");
}
#endif /* HV_DEBUG */
