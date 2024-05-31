/*
 * Copyright (c) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <logmsg.h>
#include <pci.h>
#include <asm/guest/vm.h>
#include <acrn_common.h>
#include "vroot_port.h"

#include "vpci_priv.h"

#define PCIE_CAP_VPOS		0x40				/* pcie capability reg position */
#define PTM_CAP_VPOS		PCI_ECAP_BASE_PTR	/* ptm capability reg postion */

static void init_vrp(struct pci_vdev *vdev)
{
	/* vendor and device */
	pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, VRP_VENDOR);
	pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, VRP_DEVICE);

	/* status register */
	pci_vdev_write_vcfg(vdev, PCIR_STATUS, 2U, PCIM_STATUS_CAPPRESENT);

	/* rev id */
	pci_vdev_write_vcfg(vdev, PCIR_REVID, 1U, 0x01U);

	/* sub class */
	pci_vdev_write_vcfg(vdev, PCIR_SUBCLASS, 1U, PCIS_BRIDGE_PCI);

	/* class */
	pci_vdev_write_vcfg(vdev, PCIR_CLASS, 1U, PCIC_BRIDGE);

	/* Header Type */
	pci_vdev_write_vcfg(vdev, PCIR_HDRTYPE, 1U, PCIM_HDRTYPE_BRIDGE);

	/* capability pointer */
	pci_vdev_write_vcfg(vdev, PCIR_CAP_PTR, 1U, PCIE_CAP_VPOS);

	/* pcie capability registers  */
	pci_vdev_write_vcfg(vdev, PCIE_CAP_VPOS + PCICAP_ID, 1U, PCIY_PCIE);

	/* bits (3:0): capability version = 010b
	 * bits (7:4)  device/port type = 0100b (root port of pci-e)
	 * bits (8) -- slot implemented = 1b
	 */
	pci_vdev_write_vcfg(vdev, PCIE_CAP_VPOS + PCICAP_EXP_CAP, 2U, 0x0142);

	/* It seems important that passthru device's max payload settings match
	 * the settings on the native device otherwise passthru device may not work.
	 * So we have to set vrp's max payload capacity as native root port
	 * otherwise we may accidentally change passthru device's max payload since
	 * during guest OS's pci device enumeration, pass-thru device will renegotiate
	 * its max payload's setting with vrp.
	 */
	pci_vdev_write_vcfg(vdev, PCIE_CAP_VPOS + PCIR_PCIE_DEVCAP, 4U,
			vdev->pci_dev_config->vrp_max_payload);

	/* In theory, we don't need to program dev ctr's max payload and hopefully OS
	 * will program it but we cannot always rely on OS to program
	 * this register.
	 */
	pci_vdev_write_vcfg(vdev, PCIE_CAP_VPOS + PCIR_PCIE_DEVCTRL, 2U,
			(vdev->pci_dev_config->vrp_max_payload << 5) & PCIM_PCIE_DEV_CTRL_MAX_PAYLOAD);

	vdev->parent_user = NULL;
	vdev->user = vdev;
}

static void deinit_vrp(__unused struct pci_vdev *vdev)
{
	vdev->parent_user = NULL;
	vdev->user = NULL;
}

static int32_t read_vrp_cfg(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	*val = pci_vdev_read_vcfg(vdev, offset, bytes);

	return 0;
}

static int32_t write_vrp_cfg(__unused struct pci_vdev *vdev, __unused uint32_t offset,
	__unused uint32_t bytes, __unused uint32_t val)
{
	pci_vdev_write_vcfg(vdev, offset, bytes, val);

	return 0;
}

/*
 * @pre vdev != NULL
 * @pre vrp_config != NULL
 */
static void init_ptm(struct pci_vdev *vdev, struct vrp_config *vrp_config)
{
	/* ptm capability register */
	if (vrp_config->ptm_capable)
	{
		pci_vdev_write_vcfg(vdev, PTM_CAP_VPOS, PCI_PTM_CAP_LEN, 0x0001001f);

		pci_vdev_write_vcfg(vdev, PTM_CAP_VPOS + PCIR_PTM_CAP, PCI_PTM_CAP_LEN, 0x406);

		pci_vdev_write_vcfg(vdev, PTM_CAP_VPOS + PCIR_PTM_CTRL, PCI_PTM_CAP_LEN, 0x3);
	}

	/* emulate bus numbers */
	pci_vdev_write_vcfg(vdev, PCIR_PRIBUS_1, 1U, 0x00); /* virtual root port always connects to host bridge */
	pci_vdev_write_vcfg(vdev, PCIR_SECBUS_1, 1U, vrp_config->secondary_bus);
	pci_vdev_write_vcfg(vdev, PCIR_SUBBUS_1, 1U, vrp_config->subordinate_bus);
}

int32_t create_vrp(struct acrn_vm *vm, struct acrn_vdev *dev)
{
	int32_t ret = 0;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct acrn_vm_pci_dev_config *dev_config = NULL;
	struct pci_vdev *vdev;
	struct vrp_config *vrp_config;

	uint16_t i;

	vrp_config = (struct vrp_config*)dev->args;

	pr_acrnlog("%s: virtual root port phy_bdf=0x%x, vbdf=0x%x, vendor_id=0x%x, dev_id=0x%x,\
			primary_bus=0x%x, secondary_bus=0x%x, sub_bus=0x%x.\n",
			__func__, vrp_config->phy_bdf, dev->slot,
			dev->id.fields.vendor, dev->id.fields.device,
			vrp_config->primary_bus, vrp_config->secondary_bus, vrp_config->subordinate_bus);

	for (i = 0U; i < vm_config->pci_dev_num; i++) {
		dev_config = &vm_config->pci_devs[i];
		if (dev_config->vrp_sec_bus == vrp_config->secondary_bus) {
			dev_config->vbdf.value = (uint16_t)dev->slot;
			dev_config->pbdf.value = vrp_config->phy_bdf;
			dev_config->vrp_max_payload = vrp_config->max_payload;
			dev_config->vdev_ops = &vrp_ops;

			spinlock_obtain(&vm->vpci.lock);
			vdev = vpci_init_vdev(&vm->vpci, dev_config, NULL);
			spinlock_release(&vm->vpci.lock);
			if (vdev == NULL) {
				pr_err("%s: failed to create virtual root port\n", __func__);
				ret = -EFAULT;
				break;
			}

			init_ptm(vdev, vrp_config);

			break;
		}
	}

	return ret;
}

int32_t destroy_vrp(struct pci_vdev *vdev)
{
	struct acrn_vpci *vpci = vdev->vpci;

	spinlock_obtain(&vpci->lock);
	vpci_deinit_vdev(vdev);
	spinlock_release(&vpci->lock);

	return 0;
}

const struct pci_vdev_ops vrp_ops = {
	.init_vdev         = init_vrp,
	.deinit_vdev       = deinit_vrp,
	.write_vdev_cfg    = write_vrp_cfg,
	.read_vdev_cfg     = read_vrp_cfg,
};
