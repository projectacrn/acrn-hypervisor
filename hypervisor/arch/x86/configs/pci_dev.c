/*
 * Copyright (C) 2019-2020 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <pci.h>
#include <pci_dev.h>
#include <vpci.h>
#include <logmsg.h>

/*
 * @pre pdev != NULL;
 */
static bool is_allocated_to_prelaunched_vm(struct pci_pdev *pdev)
{
	bool found = false;
	uint16_t vmid;
	uint32_t pci_idx;
	struct acrn_vm_config *vm_config;
	struct acrn_vm_pci_dev_config *dev_config;

	for (vmid = 0U; (vmid < CONFIG_MAX_VM_NUM) && !found; vmid++) {
		vm_config = get_vm_config(vmid);
		if (vm_config->load_order == PRE_LAUNCHED_VM) {
			for (pci_idx = 0U; pci_idx < vm_config->pci_dev_num; pci_idx++) {
				dev_config = &vm_config->pci_devs[pci_idx];
				if ((dev_config->emu_type == PCI_DEV_TYPE_PTDEV) &&
						bdf_is_equal(dev_config->pbdf, pdev->bdf)) {
					dev_config->pdev = pdev;
					found = true;
					break;
				}
			}
		}
	}

	return found;
}

static bool is_pbus_allocated_to_prelaunched_vm(uint8_t bus)
{
	bool found = false;
	uint16_t vmid;
	uint32_t pci_idx;
	struct acrn_vm_config *vm_config;
	struct acrn_vm_pci_dev_config *dev_config;

	for (vmid = 0U; (vmid < CONFIG_MAX_VM_NUM) && !found; vmid++) {
		vm_config = get_vm_config(vmid);
		if (vm_config->load_order == PRE_LAUNCHED_VM) {
			for (pci_idx = 0U; pci_idx < vm_config->pci_dev_num; pci_idx++) {
				dev_config = &vm_config->pci_devs[pci_idx];
				if ((dev_config->emu_type == PCI_DEV_TYPE_PTDEV) &&
						(dev_config->pbdf.bits.b == bus)) {
					found = true;
					break;
				}
			}
		}
	}

	return found;
}

static bool need_hide_from_sos(struct pci_pdev *pdev)
{
	const union pci_bdf bdf = {.bits.b = 0U, .bits.d = 0x1cU, .bits.f = 0U};
	uint8_t hdr_type, hdr_layout, sec_busno;
	uint32_t vid, pid;
	bool ret = false;

	if (bdf_is_equal(bdf, pdev->bdf)) {
		vid = pci_pdev_read_cfg(pdev->bdf, PCIR_VENDOR, 2U);
		pid = pci_pdev_read_cfg(pdev->bdf, PCIR_DEVICE, 2U);
		hdr_type = (uint8_t)pci_pdev_read_cfg(pdev->bdf, PCIR_HDRTYPE, 1U);
		hdr_layout = (hdr_type & PCIM_HDRTYPE);

		if ((vid == 0x8086) && (pid == 0xa0bc) && (hdr_layout == PCIM_HDRTYPE_BRIDGE)) {
			sec_busno = (uint8_t)pci_pdev_read_cfg(pdev->bdf, PCIR_SECBUS_1, 1U);
			ret = is_pbus_allocated_to_prelaunched_vm(sec_busno);
			if (ret) {
				pr_acrnlog("%s: [%x:%x.%x] is hidden from SOS",
					__func__, bdf.bits.b, bdf.bits.d, bdf.bits.f);
			}
		}
	}

	return ret;
}

/*
 * @brief Initialize a acrn_vm_pci_dev_config structure
 *
 * Initialize a acrn_vm_pci_dev_config structure with a specified the pdev structure.
 * A acrn_vm_pci_dev_config is used to store a PCI device configuration for a VM. The
 * caller of the function init_one_dev_config should guarantee execution atomically.
 *
 * @pre pdev != NULL
 *
 * @return If there's a successfully initialized acrn_vm_pci_dev_config return it, otherwise return NULL;
 */
struct acrn_vm_pci_dev_config *init_one_dev_config(struct pci_pdev *pdev)
{
	uint16_t vmid;
	struct acrn_vm_config *vm_config;
	struct acrn_vm_pci_dev_config *dev_config = NULL;

	if (!is_allocated_to_prelaunched_vm(pdev) && !need_hide_from_sos(pdev)) {
		for (vmid = 0U; vmid < CONFIG_MAX_VM_NUM; vmid++) {
			vm_config = get_vm_config(vmid);
			if (vm_config->load_order != SOS_VM) {
				continue;
			}

			dev_config = &vm_config->pci_devs[vm_config->pci_dev_num];
			dev_config->emu_type = PCI_DEV_TYPE_PTDEV;
			dev_config->vbdf.value = pdev->bdf.value;
			dev_config->pbdf.value = pdev->bdf.value;
			dev_config->pdev = pdev;
			vm_config->pci_dev_num++;
		}
	}
	return dev_config;
}
