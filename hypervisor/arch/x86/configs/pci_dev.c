/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/vm_config.h>
#include <pci.h>
#include <x86/pci_dev.h>
#include <vpci.h>

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

	if (!is_allocated_to_prelaunched_vm(pdev)) {
		for (vmid = 0U; vmid < CONFIG_MAX_VM_NUM; vmid++) {
			vm_config = get_vm_config(vmid);
			if (vm_config->load_order != SOS_VM) {
				continue;
			}

			dev_config = &vm_config->pci_devs[vm_config->pci_dev_num];
			if (is_hv_owned_pdev(pdev->bdf)) {
				/* SOS need to emulate the type1 pdevs owned by HV */
				dev_config->emu_type = PCI_DEV_TYPE_SOSEMUL;
				if (is_bridge(pdev)) {
					dev_config->vdev_ops = &vpci_bridge_ops;
				} else if (is_host_bridge(pdev)) {
					dev_config->vdev_ops = &vhostbridge_ops;
				} else {
					/* May have type0 device, E.g. debug pci uart */
					break;
				}
			} else {
				dev_config->emu_type = PCI_DEV_TYPE_PTDEV;
			}

			dev_config->vbdf.value = pdev->bdf.value;
			dev_config->pbdf.value = pdev->bdf.value;
			dev_config->pdev = pdev;
			vm_config->pci_dev_num++;
		}
	}
	return dev_config;
}
