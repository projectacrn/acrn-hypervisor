/*
 * Copyright (C) 2019-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/vm_config.h>
#include <pci.h>
#include <asm/pci_dev.h>
#include <vpci.h>

/*
 * @pre pdev != NULL;
 */
static bool allocate_to_prelaunched_vm(struct pci_pdev *pdev)
{
	bool found = false;
	uint16_t vmid;
	uint16_t pci_idx;
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
	struct acrn_vm_pci_dev_config *dev_config = NULL;
	bool is_allocated_to_prelaunched_vm = allocate_to_prelaunched_vm(pdev);
	bool is_allocated_to_hv = is_hv_owned_pdev(pdev->bdf);

	if (service_vm_config != NULL) {
		dev_config = &service_vm_config->pci_devs[service_vm_config->pci_dev_num];

		if (is_allocated_to_hv) {
			/* Service VM need to emulate the type1 pdevs owned by HV */
			dev_config->emu_type = PCI_DEV_TYPE_SERVICE_VM_EMUL;
			if (is_bridge(pdev)) {
				dev_config->vdev_ops = &vpci_bridge_ops;
			} else if (is_host_bridge(pdev)) {
				dev_config->vdev_ops = &vhostbridge_ops;
			} else {
				/* May have type0 device, E.g. debug pci uart */
				dev_config = NULL;
			}
		} else if (is_allocated_to_prelaunched_vm) {
			dev_config = NULL;
		} else {
			dev_config->emu_type = PCI_DEV_TYPE_PTDEV;
		}

		if ((is_allocated_to_hv || is_allocated_to_prelaunched_vm)
				&& (dev_config == NULL)
				&& is_pci_cfg_multifunction(pdev->hdr_type)
				&& (pdev->bdf.bits.f == 0U))
		{
			dev_config = &service_vm_config->pci_devs[service_vm_config->pci_dev_num];
			dev_config->emu_type = PCI_DEV_TYPE_DUMMY_MF_EMUL;
			dev_config->vdev_ops = &vpci_mf_dev_ops;
		}
	}

	if (dev_config != NULL) {
		dev_config->vbdf.value = pdev->bdf.value;
		dev_config->pbdf.value = pdev->bdf.value;
		dev_config->pdev = pdev;
		service_vm_config->pci_dev_num++;
	}
	return dev_config;
}
