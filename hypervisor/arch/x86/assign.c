/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

/*
 * lookup a ptdev entry by sid
 * Before adding a ptdev remapping, should lookup by physical sid to check
 * whether the resource has been token by others.
 * When updating a ptdev remapping, should lookup by virtual sid to check
 * whether this resource is valid.
 * @pre: vm must be NULL when lookup by physical sid, otherwise,
 * vm must not be NULL when lookup by virtual sid.
 */
static inline struct ptdev_remapping_info *
ptdev_lookup_entry_by_sid(uint32_t intr_type,
		const union source_id *sid,const struct acrn_vm *vm)
{
	struct ptdev_remapping_info *entry;
	struct list_head *pos;

	list_for_each(pos, &ptdev_list) {
		entry = list_entry(pos, struct ptdev_remapping_info,
				entry_node);
		if ((intr_type == entry->intr_type) &&
			((vm == NULL) ?
			(sid->value == entry->phys_sid.value) :
			((vm == entry->vm) &&
			(sid->value == entry->virt_sid.value)))) {
			return entry;
		}
	}

	return NULL;
}

static inline bool
is_entry_active(const struct ptdev_remapping_info *entry)
{
	return atomic_load32(&entry->active) == ACTIVE_FLAG;
}

static bool ptdev_hv_owned_intx(const struct acrn_vm *vm, const union source_id *virt_sid)
{
	/* vm0 vuart pin is owned by hypervisor under debug version */
	if (is_vm0(vm) && (virt_sid->intx_id.pin == COM1_IRQ)) {
		return true;
	} else {
		return false;
	}
}

static uint64_t calculate_logical_dest_mask(uint64_t pdmask)
{
	uint64_t dest_mask = 0UL;
	uint64_t pcpu_mask = pdmask;
	uint16_t pcpu_id;

	pcpu_id = ffs64(pcpu_mask);
	while (pcpu_id != INVALID_BIT_INDEX) {
		bitmap_clear_nolock(pcpu_id, &pcpu_mask);
		dest_mask |= per_cpu(lapic_ldr, pcpu_id);
		pcpu_id = ffs64(pcpu_mask);
	}
	return dest_mask;
}

static void ptdev_build_physical_msi(struct acrn_vm *vm, struct ptdev_msi_info *info,
		uint32_t vector)
{
	uint64_t vdmask, pdmask, dest_mask;
	uint32_t dest, delmode;
	bool phys;

	/* get physical destination cpu mask */
	dest = (uint32_t)(info->vmsi_addr >> CPU_PAGE_SHIFT) & 0xffU;
	phys = ((info->vmsi_addr & MSI_ADDR_LOG) != MSI_ADDR_LOG);

	calcvdest(vm, &vdmask, dest, phys);
	pdmask = vcpumask2pcpumask(vm, vdmask);

	/* get physical delivery mode */
	delmode = info->vmsi_data & APIC_DELMODE_MASK;
	if ((delmode != APIC_DELMODE_FIXED) && (delmode != APIC_DELMODE_LOWPRIO)) {
		delmode = APIC_DELMODE_LOWPRIO;
	}

	/* update physical delivery mode & vector */
	info->pmsi_data = info->vmsi_data;
	info->pmsi_data &= ~0x7FFU;
	info->pmsi_data |= delmode | vector;

	dest_mask = calculate_logical_dest_mask(pdmask);
	/* update physical dest mode & dest field */
	info->pmsi_addr = info->vmsi_addr;
	info->pmsi_addr &= ~0xFF00CU;
	info->pmsi_addr |= (dest_mask << CPU_PAGE_SHIFT) | MSI_ADDR_RH | MSI_ADDR_LOG;

	dev_dbg(ACRN_DBG_IRQ, "MSI addr:data = 0x%llx:%x(V) -> 0x%llx:%x(P)",
		info->vmsi_addr, info->vmsi_data,
		info->pmsi_addr, info->pmsi_data);
}

static union ioapic_rte
ptdev_build_physical_rte(struct acrn_vm *vm,
		struct ptdev_remapping_info *entry)
{
	union ioapic_rte rte;
	uint32_t phys_irq = entry->allocated_pirq;
	uint32_t vector = irq_to_vector(phys_irq);
	union source_id *virt_sid = &entry->virt_sid;

	if (virt_sid->intx_id.src == PTDEV_VPIN_IOAPIC) {
		uint64_t vdmask, pdmask, delmode, dest_mask;
		uint32_t dest;
		union ioapic_rte virt_rte;
		bool phys;

		vioapic_get_rte(vm, virt_sid->intx_id.pin, &virt_rte);
		rte = virt_rte;

		/* init polarity & pin state */
		if ((rte.full & IOAPIC_RTE_INTPOL) != 0UL) {
			if (entry->polarity == 0U) {
				vioapic_set_irq_nolock(vm,
					(uint32_t)virt_sid->intx_id.pin,
					GSI_SET_HIGH);
			}
			entry->polarity = 1U;
		} else {
			if (entry->polarity == 1U) {
				vioapic_set_irq_nolock(vm,
					(uint32_t)virt_sid->intx_id.pin,
					GSI_SET_LOW);
			}
			entry->polarity = 0U;
		}

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

		dest_mask = calculate_logical_dest_mask(pdmask);
		/* update physical dest field */
		rte.full &= ~IOAPIC_RTE_DEST_MASK;
		rte.full |= dest_mask << IOAPIC_RTE_DEST_SHIFT;

		dev_dbg(ACRN_DBG_IRQ, "IOAPIC RTE = 0x%x:%x(V) -> 0x%x:%x(P)",
			virt_rte.u.hi_32, virt_rte.u.lo_32,
			rte.u.hi_32, rte.u.lo_32);
	} else {
		enum vpic_trigger trigger;
		union ioapic_rte phys_rte;

		/* just update trigger mode */
		ioapic_get_rte(phys_irq, &phys_rte);
		rte.full = phys_rte.full & (~IOAPIC_RTE_TRGRMOD);
		vpic_get_irq_trigger(vm, virt_sid->intx_id.pin, &trigger);
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
 * - if the entry already be added by other vm, return NULL
 */
static struct ptdev_remapping_info *
add_msix_remapping(struct acrn_vm *vm, uint16_t virt_bdf, uint16_t phys_bdf,
		uint32_t entry_nr)
{
	struct ptdev_remapping_info *entry;
	DEFINE_MSI_SID(phys_sid, phys_bdf, entry_nr);
	DEFINE_MSI_SID(virt_sid, virt_bdf, entry_nr);

	spinlock_obtain(&ptdev_lock);
	entry = ptdev_lookup_entry_by_sid(PTDEV_INTR_MSI, &phys_sid, NULL);
	if (entry == NULL) {
		if (ptdev_lookup_entry_by_sid(PTDEV_INTR_MSI,
				&virt_sid, vm) != NULL) {
			pr_err("MSIX re-add vbdf%x", virt_bdf);

			spinlock_release(&ptdev_lock);
			return NULL;
		}
		entry = alloc_entry(vm, PTDEV_INTR_MSI);
		entry->phys_sid.value = phys_sid.value;
		entry->virt_sid.value = virt_sid.value;

		/* update msi source and active entry */
		ptdev_activate_entry(entry, IRQ_INVALID);
	} else if (entry->vm != vm) {
		if (is_vm0(entry->vm)) {
			entry->vm = vm;
			entry->virt_sid.msi_id.bdf = virt_bdf;
		} else {
			pr_err("MSIX pbdf%x idx=%d already in vm%d with vbdf%x,"
				" not able to add into vm%d with vbdf%x",
				entry->phys_sid.msi_id.bdf,
				entry->phys_sid.msi_id.entry_nr,
				entry->vm->vm_id, entry->virt_sid.msi_id.bdf,
				vm->vm_id, virt_bdf);
			ASSERT(false, "msix entry pbdf%x idx%d already in vm%d",
			       phys_bdf, entry_nr, entry->vm->vm_id);

			spinlock_release(&ptdev_lock);
			return NULL;
		}
	} else {
		/* The mapping has already been added to the VM. No action
		 * required. */
	}
	spinlock_release(&ptdev_lock);

	dev_dbg(ACRN_DBG_IRQ,
		"VM%d MSIX add vector mapping vbdf%x:pbdf%x idx=%d",
		entry->vm->vm_id, virt_bdf, phys_bdf, entry_nr);

	return entry;
}

/* deactive & remove mapping entry of vbdf:entry_nr for vm */
static void
remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf, uint32_t entry_nr)
{
	struct ptdev_remapping_info *entry;
	DEFINE_MSI_SID(virt_sid, virt_bdf, entry_nr);

	spinlock_obtain(&ptdev_lock);
	entry = ptdev_lookup_entry_by_sid(PTDEV_INTR_MSI, &virt_sid, vm);
	if (entry == NULL) {
		goto END;
	}

	if (is_entry_active(entry)) {
		/*TODO: disable MSIX device when HV can in future */
		ptdev_deactivate_entry(entry);
	}

	dev_dbg(ACRN_DBG_IRQ,
		"VM%d MSIX remove vector mapping vbdf-pbdf:0x%x-0x%x idx=%d",
		entry->vm->vm_id, virt_bdf,
		entry->phys_sid.msi_id.bdf, entry_nr);

	release_entry(entry);

END:
	spinlock_release(&ptdev_lock);

}

/* add intx entry for a vm, based on intx id (phys_pin)
 * - if the entry not be added by any vm, allocate it
 * - if the entry already be added by vm0, then change the owner to current vm
 * - if the entry already be added by other vm, return NULL
 */
static struct ptdev_remapping_info *
add_intx_remapping(struct acrn_vm *vm, uint8_t virt_pin,
		uint8_t phys_pin, bool pic_pin)
{
	struct ptdev_remapping_info *entry;
	enum ptdev_vpin_source vpin_src =
		pic_pin ? PTDEV_VPIN_PIC : PTDEV_VPIN_IOAPIC;
	DEFINE_IOAPIC_SID(phys_sid, phys_pin, 0);
	DEFINE_IOAPIC_SID(virt_sid, virt_pin, vpin_src);
	uint32_t phys_irq = pin_to_irq(phys_pin);

	if (!irq_is_gsi(phys_irq)) {
		pr_err("%s, invalid phys_pin: %d <-> irq: 0x%x is not a GSI\n",
			__func__, phys_pin, phys_irq);
		return NULL;
	}

	spinlock_obtain(&ptdev_lock);
	entry = ptdev_lookup_entry_by_sid(PTDEV_INTR_INTX, &phys_sid, NULL);
	if (entry == NULL) {
		if (ptdev_lookup_entry_by_sid(PTDEV_INTR_INTX,
				&virt_sid, vm) != NULL) {
			pr_err("INTX re-add vpin %d", virt_pin);
			spinlock_release(&ptdev_lock);
			return NULL;
		}
		entry = alloc_entry(vm, PTDEV_INTR_INTX);
		entry->phys_sid.value = phys_sid.value;
		entry->virt_sid.value = virt_sid.value;

		/* activate entry */
		ptdev_activate_entry(entry, phys_irq);
	} else if (entry->vm != vm) {
		if (is_vm0(entry->vm)) {
			entry->vm = vm;
			entry->virt_sid.value = virt_sid.value;
		} else {
			pr_err("INTX pin%d already in vm%d with vpin%d,"
				" not able to add into vm%d with vpin%d",
				phys_pin, entry->vm->vm_id,
				entry->virt_sid.intx_id.pin,
				vm->vm_id, virt_pin);

			spinlock_release(&ptdev_lock);
			return NULL;
		}
	} else {
		/* The mapping has already been added to the VM. No action
		 * required. */
	}

	spinlock_release(&ptdev_lock);

	dev_dbg(ACRN_DBG_IRQ,
		"VM%d INTX add pin mapping vpin%d:ppin%d",
		entry->vm->vm_id, virt_pin, phys_pin);

	return entry;
}

/* deactive & remove mapping entry of vpin for vm */
static void remove_intx_remapping(const struct acrn_vm *vm, uint8_t virt_pin, bool pic_pin)
{
	uint32_t phys_irq;
	struct ptdev_remapping_info *entry;
	enum ptdev_vpin_source vpin_src =
		pic_pin ? PTDEV_VPIN_PIC : PTDEV_VPIN_IOAPIC;
	DEFINE_IOAPIC_SID(virt_sid, virt_pin, vpin_src);

	spinlock_obtain(&ptdev_lock);
	entry = ptdev_lookup_entry_by_sid(PTDEV_INTR_INTX, &virt_sid, vm);
	if (entry == NULL) {
		goto END;
	}

	if (is_entry_active(entry)) {
		phys_irq = entry->allocated_pirq;
		/* disable interrupt */
		gsi_mask_irq(phys_irq);

		ptdev_deactivate_entry(entry);
		dev_dbg(ACRN_DBG_IRQ,
			"deactive %s intx entry:ppin=%d, pirq=%d ",
			vpin_src == PTDEV_VPIN_PIC ?
			"vPIC" : "vIOAPIC",
			entry->phys_sid.intx_id.pin, phys_irq);
		dev_dbg(ACRN_DBG_IRQ, "from vm%d vpin=%d\n",
			entry->vm->vm_id, virt_pin);
	}

	release_entry(entry);

END:
	spinlock_release(&ptdev_lock);
}

static void ptdev_intr_handle_irq(struct acrn_vm *vm,
		const struct ptdev_remapping_info *entry)
{
	const union source_id *virt_sid = &entry->virt_sid;
	switch (virt_sid->intx_id.src) {
	case PTDEV_VPIN_IOAPIC:
	{
		union ioapic_rte rte;
		bool trigger_lvl = false;

		/* VPIN_IOAPIC src means we have vioapic enabled */
		vioapic_get_rte(vm, virt_sid->intx_id.pin, &rte);
		if ((rte.full & IOAPIC_RTE_TRGRMOD) == IOAPIC_RTE_TRGRLVL) {
			trigger_lvl = true;
		}

		if (trigger_lvl) {
			if (entry->polarity != 0U) {
				vioapic_set_irq(vm, virt_sid->intx_id.pin,
						GSI_SET_LOW);
			} else {
				vioapic_set_irq(vm, virt_sid->intx_id.pin,
						GSI_SET_HIGH);
			}
		} else {
			if (entry->polarity != 0U) {
				vioapic_set_irq(vm, virt_sid->intx_id.pin,
						GSI_FALLING_PULSE);
			} else {
				vioapic_set_irq(vm, virt_sid->intx_id.pin,
						GSI_RAISING_PULSE);
			}
		}

		dev_dbg(ACRN_DBG_PTIRQ,
			"dev-assign: irq=0x%x assert vr: 0x%x vRTE=0x%lx",
			entry->allocated_pirq,
			irq_to_vector(entry->allocated_pirq),
			rte.full);
		break;
	}
	case PTDEV_VPIN_PIC:
	{
		enum vpic_trigger trigger;

		/* VPIN_PIC src means we have vpic enabled */
		vpic_get_irq_trigger(vm, virt_sid->intx_id.pin, &trigger);
		if (trigger == LEVEL_TRIGGER) {
			vpic_set_irq(vm, virt_sid->intx_id.pin, GSI_SET_HIGH);
		} else {
			vpic_set_irq(vm, virt_sid->intx_id.pin,
					GSI_RAISING_PULSE);
		}
		break;
	}
	default:
		/*
		 * In this switch statement, virt_sid->intx_id.src shall
		 * either be PTDEV_VPIN_IOAPIC or PTDEV_VPIN_PIC.
		 * Gracefully return if prior case clauses have not been met.
		 */
		break;
	}
}

void ptdev_softirq(uint16_t pcpu_id)
{
	struct acrn_vcpu *vcpu = (struct acrn_vcpu *)per_cpu(vcpu, pcpu_id);
	struct acrn_vm *vm = vcpu->vm;

	while (1) {
		struct ptdev_remapping_info *entry = ptdev_dequeue_softirq(vm);
		struct ptdev_msi_info *msi;

		if (entry == NULL) {
			break;
		}

		msi = &entry->msi;

		/* skip any inactive entry */
		if (!is_entry_active(entry)) {
			/* service next item */
			continue;
		}

		/* handle real request */
		if (entry->intr_type == PTDEV_INTR_INTX) {
			ptdev_intr_handle_irq(vm, entry);
		} else {
			/* TODO: msi destmode check required */
			(void)vlapic_intr_msi(vm,
				        msi->vmsi_addr,
				        msi->vmsi_data);
			dev_dbg(ACRN_DBG_PTIRQ,
				"dev-assign: irq=0x%x MSI VR: 0x%x-0x%x",
				entry->allocated_pirq,
				msi->vmsi_data & 0xFFU,
				irq_to_vector(entry->allocated_pirq));
			dev_dbg(ACRN_DBG_PTIRQ,
				" vmsi_addr: 0x%llx vmsi_data: 0x%x",
			        msi->vmsi_addr,
			        msi->vmsi_data);
		}
	}
}

void ptdev_intx_ack(struct acrn_vm *vm, uint8_t virt_pin,
		enum ptdev_vpin_source vpin_src)
{
	uint32_t phys_irq;
	struct ptdev_remapping_info *entry;
	DEFINE_IOAPIC_SID(virt_sid, virt_pin, vpin_src);

	spinlock_obtain(&ptdev_lock);
	entry = ptdev_lookup_entry_by_sid(PTDEV_INTR_INTX, &virt_sid, vm);
	spinlock_release(&ptdev_lock);
	if (entry == NULL) {
		return;
	}

	phys_irq = entry->allocated_pirq;

	/* NOTE: only Level trigger will process EOI/ACK and if we got here
	 * means we have this vioapic or vpic or both enabled
	 */
	switch (vpin_src) {
	case PTDEV_VPIN_IOAPIC:
		if (entry->polarity != 0U) {
			vioapic_set_irq(vm, virt_pin, GSI_SET_HIGH);
		} else {
			vioapic_set_irq(vm, virt_pin, GSI_SET_LOW);
		}
		break;
	case PTDEV_VPIN_PIC:
		vpic_set_irq(vm, virt_pin, GSI_SET_LOW);
	default:
		/*
		 * In this switch statement, vpin_src shall either be
		 * PTDEV_VPIN_IOAPIC or PTDEV_VPIN_PIC.
		 * Gracefully return if prior case clauses have not been met.
		 */
		break;
	}

	dev_dbg(ACRN_DBG_PTIRQ, "dev-assign: irq=0x%x acked vr: 0x%x",
			phys_irq, irq_to_vector(phys_irq));
	gsi_unmask_irq(phys_irq);
}

/* Main entry for PCI device assignment with MSI and MSI-X
 * MSI can up to 8 vectors and MSI-X can up to 1024 Vectors
 * We use entry_nr to indicate coming vectors
 * entry_nr = 0 means first vector
 * user must provide bdf and entry_nr
 *
 * This function is called by SOS pci MSI config routine through hcall
 */
int ptdev_msix_remap(struct acrn_vm *vm, uint16_t virt_bdf,
		uint16_t entry_nr, struct ptdev_msi_info *info)
{
	struct ptdev_remapping_info *entry;
	DEFINE_MSI_SID(virt_sid, virt_bdf, entry_nr);

	/*
	 * Device Model should pre-hold the mapping entries by calling
	 * ptdev_add_msix_remapping for UOS.
	 *
	 * For SOS(vm0), it adds the mapping entries at runtime, if the
	 * entry already be held by others, return error.
	 */
	spinlock_obtain(&ptdev_lock);
	entry = ptdev_lookup_entry_by_sid(PTDEV_INTR_MSI, &virt_sid, vm);
	spinlock_release(&ptdev_lock);
	if (entry == NULL) {
		/* VM0 we add mapping dynamically */
		if (is_vm0(vm)) {
			entry = add_msix_remapping(vm,
					virt_bdf, virt_bdf, entry_nr);
			if (entry == NULL) {
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
	if (is_entry_active(entry) && (info->vmsi_data == 0U)) {
		info->pmsi_data = 0U;
		goto END;
	}

	/* build physical config MSI, update to info->pmsi_xxx */
	ptdev_build_physical_msi(vm, info, irq_to_vector(entry->allocated_pirq));
	entry->msi = *info;

	dev_dbg(ACRN_DBG_IRQ, "PCI %x:%x.%x MSI VR[%d] 0x%x->0x%x assigned to vm%d",
		pci_bus(virt_bdf), pci_slot(virt_bdf), pci_func(virt_bdf), entry_nr,
		info->vmsi_data & 0xFFU, irq_to_vector(entry->allocated_pirq), entry->vm->vm_id);
END:
	return 0;
}

static void activate_physical_ioapic(struct acrn_vm *vm,
		struct ptdev_remapping_info *entry)
{
	union ioapic_rte rte;
	uint32_t phys_irq = entry->allocated_pirq;
	uint32_t intr_mask;
	bool is_lvl_trigger = false;

	/* disable interrupt */
	gsi_mask_irq(phys_irq);

	/* build physical IOAPIC RTE */
	rte = ptdev_build_physical_rte(vm, entry);
	intr_mask = (rte.full & IOAPIC_RTE_INTMASK);

	/* update irq trigger mode according to info in guest */
	if ((rte.full & IOAPIC_RTE_TRGRMOD) == IOAPIC_RTE_TRGRLVL) {
		is_lvl_trigger = true;
	}
	set_irq_trigger_mode(phys_irq, is_lvl_trigger);

	/* set rte entry when masked */
	rte.full |= IOAPIC_RTE_INTMSET;
	ioapic_set_rte(phys_irq, rte);

	if (intr_mask == IOAPIC_RTE_INTMCLR) {
		gsi_unmask_irq(phys_irq);
	}
}

/* Main entry for PCI/Legacy device assignment with INTx, calling from vIOAPIC
 * or vPIC
 */
int ptdev_intx_pin_remap(struct acrn_vm *vm, uint8_t virt_pin,
		enum ptdev_vpin_source vpin_src)
{
	struct ptdev_remapping_info *entry;
	bool need_switch_vpin_src = false;
	DEFINE_IOAPIC_SID(virt_sid, virt_pin, vpin_src);

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
	if (ptdev_hv_owned_intx(vm, &virt_sid)) {
		goto END;
	}

	/* query if we have virt to phys mapping */
	spinlock_obtain(&ptdev_lock);
	entry = ptdev_lookup_entry_by_sid(PTDEV_INTR_INTX, &virt_sid, vm);
	spinlock_release(&ptdev_lock);
	if (entry == NULL) {
		if (is_vm0(vm)) {
			bool pic_pin = (vpin_src == PTDEV_VPIN_PIC);

			/* for vm0, there is chance of vpin source switch
			 * between vPIC & vIOAPIC for one legacy phys_pin.
			 *
			 * here checks if there is already mapping entry from
			 * the other vpin source for legacy pin. If yes, then
			 * switch vpin source is needed
			 */
			if (virt_pin < NR_LEGACY_PIN) {
				DEFINE_IOAPIC_SID(tmp_vsid,
					pic_ioapic_pin_map[virt_pin],
					pic_pin ? PTDEV_VPIN_IOAPIC :
					PTDEV_VPIN_PIC);
				spinlock_obtain(&ptdev_lock);
				entry = ptdev_lookup_entry_by_sid(
					PTDEV_INTR_INTX, &tmp_vsid, vm);
				spinlock_release(&ptdev_lock);
				if (entry != NULL) {
					need_switch_vpin_src = true;
				}
			}

			/* entry could be updated by above switch check */
			if (entry == NULL) {
				uint8_t phys_pin = virt_pin;

				/* fix vPIC pin to correct native IOAPIC pin */
				if (pic_pin) {
					phys_pin = pic_ioapic_pin_map[virt_pin];
				}
				entry = add_intx_remapping(vm,
						virt_pin, phys_pin, pic_pin);
				if (entry == NULL) {
					pr_err("%s, add intx remapping failed",
						__func__);
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

	/* if vpin source need switch */
	if (need_switch_vpin_src) {
		dev_dbg(ACRN_DBG_IRQ,
			"IOAPIC pin=%hhu pirq=%u vpin=%d switch from %s to %s "
			"vpin=%d for vm%d", entry->phys_sid.intx_id.pin,
			entry->allocated_pirq, entry->virt_sid.intx_id.pin,
			(vpin_src == 0)? "vPIC" : "vIOAPIC",
			(vpin_src == 0)? "vIOPIC" : "vPIC",
			virt_pin, entry->vm->vm_id);
	        entry->virt_sid.value = virt_sid.value;
	}

	activate_physical_ioapic(vm, entry);
	dev_dbg(ACRN_DBG_IRQ,
			"IOAPIC pin=%hhu pirq=%u assigned to vm%d %s vpin=%d",
			entry->phys_sid.intx_id.pin, entry->allocated_pirq,
			entry->vm->vm_id, vpin_src == PTDEV_VPIN_PIC ?
			"vPIC" : "vIOAPIC", virt_pin);
END:
	return 0;
}

/* @pre vm != NULL
 * except vm0, Device Model should call this function to pre-hold ptdev intx
 * entries:
 * - the entry is identified by phys_pin:
 *   one entry vs. one phys_pin
 * - currently, one phys_pin can only be held by one pin source (vPIC or
 *   vIOAPIC)
 */
int ptdev_add_intx_remapping(struct acrn_vm *vm, uint8_t virt_pin, uint8_t phys_pin,
				bool pic_pin)
{
	struct ptdev_remapping_info *entry;

	if ((!pic_pin && virt_pin >= vioapic_pincount(vm))
			|| (pic_pin && virt_pin >= vpic_pincount())) {
		pr_err("ptdev_add_intx_remapping fails!\n");
		return -EINVAL;
	}

	entry = add_intx_remapping(vm, virt_pin, phys_pin, pic_pin);

	return (entry != NULL) ? 0 : -ENODEV;
}

/*
 * @pre vm != NULL
 */
void ptdev_remove_intx_remapping(const struct acrn_vm *vm, uint8_t virt_pin, bool pic_pin)
{
	remove_intx_remapping(vm, virt_pin, pic_pin);
}

/* except vm0, Device Model should call this function to pre-hold ptdev msi
 * entries:
 * - the entry is identified by phys_bdf:msi_idx:
 *   one entry vs. one phys_bdf:msi_idx
 */
int ptdev_add_msix_remapping(struct acrn_vm *vm, uint16_t virt_bdf,
		uint16_t phys_bdf, uint32_t vector_count)
{
	struct ptdev_remapping_info *entry;
	uint32_t i;

	for (i = 0U; i < vector_count; i++) {
		entry = add_msix_remapping(vm, virt_bdf, phys_bdf, i);
		if (entry == NULL) {
			return -ENODEV;
		}
	}

	return 0;
}

/*
 * @pre vm != NULL
 */
void ptdev_remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf,
		uint32_t vector_count)
{
	uint32_t i;

	for (i = 0U; i < vector_count; i++) {
		remove_msix_remapping(vm, virt_bdf, i);
	}
}

#ifdef HV_DEBUG
#define PTDEV_INVALID_PIN 0xffU
static void get_entry_info(const struct ptdev_remapping_info *entry, char *type,
		uint32_t *irq, uint32_t *vector, uint64_t *dest, bool *lvl_tm,
		uint8_t *pin, uint8_t *vpin, uint32_t *bdf, uint32_t *vbdf)
{
	if (is_entry_active(entry)) {
		if (entry->intr_type == PTDEV_INTR_MSI) {
			(void)strcpy_s(type, 16U, "MSI");
			*dest = (entry->msi.pmsi_addr & 0xFF000U) >> CPU_PAGE_SHIFT;
			if ((entry->msi.pmsi_data & APIC_TRIGMOD_LEVEL) != 0U) {
				*lvl_tm = true;
			} else {
				*lvl_tm = false;
			}
			*pin = PTDEV_INVALID_PIN;
			*vpin = PTDEV_INVALID_PIN;
			*bdf = entry->phys_sid.msi_id.bdf;
			*vbdf = entry->virt_sid.msi_id.bdf;
		} else {
			uint32_t phys_irq = entry->allocated_pirq;
			union ioapic_rte rte;

			if (entry->virt_sid.intx_id.src == PTDEV_VPIN_IOAPIC) {
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
			*pin = entry->phys_sid.intx_id.pin;
			*vpin = entry->virt_sid.intx_id.pin;
			*bdf = 0U;
			*vbdf = 0U;
		}
		*irq = entry->allocated_pirq;
		*vector = irq_to_vector(entry->allocated_pirq);
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

void get_ptdev_info(char *str_arg, size_t str_max)
{
	char *str = str_arg;
	struct ptdev_remapping_info *entry;
	size_t len, size = str_max;
	uint32_t irq, vector;
	char type[16];
	uint64_t dest;
	bool lvl_tm;
	uint8_t pin, vpin;
	uint32_t bdf, vbdf;
	struct list_head *pos;

	len = snprintf(str, size, "\r\nVM\tTYPE\tIRQ\tVEC\tDEST\tTM\tPIN\tVPIN\tBDF\tVBDF");
	if (len >= size) {
		goto overflow;
	}
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
			len = snprintf(str, size, "\r\n%d\t%s\t%d\t0x%X\t0x%X",
					entry->vm->vm_id, type, irq, vector, dest);
			if (len >= size) {
				spinlock_release(&ptdev_lock);
				goto overflow;
			}
			size -= len;
			str += len;

			len = snprintf(str, size, "\t%s\t%hhu\t%hhu\t%x:%x.%x\t%x:%x.%x",
					is_entry_active(entry) ? (lvl_tm ? "level" : "edge") : "none",
					pin, vpin, (bdf & 0xff00U) >> 8U,
					(bdf & 0xf8U) >> 3U, bdf & 0x7U,
					(vbdf & 0xff00U) >> 8U,
					(vbdf & 0xf8U) >> 3U, vbdf & 0x7U);
			if (len >= size) {
				spinlock_release(&ptdev_lock);
				goto overflow;
			}
			size -= len;
			str += len;
		}
	}
	spinlock_release(&ptdev_lock);

	snprintf(str, size, "\r\n");
	return;

overflow:
	printf("buffer size could not be enough! please check!\n");
}
#endif /* HV_DEBUG */
