/*-
* Copyright (c) 2011 NetApp, Inc.
* Copyright (c) 2018 Intel Corporation
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


/*_
* Emulate a PCI Host bridge:
* Intel Corporation Celeron N3350/Pentium N4200/Atom E3900
* Series Host Bridge (rev 0b)
*/

#include <x86/guest/vm.h>
#include <pci.h>
#include "vpci_priv.h"
#include <vacpi.h>

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
/* The chart below shows the hostbridge DID high-byte of the platform later than broadwell, whose PCIEXBAR are always
	located in the PCI hostbridge config space at the offset 0x60. This chart may need further extension in the future
	--------------------------------------------------------------------------------------
		platform			|	hostbridge DID high-byte
	--------------------------------------------------------------------------------------
		SKL(6-gen)			|	0x19
		APL(6-gen Atom)			|	0x5a
		KBL(7/8-gen)			|	0x59
		CFL/CFL-R(8/9-gen)		|	0x3e
		ICL(10-gen)			|	0x9b
		EHL(11-gen)			|	0x45
		TGL(11-gen)			|	0x9a
	--------------------------------------------------------------------------------------
*/
static const uint32_t hostbridge_did_highbytes[] = {0x19U, 0x5aU, 0x59U, 0x3eU, 0x9aU, 0x45U, 0x9bU};

/*
	The vhostbridge we currently emulated is "Celeron N3350/Pentium N4200/Atom E3900 Series Host Bridge",
	which belongs to Intel Appollo Lake processors family, and the device id is 5af0

	TODO:
		1. In the future, we may add one or more virtual hostbridges for CPUs that are incompatible in layout with the current one
		2. Besides PCIEXBAR(0x60), there are also some registers needs to be emulated more precisely rather than be treated as read-only and hard-coded, listed below:

	----------------------------------------------------------------------------------------------------------------
	reg	|	offset	|	length	|	current status	|	remark
	----------------------------------------------------------------------------------------------------------------
	STATUS_COMMAND	|	0x8	|	dword	|	unemulated	|	pci status and command
	SVID_SID	|	0x2C	|	dword	|	unemulated	|	subsys id and subsys vendor id
	MCHBAR		|	0x48	|	qword	|	hard-coded	|	BAR of memory controller hub
	GGC		|	0x50	|	dword	|	hard-coded	|	graphics & mem controller hub graphics CR
	DEVEN		|	0x54	|	dword	|	hard-coded	|	device enable register
	PAVPC		|	0x58	|	dword	|	hard-coded	|	protected audio video path control
	TOUUD		|	0xA8	|	qword	|	hard-coded	|	top of upper usable DRAM
	BDSM		|	0xB0	|	dword	|	hard-coded	|	base of data stolen memory
	BGSM		|	0xB4	|	dword	|	hard-coded	|	base of graphics stolen memory
	TSEGMB		|	0xB8	|	dword	|	hard-coded	|	top segmentmemory base
	TOLUD		|	0xBC	|	dword	|	hard-coded	|	top of lower usable dram
	SKPD		|	0xDC	|	dword	|	unemulated	|	scratchpad
	CAPID0_CAPCTRL0	|	0xe0	|	dword	|	hard-coded	|	capability 0 control
	----------------------------------------------------------------------------------------------------------------
*/
static void init_vhostbridge(struct pci_vdev *vdev)
{
	union pci_bdf hostbridge_bdf = {.value = 0x0U};
	uint32_t pciexbar_low = 0x0U, pciexbar_high = 0x0U, phys_did, i;
	/* PCI config space */
	pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, 0x8086U);
	pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, 0x5af0U);
	pci_vdev_write_vcfg(vdev, PCIR_REVID, 1U, 0xbU);
	pci_vdev_write_vcfg(vdev, PCIR_HDRTYPE, 1U, (PCIM_HDRTYPE_NORMAL | PCIM_MFDEV));
	pci_vdev_write_vcfg(vdev, PCIR_CLASS, 1U, PCIC_BRIDGE);
	pci_vdev_write_vcfg(vdev, PCIR_SUBCLASS, 1U, PCIS_BRIDGE_HOST);
	pci_vdev_write_vcfg(vdev, 0x34U, 1U, 0xe0U);
	pci_vdev_write_vcfg(vdev, 0x3cU, 1U, 0xe0U);

	pci_vdev_write_vcfg(vdev, 0x48U, 4U, 0xfed10001U);
	pci_vdev_write_vcfg(vdev, 0x50U, 4U, 0x000002c1U);
	pci_vdev_write_vcfg(vdev, 0x54U, 4U, 0x00000033U);
	pci_vdev_write_vcfg(vdev, 0x58U, 4U, 0x7ff00007U);
	pci_vdev_write_vcfg(vdev, 0xa8U, 4U, 0x80000000U);
	pci_vdev_write_vcfg(vdev, 0xacU, 4U, 0x00000002U);
	pci_vdev_write_vcfg(vdev, 0xb0U, 4U, 0x7c000001U);
	pci_vdev_write_vcfg(vdev, 0xb4U, 4U, 0x7b800001U);
	pci_vdev_write_vcfg(vdev, 0xb8U, 4U, 0x7b000001U);
	pci_vdev_write_vcfg(vdev, 0xbcU, 4U, 0x80000001U);
	pci_vdev_write_vcfg(vdev, 0xe0U, 4U, 0x010c0009U);
	pci_vdev_write_vcfg(vdev, 0xf4U, 4U, 0x011c0f00U);

	if (is_prelaunched_vm(container_of(vdev->vpci, struct acrn_vm, vpci))) {
		/* For pre-launched VMs, we only need to write an GPA that's reserved in guest ve820,
		 * and UOS_VIRT_PCI_MMCFG_BASE(0xE0000000) is fine. The trailing 1 is a ECAM enable-bit
		 */
		pciexbar_low = UOS_VIRT_PCI_MMCFG_BASE | 0x1U;
	}
	else {
		/*Inject physical ECAM value to SOS vhostbridge since SOS may check PCIe-MMIO Base Address with it */
		phys_did = pci_pdev_read_cfg(hostbridge_bdf, PCIR_DEVICE, 2);
		for (i = 0U; i < (sizeof(hostbridge_did_highbytes) / sizeof(uint32_t)); i++){
			if (((phys_did & 0xff00U) >> 8) == hostbridge_did_highbytes[i]) {
				/* The offset of PCIEXBAR register is 0x60 on Intel platforms, and no counter-case is encountered yet */
				pciexbar_low = pci_pdev_read_cfg(hostbridge_bdf, 0x60U, 4);
				pciexbar_high = pci_pdev_read_cfg(hostbridge_bdf, 0x64U, 4);
				break;
			}
		}
	}

	pci_vdev_write_vcfg(vdev, 0x60U, 4, pciexbar_low);
	pci_vdev_write_vcfg(vdev, 0x64U, 4, pciexbar_high);
	vdev->parent_user = NULL;
	vdev->user = vdev;
}

static void deinit_vhostbridge(__unused struct pci_vdev *vdev)
{
	vdev->parent_user = NULL;
	vdev->user = NULL;
}


/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
static int32_t read_vhostbridge_cfg(const struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	*val = pci_vdev_read_vcfg(vdev, offset, bytes);
	return 0;
}


/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
static int32_t write_vhostbridge_cfg(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t val)
{
	if (!is_bar_offset(PCI_BAR_COUNT, offset)) {
		pci_vdev_write_vcfg(vdev, offset, bytes, val);
	}
	return 0;
}

const struct pci_vdev_ops vhostbridge_ops = {
	.init_vdev	= init_vhostbridge,
	.deinit_vdev	= deinit_vhostbridge,
	.write_vdev_cfg	= write_vhostbridge_cfg,
	.read_vdev_cfg	= read_vhostbridge_cfg,
};
