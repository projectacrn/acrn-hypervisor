/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/guest/vm.h>
#include <ptdev.h>
#include <x86/guest/assign.h>
#include <vpci.h>
#include <x86/vtd.h>
#include <x86/board.h>
#include "vpci_priv.h"

#define PER_VECTOR_MASK_CAP 0x0100U

/* Pre-assumptions for vMSI-x on MSI emulation:
 * 1. The device is in vmsix_on_msi_devs array.
 * 2. The device should support MSI capability as well as per-vector mask
 * 3. The device doesn't support MSI-x capability.
 * 4. The device should have an unused BAR (this condition is checked inside init_vmsix_on_msi).
 * 5. HV doesn't emulate PBA according to physcial device status, the device driver should not rely on PBA
 *    for functionality.
 */
static bool need_vmsix_on_msi_emulation(__unused struct pci_pdev *pdev, __unused uint16_t *vector_count)
{
	bool ret = false;
#if (MAX_VMSIX_ON_MSI_PDEVS_NUM > 0)
	uint16_t msgctrl;
	uint32_t i;

	for(i = 0U; i < MAX_VMSIX_ON_MSI_PDEVS_NUM; i++) {
		if (pdev->bdf.value == vmsix_on_msi_devs[i].bdf.value) {
			if ((pdev->msi_capoff != 0U) && (pdev->msix.capoff == 0U)) {
				msgctrl = (uint16_t)pci_pdev_read_cfg(pdev->bdf, pdev->msi_capoff + PCIR_MSI_CTRL, 2U);
				*vector_count = 1U << ((msgctrl & PCIM_MSICTRL_MMC_MASK) >> 1U);
				if ((*vector_count > 1U) && ((msgctrl & PER_VECTOR_MASK_CAP) != 0U)) {
					ret = true;
				}
			}
			break;
		}
	}
#endif

	return ret;
}

void reserve_vmsix_on_msi_irtes(struct pci_pdev *pdev)
{
	struct intr_source intr_src;
	uint16_t count = 0;
	int32_t ret;

	if (need_vmsix_on_msi_emulation(pdev, &count)) {
		intr_src.is_msi = true;
		intr_src.src.msi.value = pdev->bdf.value;
		ret = dmar_reserve_irte(&intr_src, count, &pdev->irte_start);
		if ((ret == 0) && (pdev->irte_start != INVALID_IRTE_ID)) {
			pdev->irte_count = count;
		}
	}
}

static inline uint32_t get_mask_bits_offset(const struct pci_vdev *vdev)
{
	return vdev->msi.is_64bit ? (vdev->msix.capoff + 0x10U) : (vdev->msix.capoff + 0xCU);
}

/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void init_vmsix_on_msi(struct pci_vdev *vdev)
{
	struct pci_pdev *pdev = vdev->pdev;
	uint32_t i;

	/* irte_count > 1 only when the device needs vMSI-x on MSI emulation and IRTEs are reserved successfully */
	if (pdev->irte_count > 1U) {
		/* find an unused BAR */
		for (i = 0U; i < vdev->nr_bars; i++) {
			if (vdev->vbars[i].base_hpa == 0UL){
				break;
			}
			if (is_pci_mem64lo_bar(&vdev->vbars[i])) {
				i++;
			}
		}
		if (i < vdev->nr_bars) {
			vdev->msix.capoff = pdev->msi_capoff;
			vdev->msi.capoff = 0U;
			vdev->msix.is_vmsix_on_msi = true;
			/* For a device support MSI with per-vector mask, the length of MSI cap is at least 20 bytes */
			vdev->msix.caplen = MSIX_CAPLEN;
			vdev->msix.table_bar = i;
			vdev->msix.table_offset = 0U;
			vdev->msix.table_count = pdev->irte_count;

			/* capability ID */
			pci_vdev_write_vcfg(vdev, vdev->msix.capoff, 1U, 0x11U);
			/* message control, MSI-X Diabled, Function unamsked */
			pci_vdev_write_vcfg(vdev, vdev->msix.capoff + 2U, 2U, pdev->irte_count - 1U);
			/* Init MSIX table vBAR, offset is 0 */
			pci_vdev_write_vcfg(vdev, vdev->msix.capoff + 4U, 4U, i);
			/* Init PBA table vBAR, offset is 2048 */
			pci_vdev_write_vcfg(vdev, vdev->msix.capoff + 8U, 4U, 2048U + i);

			vdev->vbars[i].size = 4096U;
			vdev->vbars[i].base_hpa = 0x0UL;
			vdev->vbars[i].mask = 0xFFFFF000U & PCI_BASE_ADDRESS_MEM_MASK;
			/* fixed for memory, 32bit, non-prefetchable */
			vdev->vbars[i].bar_type.bits = PCIM_BAR_MEM_32;

			/* About MSI-x bar GPA:
			 * - For Service VM: when first time init, it is programmed as 0, then OS will program
			 *   the value later.
			 * - For Post-launched VM: The GPA is assigned by device model.
			 * - For Pre-launched VM: The GPA is assigned by acrn-config tool.
			 */
			if (is_prelaunched_vm(vpci2vm(vdev->vpci))) {
				vdev->vbars[i].base_gpa = vdev->pci_dev_config->vbar_base[i];
				pci_vdev_write_vbar(vdev, i, (uint32_t)vdev->vbars[i].base_gpa);
			}
		}
	}
}

void write_vmsix_cap_reg_on_msi(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	uint16_t old_msgctrl, msgctrl;
	uint16_t msi_msgctrl;

	old_msgctrl = (uint16_t)pci_vdev_read_vcfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U);
	/* Write to vdev */
	pci_vdev_write_vcfg(vdev, offset, bytes, val);
	msgctrl = (uint16_t)pci_vdev_read_vcfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U);

	if (((old_msgctrl ^ msgctrl) & (PCIM_MSIXCTRL_MSIX_ENABLE | PCIM_MSIXCTRL_FUNCTION_MASK)) != 0U) {
		msi_msgctrl = (uint16_t)pci_pdev_read_cfg(vdev->pdev->bdf, offset, 2U);

		msi_msgctrl = msi_msgctrl & (~PCIM_MSICTRL_MME_MASK);
		msi_msgctrl &= ~ PCIM_MSICTRL_MSI_ENABLE;

		/* If MSI Enable is being set, make sure INTxDIS bit is set */
		if ((msgctrl & PCIM_MSIXCTRL_MSIX_ENABLE) != 0U) {
			enable_disable_pci_intx(vdev->pdev->bdf, false);
			msi_msgctrl |= (msi_msgctrl & PCIM_MSICTRL_MMC_MASK) << 3U;
			msi_msgctrl |= PCIM_MSICTRL_MSI_ENABLE;
		}
		pci_pdev_write_cfg(vdev->pdev->bdf, offset, 2U, msi_msgctrl);

		if ((msgctrl & PCIM_MSIXCTRL_FUNCTION_MASK) != 0U) {
			pci_pdev_write_cfg(vdev->pdev->bdf, get_mask_bits_offset(vdev), 4U, 0xFFFFFFFFU);
		}
	}
}

void remap_one_vmsix_entry_on_msi(struct pci_vdev *vdev, uint32_t index)
{
	const struct msix_table_entry *ventry;
	uint32_t mask_bits;
	uint32_t vector_mask = 1U << index;
	struct msi_info info = {};
	union pci_bdf pbdf = vdev->pdev->bdf;
	union irte_index ir_index;
	int32_t ret = 0;
	uint32_t capoff = vdev->msix.capoff;

	mask_bits = pci_pdev_read_cfg(pbdf, get_mask_bits_offset(vdev), 4U);
	mask_bits |= vector_mask;
	pci_pdev_write_cfg(pbdf, get_mask_bits_offset(vdev), 4U, mask_bits);

	ventry = &vdev->msix.table_entries[index];
	if ((ventry->vector_control & PCIM_MSIX_VCTRL_MASK) == 0U) {
		info.addr.full = vdev->msix.table_entries[index].addr;
		info.data.full = vdev->msix.table_entries[index].data;

		ret = ptirq_prepare_msix_remap(vpci2vm(vdev->vpci), vdev->bdf.value, pbdf.value,
			(uint16_t)index, &info, vdev->pdev->irte_start + (uint16_t)index);
		if (ret == 0) {
			if (!vdev->msix.is_vmsix_on_msi_programmed) {
				ir_index.index = vdev->pdev->irte_start;
				info.addr.ir_bits.shv = 1U;
				info.addr.ir_bits.intr_index_high = ir_index.bits.index_high;
				info.addr.ir_bits.intr_index_low = ir_index.bits.index_low;
				pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_ADDR, 0x4U, (uint32_t)info.addr.full);
				if (vdev->msi.is_64bit) {
					pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_ADDR_HIGH, 0x4U,
							(uint32_t)(info.addr.full >> 32U));
					pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_DATA_64BIT, 0x2U,
							(uint16_t)info.data.full);
				} else {
					pci_pdev_write_cfg(pbdf, capoff + PCIR_MSI_DATA, 0x2U,
							(uint16_t)info.data.full);
				}
				vdev->msix.is_vmsix_on_msi_programmed = true;
			}
			mask_bits &= ~vector_mask;
		}
	}
	pci_pdev_write_cfg(pbdf, get_mask_bits_offset(vdev), 4U, mask_bits);
}
