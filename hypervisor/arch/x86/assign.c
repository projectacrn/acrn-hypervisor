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
static inline struct ptirq_remapping_info *
ptirq_lookup_entry_by_sid(uint32_t intr_type,
		const union source_id *sid,const struct acrn_vm *vm)
{
	uint16_t idx;
	struct ptirq_remapping_info *entry;
	struct ptirq_remapping_info *entry_found = NULL;

	for (idx = 0U; idx < CONFIG_MAX_PT_IRQ_ENTRIES; idx++) {
		entry = &ptirq_entries[idx];
		if (!is_entry_active(entry)) {
			continue;
		}
		if ((intr_type == entry->intr_type) &&
			((vm == NULL) ?
			(sid->value == entry->phys_sid.value) :
			((vm == entry->vm) &&
			(sid->value == entry->virt_sid.value)))) {
			entry_found = entry;
			break;
		}
	}
	return entry_found;
}

static inline struct ptirq_remapping_info *
ptirq_lookup_entry_by_vpin(struct acrn_vm *vm, uint8_t virt_pin, bool pic_pin)
{
	struct ptirq_remapping_info *entry;

	if (pic_pin) {
		entry = vm->arch_vm.vpic.vpin_to_pt_entry[virt_pin];
	} else {
		entry = vm->arch_vm.vioapic.vpin_to_pt_entry[virt_pin];
	}
	return entry;
}

#ifdef CONFIG_COM_IRQ
static bool ptdev_hv_owned_intx(const struct acrn_vm *vm, const union source_id *virt_sid)
{
	bool ret;

	/* vm0 vuart pin is owned by hypervisor under debug version */
	if (is_vm0(vm) && (virt_sid->intx_id.pin == CONFIG_COM_IRQ)) {
		ret = true;
	} else {
	        ret = false;
	}

	return ret;
}
#endif /* CONFIG_COM_IRQ */

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

static void ptirq_build_physical_msi(struct acrn_vm *vm, struct ptirq_msi_info *info,
		uint32_t vector)
{
	uint64_t vdmask, pdmask, dest_mask;
	uint32_t dest, delmode;
	bool phys;

	/* get physical destination cpu mask */
	dest = (uint32_t)(info->vmsi_addr >> PAGE_SHIFT) & 0xffU;
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
	info->pmsi_addr |= (dest_mask << PAGE_SHIFT) | MSI_ADDR_RH | MSI_ADDR_LOG;

	dev_dbg(ACRN_DBG_IRQ, "MSI addr:data = 0x%llx:%x(V) -> 0x%llx:%x(P)",
		info->vmsi_addr, info->vmsi_data,
		info->pmsi_addr, info->pmsi_data);
}

static union ioapic_rte
ptirq_build_physical_rte(struct acrn_vm *vm, struct ptirq_remapping_info *entry)
{
	union ioapic_rte rte;
	uint32_t phys_irq = entry->allocated_pirq;
	union source_id *virt_sid = &entry->virt_sid;

	if (virt_sid->intx_id.src == PTDEV_VPIN_IOAPIC) {
		uint64_t vdmask, pdmask, delmode, dest_mask, vector;
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
		vector = (uint64_t)irq_to_vector(phys_irq);
		rte.full &= ~(IOAPIC_RTE_DESTMOD | IOAPIC_RTE_DELMOD | IOAPIC_RTE_INTVEC);
		rte.full |= IOAPIC_RTE_DESTLOG | delmode | vector;

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
static struct ptirq_remapping_info *add_msix_remapping(struct acrn_vm *vm,
	uint16_t virt_bdf, uint16_t phys_bdf, uint32_t entry_nr)
{
	struct ptirq_remapping_info *entry;
	DEFINE_MSI_SID(phys_sid, phys_bdf, entry_nr);
	DEFINE_MSI_SID(virt_sid, virt_bdf, entry_nr);

	entry = ptirq_lookup_entry_by_sid(PTDEV_INTR_MSI, &phys_sid, NULL);
	if (entry == NULL) {
		if (ptirq_lookup_entry_by_sid(PTDEV_INTR_MSI, &virt_sid, vm) != NULL) {
			pr_err("MSIX re-add vbdf%x", virt_bdf);
		} else {
			entry = ptirq_alloc_entry(vm, PTDEV_INTR_MSI);
			if (entry != NULL) {
				entry->phys_sid.value = phys_sid.value;
				entry->virt_sid.value = virt_sid.value;
				/* update msi source and active entry */
				if (ptirq_activate_entry(entry, IRQ_INVALID) < 0) {
					ptirq_release_entry(entry);
					entry = NULL;
				}
			}
		}
	} else if (entry->vm != vm) {
		if (is_vm0(entry->vm)) {
			entry->vm = vm;
			entry->virt_sid.msi_id.bdf = virt_bdf;
		} else {
			pr_err("MSIX pbdf%x idx=%d already in vm%d with vbdf%x, not able to add into vm%d with vbdf%x",
				entry->phys_sid.msi_id.bdf, entry->phys_sid.msi_id.entry_nr, entry->vm->vm_id,
				entry->virt_sid.msi_id.bdf, vm->vm_id, virt_bdf);

			pr_err("msix entry pbdf%x idx%d already in vm%d", phys_bdf, entry_nr, entry->vm->vm_id);
			entry = NULL;
		}
	} else {
		/* The mapping has already been added to the VM. No action
		 * required. */
	}

	dev_dbg(ACRN_DBG_IRQ, "VM%d MSIX add vector mapping vbdf%x:pbdf%x idx=%d",
		vm->vm_id, virt_bdf, phys_bdf, entry_nr);

	return entry;
}

/* deactive & remove mapping entry of vbdf:entry_nr for vm */
static void
remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf, uint32_t entry_nr)
{
	struct ptirq_remapping_info *entry;
	DEFINE_MSI_SID(virt_sid, virt_bdf, entry_nr);

	entry = ptirq_lookup_entry_by_sid(PTDEV_INTR_MSI, &virt_sid, vm);
	if (entry != NULL) {
		if (is_entry_active(entry)) {
			/*TODO: disable MSIX device when HV can in future */
			ptirq_deactivate_entry(entry);
		}

		dev_dbg(ACRN_DBG_IRQ,
			"VM%d MSIX remove vector mapping vbdf-pbdf:0x%x-0x%x idx=%d",
			entry->vm->vm_id, virt_bdf,
			entry->phys_sid.msi_id.bdf, entry_nr);

		ptirq_release_entry(entry);
	}

}

/* add intx entry for a vm, based on intx id (phys_pin)
 * - if the entry not be added by any vm, allocate it
 * - if the entry already be added by vm0, then change the owner to current vm
 * - if the entry already be added by other vm, return NULL
 */
static struct ptirq_remapping_info *add_intx_remapping(struct acrn_vm *vm, uint8_t virt_pin,
		uint8_t phys_pin, bool pic_pin)
{
	struct ptirq_remapping_info *entry = NULL;
	uint8_t vpin_src = pic_pin ? PTDEV_VPIN_PIC : PTDEV_VPIN_IOAPIC;
	DEFINE_IOAPIC_SID(phys_sid, phys_pin, 0);
	DEFINE_IOAPIC_SID(virt_sid, virt_pin, vpin_src);
	uint32_t phys_irq = pin_to_irq(phys_pin);

	if ((!pic_pin && (virt_pin >= vioapic_pincount(vm))) || (pic_pin && (virt_pin >= vpic_pincount()))) {
		pr_err("ptirq_add_intx_remapping fails!\n");
	} else if (!irq_is_gsi(phys_irq)) {
		pr_err("%s, invalid phys_pin: %d <-> irq: 0x%x is not a GSI\n", __func__, phys_pin, phys_irq);
	} else {
		entry = ptirq_lookup_entry_by_sid(PTDEV_INTR_INTX, &phys_sid, NULL);
		if (entry == NULL) {
			if (ptirq_lookup_entry_by_vpin(vm, virt_pin, pic_pin) == NULL) {
				entry = ptirq_alloc_entry(vm, PTDEV_INTR_INTX);
				if (entry != NULL) {
					entry->phys_sid.value = phys_sid.value;
					entry->virt_sid.value = virt_sid.value;

					/* activate entry */
					if (ptirq_activate_entry(entry, phys_irq) < 0) {
						ptirq_release_entry(entry);
						entry = NULL;
					}
				}
			} else {
				pr_err("INTX re-add vpin %d", virt_pin);
			}
		} else if (entry->vm != vm) {
			if (is_vm0(entry->vm)) {
				entry->vm = vm;
				entry->virt_sid.value = virt_sid.value;
			} else {
				pr_err("INTX pin%d already in vm%d with vpin%d, not able to add into vm%d with vpin%d",
					phys_pin, entry->vm->vm_id, entry->virt_sid.intx_id.pin, vm->vm_id, virt_pin);
				entry = NULL;
			}
		} else {
			/* The mapping has already been added to the VM. No action
			 * required. */
		}

		if (entry != NULL) {
			if (pic_pin) {
				vm->arch_vm.vpic.vpin_to_pt_entry[virt_pin] = entry;
			} else {
				vm->arch_vm.vioapic.vpin_to_pt_entry[virt_pin] = entry;
			}
			dev_dbg(ACRN_DBG_IRQ, "VM%d INTX add pin mapping vpin%d:ppin%d",
				entry->vm->vm_id, virt_pin, phys_pin);
		}
	}

	return entry;
}

/* deactive & remove mapping entry of vpin for vm */
static void remove_intx_remapping(struct acrn_vm *vm, uint8_t virt_pin, bool pic_pin)
{
	uint32_t phys_irq;
	struct ptirq_remapping_info *entry;

	if ((!pic_pin && (virt_pin >= vioapic_pincount(vm))) || (pic_pin && (virt_pin >= vpic_pincount()))) {
		pr_err("virtual irq pin is invalid!\n");
	} else {
		entry = ptirq_lookup_entry_by_vpin(vm, virt_pin, pic_pin);
		if (entry != NULL) {
			if (is_entry_active(entry)) {
				phys_irq = entry->allocated_pirq;
				/* disable interrupt */
				gsi_mask_irq(phys_irq);

				ptirq_deactivate_entry(entry);
				dev_dbg(ACRN_DBG_IRQ,
					"deactive %s intx entry:ppin=%d, pirq=%d ",
					pic_pin ? "vPIC" : "vIOAPIC",
					entry->phys_sid.intx_id.pin, phys_irq);
				dev_dbg(ACRN_DBG_IRQ, "from vm%d vpin=%d\n",
					entry->vm->vm_id, virt_pin);
			}

			if (pic_pin) {
				vm->arch_vm.vpic.vpin_to_pt_entry[virt_pin] = NULL;
			} else {
				vm->arch_vm.vioapic.vpin_to_pt_entry[virt_pin] = NULL;
			}

			ptirq_release_entry(entry);
		}
	}
}

static void ptirq_handle_intx(struct acrn_vm *vm,
		const struct ptirq_remapping_info *entry)
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

void ptirq_softirq(uint16_t pcpu_id)
{
	struct acrn_vcpu *vcpu = (struct acrn_vcpu *)per_cpu(vcpu, pcpu_id);
	struct acrn_vm *vm = vcpu->vm;

	while (1) {
		struct ptirq_remapping_info *entry = ptirq_dequeue_softirq(vm);
		struct ptirq_msi_info *msi;

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
			ptirq_handle_intx(vm, entry);
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

void ptirq_intx_ack(struct acrn_vm *vm, uint8_t virt_pin,
		uint8_t vpin_src)
{
	uint32_t phys_irq;
	struct ptirq_remapping_info *entry;
	bool pic_pin = (vpin_src == PTDEV_VPIN_PIC);

	entry = ptirq_lookup_entry_by_vpin(vm, virt_pin, pic_pin);
	if (entry != NULL) {
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
}

/* Main entry for PCI device assignment with MSI and MSI-X
 * MSI can up to 8 vectors and MSI-X can up to 1024 Vectors
 * We use entry_nr to indicate coming vectors
 * entry_nr = 0 means first vector
 * user must provide bdf and entry_nr
 */
int32_t ptirq_msix_remap(struct acrn_vm *vm, uint16_t virt_bdf,
		uint16_t entry_nr, struct ptirq_msi_info *info)
{
	struct ptirq_remapping_info *entry;
	DEFINE_MSI_SID(virt_sid, virt_bdf, entry_nr);
	int32_t ret = -ENODEV;

	/*
	 * Device Model should pre-hold the mapping entries by calling
	 * ptirq_add_msix_remapping for UOS.
	 *
	 * For SOS(vm0), it adds the mapping entries at runtime, if the
	 * entry already be held by others, return error.
	 */
	spinlock_obtain(&ptdev_lock);
	entry = ptirq_lookup_entry_by_sid(PTDEV_INTR_MSI, &virt_sid, vm);
	if (entry == NULL) {
		/* VM0 we add mapping dynamically */
		if (is_vm0(vm)) {
			entry = add_msix_remapping(vm, virt_bdf, virt_bdf, entry_nr);
			if (entry == NULL) {
				pr_err("dev-assign: msi entry exist in others");
			}
		} else {
			/* ptirq_msix_remap is called by SOS on demand, if
			 * failed to find pre-hold mapping, return error to
			 * the caller.
			 */
			pr_err("dev-assign: msi entry not exist");
		}
	}
	spinlock_release(&ptdev_lock);

	if (entry != NULL) {
		if (is_entry_active(entry) && (info->vmsi_data == 0U)) {
			/* handle destroy case */
			info->pmsi_data = 0U;
		} else {
			/* build physical config MSI, update to info->pmsi_xxx */
			ptirq_build_physical_msi(vm, info, irq_to_vector(entry->allocated_pirq));
			entry->msi = *info;
			dev_dbg(ACRN_DBG_IRQ, "PCI %x:%x.%x MSI VR[%d] 0x%x->0x%x assigned to vm%d",
				pci_bus(virt_bdf), pci_slot(virt_bdf), pci_func(virt_bdf), entry_nr,
				info->vmsi_data & 0xFFU, irq_to_vector(entry->allocated_pirq), entry->vm->vm_id);
		}
		ret = 0;
	}

	return ret;
}

static void activate_physical_ioapic(struct acrn_vm *vm,
		struct ptirq_remapping_info *entry)
{
	union ioapic_rte rte;
	uint32_t phys_irq = entry->allocated_pirq;
	uint32_t intr_mask;
	bool is_lvl_trigger = false;

	/* disable interrupt */
	gsi_mask_irq(phys_irq);

	/* build physical IOAPIC RTE */
	rte = ptirq_build_physical_rte(vm, entry);
	intr_mask = (uint32_t)(rte.full & IOAPIC_RTE_INTMASK);

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
int32_t ptirq_intx_pin_remap(struct acrn_vm *vm, uint8_t virt_pin, uint8_t vpin_src)
{
	int32_t status = 0;
	struct ptirq_remapping_info *entry = NULL;
	bool need_switch_vpin_src = false;
	DEFINE_IOAPIC_SID(virt_sid, virt_pin, vpin_src);
	bool pic_pin = (vpin_src == PTDEV_VPIN_PIC);

	/*
	 * virt pin could come from vpic master, vpic slave or vioapic
	 * while phys pin is always means for physical IOAPIC.
	 *
	 * Device Model should pre-hold the mapping entries by calling
	 * ptirq_add_intx_remapping for UOS.
	 *
	 * For SOS(vm0), it adds the mapping entries at runtime, if the
	 * entry already be held by others, return error.
	 */

	/* no remap for hypervisor owned intx */
#ifdef CONFIG_COM_IRQ
	if (ptdev_hv_owned_intx(vm, &virt_sid)) {
		status = -ENODEV;
	}
#endif /* CONFIG_COM_IRQ */

	if (status || (pic_pin && (virt_pin >= NR_VPIC_PINS_TOTAL))) {
		status = -EINVAL;
	} else {
		/* query if we have virt to phys mapping */
		spinlock_obtain(&ptdev_lock);
		entry = ptirq_lookup_entry_by_vpin(vm, virt_pin, pic_pin);
		if (entry == NULL) {
			if (is_vm0(vm)) {

				/* for vm0, there is chance of vpin source switch
				 * between vPIC & vIOAPIC for one legacy phys_pin.
				 *
				 * here checks if there is already mapping entry from
				 * the other vpin source for legacy pin. If yes, then
				 * switch vpin source is needed
				 */
				if (virt_pin < NR_LEGACY_PIN) {
					uint8_t vpin = pic_ioapic_pin_map[virt_pin];

					entry = ptirq_lookup_entry_by_vpin(vm, vpin, !pic_pin);
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
						status = -ENODEV;
					}
				}
			} else {
				/* ptirq_intx_pin_remap is triggered by vPIC/vIOAPIC
				 * everytime a pin get unmask, here filter out pins
				 * not get mapped.
				 */
				status = -ENODEV;
			}
		}
		spinlock_release(&ptdev_lock);
	}

	if (!status) {
		spinlock_obtain(&ptdev_lock);
		/* if vpin source need switch */
		if ((need_switch_vpin_src) && (entry != NULL)) {
			dev_dbg(ACRN_DBG_IRQ,
				"IOAPIC pin=%hhu pirq=%u vpin=%d switch from %s to %s vpin=%d for vm%d",
				entry->phys_sid.intx_id.pin,
				entry->allocated_pirq, entry->virt_sid.intx_id.pin,
				(vpin_src == 0U) ? "vPIC" : "vIOAPIC",
				(vpin_src == 0U) ? "vIOPIC" : "vPIC",
				virt_pin, entry->vm->vm_id);
			entry->virt_sid.value = virt_sid.value;
		}
		spinlock_release(&ptdev_lock);
		activate_physical_ioapic(vm, entry);
	}

	return status;
}

/* @pre vm != NULL
 * except vm0, Device Model should call this function to pre-hold ptdev intx
 * entries:
 * - the entry is identified by phys_pin:
 *   one entry vs. one phys_pin
 * - currently, one phys_pin can only be held by one pin source (vPIC or
 *   vIOAPIC)
 */
int32_t ptirq_add_intx_remapping(struct acrn_vm *vm, uint8_t virt_pin, uint8_t phys_pin,
				bool pic_pin)
{
	struct ptirq_remapping_info *entry;

	spinlock_obtain(&ptdev_lock);
	entry = add_intx_remapping(vm, virt_pin, phys_pin, pic_pin);
	spinlock_release(&ptdev_lock);

	return (entry != NULL) ? 0 : -ENODEV;
}

/*
 * @pre vm != NULL
 */
void ptirq_remove_intx_remapping(struct acrn_vm *vm, uint8_t virt_pin, bool pic_pin)
{
	spinlock_obtain(&ptdev_lock);
	remove_intx_remapping(vm, virt_pin, pic_pin);
	spinlock_release(&ptdev_lock);
}

/* except vm0, Device Model should call this function to pre-hold ptdev msi
 * entries:
 * - the entry is identified by phys_bdf:msi_idx:
 *   one entry vs. one phys_bdf:msi_idx
 */
int32_t ptirq_add_msix_remapping(struct acrn_vm *vm, uint16_t virt_bdf,
		uint16_t phys_bdf, uint32_t vector_count)
{
	struct ptirq_remapping_info *entry;
	uint32_t i;
	uint32_t vector_added = 0U;

	for (i = 0U; i < vector_count; i++) {
		spinlock_obtain(&ptdev_lock);
		entry = add_msix_remapping(vm, virt_bdf, phys_bdf, i);
		spinlock_release(&ptdev_lock);
		if (entry == NULL) {
			break;
		}
		vector_added++;
	}

	if (vector_added != vector_count) {
		ptirq_remove_msix_remapping(vm, virt_bdf, vector_added);
	}

	return (vector_added == vector_count) ? 0 : -ENODEV;
}

/*
 * @pre vm != NULL
 */
void ptirq_remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf,
		uint32_t vector_count)
{
	uint32_t i;

	for (i = 0U; i < vector_count; i++) {
		spinlock_obtain(&ptdev_lock);
		remove_msix_remapping(vm, virt_bdf, i);
		spinlock_release(&ptdev_lock);
	}
}
