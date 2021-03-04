/*
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2019 Intel Corporation
 * All rights reserved.
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
 *   1. SOS how to reset PCI devices under the PCI bridge
 */

#include <x86/guest/vm.h>
#include <errno.h>
#include <logmsg.h>
#include <pci.h>
#include "vpci_priv.h"

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

static void deinit_vpci_bridge(__unused struct pci_vdev *vdev)
{
	vdev->parent_user = NULL;
	vdev->user = NULL;
}

static int32_t read_vpci_bridge_cfg(const struct pci_vdev *vdev, uint32_t offset,
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

static int32_t write_vpci_bridge_cfg(__unused struct pci_vdev *vdev, __unused uint32_t offset,
	__unused uint32_t bytes, __unused uint32_t val)
{
	return 0;
}

const struct pci_vdev_ops vpci_bridge_ops = {
	.init_vdev         = init_vpci_bridge,
	.deinit_vdev       = deinit_vpci_bridge,
	.write_vdev_cfg    = write_vpci_bridge_cfg,
	.read_vdev_cfg     = read_vpci_bridge_cfg,
};
