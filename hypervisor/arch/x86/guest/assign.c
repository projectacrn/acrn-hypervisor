/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <vm.h>
#include <vtd.h>
#include <per_cpu.h>
#include <ioapic.h>

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
		const union source_id *sid, const struct acrn_vm *vm)
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
ptirq_lookup_entry_by_vpin(const struct acrn_vm *vm, uint32_t virt_pin, bool pic_pin)
{
	struct ptirq_remapping_info *entry;

	if (pic_pin) {
		entry = vm->arch_vm.vpic.vpin_to_pt_entry[virt_pin];
	} else {
		entry = vm->arch_vm.vioapic.vpin_to_pt_entry[virt_pin];
	}
	return entry;
}

static uint32_t calculate_logical_dest_mask(uint64_t pdmask)
{
	uint32_t dest_mask = 0UL;
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
		const struct ptirq_remapping_info *entry, uint32_t vector)
{
	uint64_t vdmask, pdmask;
	uint32_t dest, delmode, dest_mask;
	bool phys;
	union dmar_ir_entry irte;
	union irte_index ir_index;
	int32_t ret;
	struct intr_source intr_src;

	/* get physical destination cpu mask */
	dest = info->vmsi_addr.bits.dest_field;
	phys = (info->vmsi_addr.bits.dest_mode == MSI_ADDR_DESTMODE_PHYS);

	vlapic_calc_dest(vm, &vdmask, dest, phys, false);
	pdmask = vcpumask2pcpumask(vm, vdmask);

	/* get physical delivery mode */
	delmode = info->vmsi_data.bits.delivery_mode;
	if ((delmode != MSI_DATA_DELMODE_FIXED) && (delmode != MSI_DATA_DELMODE_LOPRI)) {
		delmode = MSI_DATA_DELMODE_LOPRI;
	}

	dest_mask = calculate_logical_dest_mask(pdmask);

	/* Using phys_irq as index in the corresponding IOMMU */
	irte.entry.lower = 0UL;
	irte.entry.upper = 0UL;
	irte.bits.vector = vector;
	irte.bits.delivery_mode = delmode;
	irte.bits.dest_mode = MSI_ADDR_DESTMODE_LOGICAL;
	irte.bits.rh = MSI_ADDR_RH;
	irte.bits.dest = dest_mask;

	intr_src.is_msi = true;
	intr_src.src.msi.value = entry->phys_sid.msi_id.bdf;
	ret = dmar_assign_irte(intr_src, irte, (uint16_t)entry->allocated_pirq);

	if (ret == 0) {
		/*
		 * Update the MSI interrupt source to point to the IRTE
		 * SHV is set to 0 as ACRN disables MMC (Multi-Message Capable
		 * for MSI devices.
		 */
		info->pmsi_data.full = 0U;
		ir_index.index = (uint16_t)entry->allocated_pirq;

		info->pmsi_addr.full = 0UL;
		info->pmsi_addr.ir_bits.intr_index_high = ir_index.bits.index_high;
		info->pmsi_addr.ir_bits.shv = 0U;
		info->pmsi_addr.ir_bits.intr_format = 0x1U;
		info->pmsi_addr.ir_bits.intr_index_low = ir_index.bits.index_low;
		info->pmsi_addr.ir_bits.constant = 0xFEEU;
	} else {
		/* In case there is no corresponding IOMMU, for example, if the
		 * IOMMU is ignored, pass the MSI info in Compatibility Format
		 */
		info->pmsi_data = info->vmsi_data;
		info->pmsi_data.bits.delivery_mode = delmode;
		info->pmsi_data.bits.vector = vector;

		info->pmsi_addr = info->vmsi_addr;
		info->pmsi_addr.bits.dest_field = dest_mask;
		info->pmsi_addr.bits.rh = MSI_ADDR_RH;
		info->pmsi_addr.bits.dest_mode = MSI_ADDR_DESTMODE_LOGICAL;
	}
	dev_dbg(ACRN_DBG_IRQ, "MSI %s addr:data = 0x%llx:%x(V) -> 0x%llx:%x(P)",
		(info->pmsi_addr.ir_bits.intr_format != 0U) ? " Remappable Format" : "Compatibility Format",
		info->vmsi_addr.full, info->vmsi_data.full,
		info->pmsi_addr.full, info->pmsi_data.full);
}

static union ioapic_rte
ptirq_build_physical_rte(struct acrn_vm *vm, struct ptirq_remapping_info *entry)
{
	union ioapic_rte rte;
	uint32_t phys_irq = entry->allocated_pirq;
	union source_id *virt_sid = &entry->virt_sid;
	union irte_index ir_index;
	union dmar_ir_entry irte;
	struct intr_source intr_src;
	int32_t ret;

	if (virt_sid->intx_id.src == PTDEV_VPIN_IOAPIC) {
		uint64_t vdmask, pdmask;
		uint32_t dest, delmode, dest_mask, vector;
		union ioapic_rte virt_rte;
		bool phys;

		vioapic_get_rte(vm, virt_sid->intx_id.pin, &virt_rte);
		rte = virt_rte;

		/* init polarity & pin state */
		if (rte.bits.intr_polarity == IOAPIC_RTE_INTPOL_ALO) {
			if (entry->polarity == 0U) {
				vioapic_set_irqline_nolock(vm, virt_sid->intx_id.pin, GSI_SET_HIGH);
			}
			entry->polarity = 1U;
		} else {
			if (entry->polarity == 1U) {
				vioapic_set_irqline_nolock(vm, virt_sid->intx_id.pin, GSI_SET_LOW);
			}
			entry->polarity = 0U;
		}

		/* physical destination cpu mask */
		phys = (virt_rte.bits.dest_mode == IOAPIC_RTE_DESTMODE_PHY);
		dest = (uint32_t)virt_rte.bits.dest_field;
		vlapic_calc_dest(vm, &vdmask, dest, phys, false);
		pdmask = vcpumask2pcpumask(vm, vdmask);

		/* physical delivery mode */
		delmode = virt_rte.bits.delivery_mode;
		if ((delmode != IOAPIC_RTE_DELMODE_FIXED) &&
			(delmode != IOAPIC_RTE_DELMODE_LOPRI)) {
			delmode = IOAPIC_RTE_DELMODE_LOPRI;
		}

		/* update physical delivery mode, dest mode(logical) & vector */
		vector = irq_to_vector(phys_irq);
		dest_mask = calculate_logical_dest_mask(pdmask);

		irte.entry.lower = 0UL;
		irte.entry.upper = 0UL;
		irte.bits.vector = vector;
		irte.bits.delivery_mode = delmode;
		irte.bits.dest_mode = IOAPIC_RTE_DESTMODE_LOGICAL;
		irte.bits.dest = dest_mask;
		irte.bits.trigger_mode = rte.bits.trigger_mode;

		intr_src.is_msi = false;
		intr_src.src.ioapic_id = ioapic_irq_to_ioapic_id(phys_irq);
		ret = dmar_assign_irte(intr_src, irte, (uint16_t)phys_irq);

		if (ret == 0) {
			ir_index.index = (uint16_t)phys_irq;
			rte.ir_bits.vector = vector;
			rte.ir_bits.constant = 0U;
			rte.ir_bits.intr_index_high = ir_index.bits.index_high;
			rte.ir_bits.intr_format = 1U;
			rte.ir_bits.intr_index_low = ir_index.bits.index_low;
		} else {
			rte.bits.dest_mode = IOAPIC_RTE_DESTMODE_LOGICAL;
			rte.bits.delivery_mode = delmode;
			rte.bits.vector = vector;
			rte.bits.dest_field = dest_mask;
		}

		dev_dbg(ACRN_DBG_IRQ, "IOAPIC RTE %s = 0x%x:%x(V) -> 0x%x:%x(P)",
			(rte.ir_bits.intr_format != 0U) ? "Remappable Format" : "Compatibility Format",
			virt_rte.u.hi_32, virt_rte.u.lo_32,
			rte.u.hi_32, rte.u.lo_32);
	} else {
		enum vpic_trigger trigger;
		union ioapic_rte phys_rte;

		/* just update trigger mode */
		ioapic_get_rte(phys_irq, &phys_rte);
		rte = phys_rte;
		rte.bits.trigger_mode = IOAPIC_RTE_TRGRMODE_EDGE;
		vpic_get_irqline_trigger_mode(vm, (uint32_t)virt_sid->intx_id.pin, &trigger);
		if (trigger == LEVEL_TRIGGER) {
			rte.bits.trigger_mode = IOAPIC_RTE_TRGRMODE_LEVEL;
		}

		dev_dbg(ACRN_DBG_IRQ, "IOAPIC RTE %s = 0x%x:%x(P) -> 0x%x:%x(P)",
			(rte.ir_bits.intr_format != 0U) ? "Remappable Format" : "Compatibility Format",
			phys_rte.u.hi_32, phys_rte.u.lo_32,
			rte.u.hi_32, rte.u.lo_32);
	}

	return rte;
}

/* add msix entry for a vm, based on msi id (phys_bdf+msix_index)
 * - if the entry not be added by any vm, allocate it
 * - if the entry already be added by sos_vm, then change the owner to current vm
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
		if (is_sos_vm(entry->vm)) {
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
	struct intr_source intr_src;

	entry = ptirq_lookup_entry_by_sid(PTDEV_INTR_MSI, &virt_sid, vm);
	if (entry != NULL) {
		if (is_entry_active(entry)) {
			/*TODO: disable MSIX device when HV can in future */
			ptirq_deactivate_entry(entry);
		}

		intr_src.is_msi = true;
		intr_src.src.msi.value = entry->phys_sid.msi_id.bdf;
		dmar_free_irte(intr_src, (uint16_t)entry->allocated_pirq);

		dev_dbg(ACRN_DBG_IRQ,
			"VM%d MSIX remove vector mapping vbdf-pbdf:0x%x-0x%x idx=%d",
			entry->vm->vm_id, virt_bdf,
			entry->phys_sid.msi_id.bdf, entry_nr);

		ptirq_release_entry(entry);
	}

}

/* add intx entry for a vm, based on intx id (phys_pin)
 * - if the entry not be added by any vm, allocate it
 * - if the entry already be added by sos_vm, then change the owner to current vm
 * - if the entry already be added by other vm, return NULL
 */
static struct ptirq_remapping_info *add_intx_remapping(struct acrn_vm *vm, uint32_t virt_pin,
		uint32_t phys_pin, bool pic_pin)
{
	struct ptirq_remapping_info *entry = NULL;
	uint32_t vpin_src = pic_pin ? PTDEV_VPIN_PIC : PTDEV_VPIN_IOAPIC;
	DEFINE_IOAPIC_SID(phys_sid, phys_pin, 0U);
	DEFINE_IOAPIC_SID(virt_sid, virt_pin, vpin_src);
	uint32_t phys_irq = ioapic_pin_to_irq(phys_pin);

	if (((!pic_pin) && (virt_pin >= vioapic_pincount(vm))) || (pic_pin && (virt_pin >= vpic_pincount()))) {
		pr_err("ptirq_add_intx_remapping fails!\n");
	} else if (!ioapic_irq_is_gsi(phys_irq)) {
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
			if (is_sos_vm(entry->vm)) {
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
static void remove_intx_remapping(struct acrn_vm *vm, uint32_t virt_pin, bool pic_pin)
{
	uint32_t phys_irq;
	struct ptirq_remapping_info *entry;
	struct intr_source intr_src;

	if (((!pic_pin) && (virt_pin >= vioapic_pincount(vm))) || (pic_pin && (virt_pin >= vpic_pincount()))) {
		pr_err("virtual irq pin is invalid!\n");
	} else {
		entry = ptirq_lookup_entry_by_vpin(vm, virt_pin, pic_pin);
		if (entry != NULL) {
			if (is_entry_active(entry)) {
				phys_irq = entry->allocated_pirq;
				/* disable interrupt */
				ioapic_gsi_mask_irq(phys_irq);

				ptirq_deactivate_entry(entry);
				intr_src.is_msi = false;
				intr_src.src.ioapic_id = ioapic_irq_to_ioapic_id(phys_irq);

				dmar_free_irte(intr_src, (uint16_t)phys_irq);
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
		vioapic_get_rte(vm, (uint32_t)virt_sid->intx_id.pin, &rte);
		if (rte.bits.trigger_mode == IOAPIC_RTE_TRGRMODE_LEVEL) {
			trigger_lvl = true;
		}

		if (trigger_lvl) {
			if (entry->polarity != 0U) {
				vioapic_set_irqline_lock(vm, virt_sid->intx_id.pin, GSI_SET_LOW);
			} else {
				vioapic_set_irqline_lock(vm, virt_sid->intx_id.pin, GSI_SET_HIGH);
			}
		} else {
			if (entry->polarity != 0U) {
				vioapic_set_irqline_lock(vm, virt_sid->intx_id.pin, GSI_FALLING_PULSE);
			} else {
				vioapic_set_irqline_lock(vm, virt_sid->intx_id.pin, GSI_RAISING_PULSE);
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
		vpic_get_irqline_trigger_mode(vm, virt_sid->intx_id.pin, &trigger);
		if (trigger == LEVEL_TRIGGER) {
			vpic_set_irqline(vm, virt_sid->intx_id.pin, GSI_SET_HIGH);
		} else {
			vpic_set_irqline(vm, virt_sid->intx_id.pin, GSI_RAISING_PULSE);
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
			if (msi != NULL) {
				/* TODO: msi destmode check required */
				(void)vlapic_intr_msi(vm, msi->vmsi_addr.full, msi->vmsi_data.full);
				dev_dbg(ACRN_DBG_PTIRQ, "dev-assign: irq=0x%x MSI VR: 0x%x-0x%x",
					entry->allocated_pirq,
					msi->vmsi_data.bits.vector,
					irq_to_vector(entry->allocated_pirq));
				dev_dbg(ACRN_DBG_PTIRQ, " vmsi_addr: 0x%llx vmsi_data: 0x%x",
					msi->vmsi_addr.full,
					msi->vmsi_data.full);
			}
		}
	}
}

void ptirq_intx_ack(struct acrn_vm *vm, uint32_t virt_pin, uint32_t vpin_src)
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
				vioapic_set_irqline_lock(vm, virt_pin, GSI_SET_HIGH);
			} else {
				vioapic_set_irqline_lock(vm, virt_pin, GSI_SET_LOW);
			}
			break;
		case PTDEV_VPIN_PIC:
			vpic_set_irqline(vm, virt_pin, GSI_SET_LOW);
			break;
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
		ioapic_gsi_unmask_irq(phys_irq);
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
	 * For SOS(sos_vm), it adds the mapping entries at runtime, if the
	 * entry already be held by others, return error.
	 */
	spinlock_obtain(&ptdev_lock);
	entry = ptirq_lookup_entry_by_sid(PTDEV_INTR_MSI, &virt_sid, vm);
	if (entry == NULL) {
		/* SOS_VM we add mapping dynamically */
		if (is_sos_vm(vm)) {
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
		if (is_entry_active(entry) && (info->vmsi_data.full == 0U)) {
			/* handle destroy case */
			info->pmsi_data.full = 0U;
		} else {
			/* build physical config MSI, update to info->pmsi_xxx */
			if (is_lapic_pt(vm)) {
				/* for vm with lapic-pt, keep vector from guest */
				ptirq_build_physical_msi(vm, info, entry, (uint32_t)info->vmsi_data.bits.vector);
			} else {
				ptirq_build_physical_msi(vm, info, entry, irq_to_vector(entry->allocated_pirq));
			}

			entry->msi = *info;
			dev_dbg(ACRN_DBG_IRQ, "PCI %x:%x.%x MSI VR[%d] 0x%x->0x%x assigned to vm%d",
				pci_bus(virt_bdf), pci_slot(virt_bdf), pci_func(virt_bdf), entry_nr,
				info->vmsi_data.bits.vector, irq_to_vector(entry->allocated_pirq), entry->vm->vm_id);
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
	uint64_t intr_mask;
	bool is_lvl_trigger = false;

	/* disable interrupt */
	ioapic_gsi_mask_irq(phys_irq);

	/* build physical IOAPIC RTE */
	rte = ptirq_build_physical_rte(vm, entry);
	intr_mask = rte.bits.intr_mask;

	/* update irq trigger mode according to info in guest */
	if (rte.bits.trigger_mode == IOAPIC_RTE_TRGRMODE_LEVEL) {
		is_lvl_trigger = true;
	}
	set_irq_trigger_mode(phys_irq, is_lvl_trigger);

	/* set rte entry when masked */
	rte.bits.intr_mask = IOAPIC_RTE_MASK_SET;
	ioapic_set_rte(phys_irq, rte);

	if (intr_mask == IOAPIC_RTE_MASK_CLR) {
		ioapic_gsi_unmask_irq(phys_irq);
	}
}

/* Main entry for PCI/Legacy device assignment with INTx, calling from vIOAPIC
 * or vPIC
 */
int32_t ptirq_intx_pin_remap(struct acrn_vm *vm, uint32_t virt_pin, uint32_t vpin_src)
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
	 * For SOS(sos_vm), it adds the mapping entries at runtime, if the
	 * entry already be held by others, return error.
	 */

	/* no remap for hypervisor owned intx */
	if (is_sos_vm(vm) && hv_used_dbg_intx(virt_sid.intx_id.pin)) {
		status = -ENODEV;
	}

	if ((status != 0) || (pic_pin && (virt_pin >= NR_VPIC_PINS_TOTAL))) {
		status = -EINVAL;
	} else {
		/* query if we have virt to phys mapping */
		spinlock_obtain(&ptdev_lock);
		entry = ptirq_lookup_entry_by_vpin(vm, virt_pin, pic_pin);
		if (entry == NULL) {
			if (is_sos_vm(vm)) {

				/* for sos_vm, there is chance of vpin source switch
				 * between vPIC & vIOAPIC for one legacy phys_pin.
				 *
				 * here checks if there is already mapping entry from
				 * the other vpin source for legacy pin. If yes, then
				 * switch vpin source is needed
				 */
				if (virt_pin < NR_LEGACY_PIN) {
					uint32_t vpin = get_pic_pin_from_ioapic_pin(virt_pin);

					entry = ptirq_lookup_entry_by_vpin(vm, vpin, !pic_pin);
					if (entry != NULL) {
						need_switch_vpin_src = true;
					}
				}

				/* entry could be updated by above switch check */
				if (entry == NULL) {
					uint32_t phys_pin = virt_pin;

					/* fix vPIC pin to correct native IOAPIC pin */
					if (pic_pin) {
						phys_pin = get_pic_pin_from_ioapic_pin(virt_pin);
					}
					entry = add_intx_remapping(vm, virt_pin, phys_pin, pic_pin);
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

	if (status == 0) {
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
 * except sos_vm, Device Model should call this function to pre-hold ptdev intx
 * entries:
 * - the entry is identified by phys_pin:
 *   one entry vs. one phys_pin
 * - currently, one phys_pin can only be held by one pin source (vPIC or
 *   vIOAPIC)
 */
int32_t ptirq_add_intx_remapping(struct acrn_vm *vm, uint32_t virt_pin, uint32_t phys_pin, bool pic_pin)
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
void ptirq_remove_intx_remapping(struct acrn_vm *vm, uint32_t virt_pin, bool pic_pin)
{
	spinlock_obtain(&ptdev_lock);
	remove_intx_remapping(vm, virt_pin, pic_pin);
	spinlock_release(&ptdev_lock);
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

/* except sos_vm, Device Model should call this function to pre-hold ptdev msi
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
