/*
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2019-2024 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Emulate a PCI bridge: Intel Corporation Sunrise Point-LP (rev f1)
 * Assumptions:
 *   1. before hypervisor bootup, all PCI devices have been configured correctly
 * by BIOS(boot loader). It's not expected service OS change the configure;
 *   2. for ACS(Access Control Service) Capability in PCI bridge is enabled and configured
 * by BIOS to support the devices under it isolated and allocated to different VMs.
 *
 * for this emulation of vpci bridge, limitations set as following:
 *   1. all configure registers are readonly
 *
 * TODO:
 *  1. configure tool can select whether a PCI bridge is emulated or pass through
 *
 * Open:
 *   1. Service VM how to reset PCI devices under the PCI bridge
 */

#include <asm/guest/vm.h>
#include <errno.h>
#include <logmsg.h>
#include <pci.h>
#include "vpci_priv.h"

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief Implementation of virtual PCI bridge.
 *
 * This file defines operations to support virtual PCI bridge. It implements struct pci_vdev_ops and related functions.
 */

/**
 * @brief Initializes the vPCI bridge.
 *
 * A PCI bridge is also a PCI device. This function initializes the specified virtual PCI device as a PCI bridge. A vPCI
 * bridge is based on a physical PCI bridge and is used for Service VM. It's usually used in the initialization phase of
 * Service VM, when the pre-launched VM exists.
 *
 * It initializes most of the virtual PCI configuration space registers based on the physical PCI bridge registers,
 * except those that specify the type information, which is emulated. Such type related registers include Vendor ID,
 * Device ID, Revision ID, Header Type, class and sub-class code. Finally, it sets the field parent_user to NULL and
 * the field user to vdev, indicating that this vPCI bridge is used by a VM.
 * Note that the physical PCI bridge registers are already initialized and configured during the initialization of the
 * physical PCI hierarchy.
 *
 * @param[inout] vdev Pointer to the virtual PCI device to be initialized.
 *
 * @return None
 *
 * @pre vdev != NULL
 *
 * @post N/A
 */
static void init_vpci_bridge(struct pci_vdev *vdev)
{
	uint32_t offset, val;

	/* read PCI config space to virtual space */
	for (offset = 0x00U; offset < 0x100U; offset += 4U) {
		val = pci_pdev_read_cfg(vdev->pdev->bdf, offset, 4U);
		pci_vdev_write_vcfg(vdev, offset, 4U, val);
	}

	/* emulated for type info */
	pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, 0x8086U);
	pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, 0x9d12U);

	pci_vdev_write_vcfg(vdev, PCIR_REVID, 1U, 0xf1U);

	pci_vdev_write_vcfg(vdev, PCIR_HDRTYPE, 1U, (PCIM_HDRTYPE_BRIDGE | PCIM_MFDEV));
	pci_vdev_write_vcfg(vdev, PCIR_CLASS, 1U, PCIC_BRIDGE);
	pci_vdev_write_vcfg(vdev, PCIR_SUBCLASS, 1U, PCIS_BRIDGE_PCI);

	vdev->parent_user = NULL;
	vdev->user = vdev;
}

/**
 * @brief Deinitializes the vPCI bridge.
 *
 * This function deinitializes the specified virtual PCI device that was previously initialized as a PCI bridge.
 *
 * For the specified vdev, it sets the fields parent_user and user to NULL, indicating that this virtual device is not
 * owned by any VM.
 *
 * @param[inout] vdev Pointer to the virtual PCI device to be deinitialized.
 *
 * @return None
 *
 * @pre vdev != NULL
 *
 * @post N/A
 */
static void deinit_vpci_bridge(struct pci_vdev *vdev)
{
	vdev->parent_user = NULL;
	vdev->user = NULL;
}

/**
 * @brief Reads the configuration of the vPCI bridge.
 *
 * This function reads the configuration space of the specified virtual PCI device that is configured as a PCI bridge.
 * It is used to retrieve the configuration data of the vPCI bridge for further processing or validation.
 *
 * - For PCI configuration space (offset <= 0x100U), it reads the configuration space of the vPCI bridge.
 * - For PCI Express Extended configuration space (offset > 0x100U), it simply passthrough by reading directly from the
 *   physical device.
 * - The read configuration data is stored in the buffer pointed to by val.
 *
 * @param[in] vdev Pointer to the virtual PCI device whose configuration is to be read.
 * @param[in] offset Offset within the configuration space to start reading from.
 * @param[in] bytes Number of bytes to read from the configuration space.
 * @param[inout] val Pointer to the buffer where the read configuration data will be stored.
 *
 * @return Always return 0.
 *
 * @pre vdev != NULL
 * @pre val != NULL
 *
 * @post retval == 0
 */
static int32_t read_vpci_bridge_cfg(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	if ((offset + bytes) <= 0x100U) {
		*val = pci_vdev_read_vcfg(vdev, offset, bytes);
	} else {
		/* just passthru read to physical device when read PCIE sapce > 0x100 */
		*val = pci_pdev_read_cfg(vdev->pdev->bdf, offset, bytes);
	}

	return 0;
}

/**
 * @brief Writes the configuration of the vPCI bridge.
 *
 * This function writes to the configuration space of the specified virtual PCI device that is configured as a PCI
 * bridge. It is used to update the configuration data of the vPCI bridge. However, the configuration space of the vPCI
 * bridge is read-only, so this function does not perform any operation.
 *
 * It just returns 0 without any operation.
 *
 * @param[in] vdev Pointer to the virtual PCI device whose configuration is to be written (unused in this function).
 * @param[in] offset Offset within the configuration space to start writing to (unused in this function).
 * @param[in] bytes Number of bytes to write to the configuration space (unused in this function).
 * @param[in] val Value to be written to the configuration space (unused in this function).
 *
 * @return Always return 0.
 *
 * @pre vdev != NULL
 *
 * @post retval == 0
 */
static int32_t write_vpci_bridge_cfg(__unused struct pci_vdev *vdev, __unused uint32_t offset,
	__unused uint32_t bytes, __unused uint32_t val)
{
	return 0;
}

/**
 * @brief Data structure implementation for virtual PCI bridge operations.
 *
 * Struct pci_vdev_ops is used to define the operations of virtual PCI device and definition here is used to support PCI
 * bridge.
 *
 * All PCI devices (including PCI bridge) on platform are passed to Service VM by default. But PCI bridges should be
 * emulated by hypervisor if pre-launched VM exists. This struct is used to define the operations of virtual PCI bridge
 * in this case.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
const struct pci_vdev_ops vpci_bridge_ops = {
	.init_vdev         = init_vpci_bridge,
	.deinit_vdev       = deinit_vpci_bridge,
	.write_vdev_cfg    = write_vpci_bridge_cfg,
	.read_vdev_cfg     = read_vpci_bridge_cfg,
};

/**
 * @}
 */