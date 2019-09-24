/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <pci.h>
#include <pci_dev.h>
#include <vpci.h>

struct acrn_vm_pci_dev_config sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM] = {
	{
		.emu_type = PCI_DEV_TYPE_HVEMUL,
		.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},
		.vdev_ops = &vhostbridge_ops,
	},
};

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
					bdf_is_equal(dev_config->pbdf, pdev->bdf)) {
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
	uint16_t vmid;
	struct acrn_vm_config *vm_config;
	struct acrn_vm_pci_dev_config *dev_config;

	if (!is_allocated_to_prelaunched_vm(pdev)) {
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
}
