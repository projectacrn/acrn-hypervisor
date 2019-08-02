/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <pci.h>

static uint16_t pcidev_config_num = 0U;
static struct acrn_vm_pci_dev_config pcidev_config[CONFIG_MAX_PCI_DEV_NUM] = {};

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

	for (vmid = 0U; vmid < CONFIG_MAX_VM_NUM; vmid++) {
		vm_config = get_vm_config(vmid);
		if (vm_config->load_order != PRE_LAUNCHED_VM) {
			continue;
		}

		for (pci_idx = 0U; pci_idx < vm_config->pci_dev_num; pci_idx++) {
			dev_config = &vm_config->pci_devs[pci_idx];
			if ((dev_config->emu_type == PCI_DEV_TYPE_PTDEV) &&
					bdf_is_equal(&dev_config->pbdf, &pdev->bdf)) {
				dev_config->pdev = pdev;
				found = true;
				break;
			}
		}

		if (found) {
			break;
		}
	}

	return found;
}


/*
 * @pre: pdev != NULL
 */
void fill_pci_dev_config(struct pci_pdev *pdev)
{
	struct acrn_vm_pci_dev_config *dev_config;

	if (!is_allocated_to_prelaunched_vm(pdev)) {
		dev_config = &pcidev_config[pcidev_config_num];
		dev_config->emu_type = (pdev->bdf.value != HOST_BRIDGE_BDF) ? PCI_DEV_TYPE_PTDEV : PCI_DEV_TYPE_HVEMUL;
		dev_config->vbdf.value = pdev->bdf.value;
		dev_config->pbdf.value = pdev->bdf.value;
		dev_config->pdev = pdev;
		pcidev_config_num++;
	}
}

/*
 * @pre vm_config != NULL
 */
void initialize_sos_pci_dev_config(struct acrn_vm_config *vm_config)
{
	vm_config->pci_dev_num = pcidev_config_num;
	vm_config->pci_devs = pcidev_config;
}
