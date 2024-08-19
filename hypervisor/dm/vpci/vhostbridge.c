/*-
* Copyright (c) 2011 NetApp, Inc.
* Copyright (c) 2018-2024 Intel Corporation.
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

#include <asm/guest/vm.h>
#include <pci.h>
#include "vpci_priv.h"
#include <vacpi.h>

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief Implementation of virtual PCI host bridge.
 *
 * This file defines operations to support virtual PCI host bridge. All the operation related APIs are registered as the
 * callbacks in the global variable "struct pci_vdev_ops vhostbridge_ops".
 */

/*
The chart below shows the hostbridge DID high-byte of the platform later than broadwell, whose PCIEXBAR are always
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
TODO:
1. In the future, we may add one or more virtual hostbridges for CPUs that are incompatible in layout with the current
one
2. Besides PCIEXBAR(0x60), there are also some registers needs to be emulated more precisely rather than be treated as
read-only and hard-coded, listed below:

-----------------------------------------------------------------------------------------------
reg		|offset	|length	|current status	|remark
-----------------------------------------------------------------------------------------------
STATUS_COMMAND	|0x8	|dword	|unemulated	|pci status and command
SVID_SID	|0x2C	|dword	|unemulated	|subsys id and subsys vendor id
MCHBAR		|0x48	|qword	|hard-coded	|BAR of memory controller hub
GGC		|0x50	|dword	|hard-coded	|graphics & mem controller hub graphics CR
DEVEN		|0x54	|dword	|hard-coded	|device enable register
PAVPC		|0x58	|dword	|hard-coded	|protected audio video path control
TOUUD		|0xA8	|qword	|hard-coded	|top of upper usable DRAM
BDSM		|0xB0	|dword	|hard-coded	|base of data stolen memory
BGSM		|0xB4	|dword	|hard-coded	|base of graphics stolen memory
TSEGMB		|0xB8	|dword	|hard-coded	|top segmentmemory base
TOLUD		|0xBC	|dword	|hard-coded	|top of lower usable dram
SKPD		|0xDC	|dword	|unemulated	|scratchpad
CAPID0_CAPCTRL0	|0xe0	|dword	|hard-coded	|capability 0 control
-----------------------------------------------------------------------------------------------
*/

/**
 * @brief Initialize the virtual host bridge.
 *
 * A host bridge is a PCI device that is used to support the pci devices under it. This function initializes the
 * specified virtual PCI device as a host bridge. It's usually called during the initialization of a VM.
 *
 * This function emulates the virtual host bridge as a "Celeron N3350/Pentium N4200/Atom E3900 Series Host Bridge",
 * which belongs to Intel Apollo Lake processors family, and the device id is 0x5af0. Per "Section 9 C-Unit in Intel®
 * Pentium® and Celeron® Processor N- and J- Series, Datasheet Volume 2", it initializes related type info registers in
 * configuration space.
 * PCI Express Enhanced Configuration Range Base Address Register (PCIEXBAR) is emulated differently for pre-launched
 * VMs and Service VM, to support PCI Express Enhanced Configuration Access Mechanism (ECAM).
 * - For a pre-launched VM, it is emulated as 'USER_VM_VIRT_PCI_MMCFG_BASE | 0x1'. USER_VM_VIRT_PCI_MMCFG_BASE
 *   (0xE0000000) is the hard-coded virtual PCI MMCFG address base for pre/post-launched VMs. Bit 0 is set to 1 to
 *   indicate that the base address defined in the PCIEXBAR register is active.
 * - For a Service VM, it is emulated to be the same value as the physical PCIEXBAR. It is not used for now, mainly for
 *   feature extension in the future.
 * Finally, it sets the field parent_user to NULL and the field user to vdev, indicating that this vPCI bridge is used
 * by a VM.
 *
 * @param[inout] vdev Pointer to the virtual PCI device to be initialized.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 *
 * @post N/A
 */
static void init_vhostbridge(struct pci_vdev *vdev)
{
	union pci_bdf hostbridge_bdf = {.value = 0x0U};
	uint32_t pciexbar_low = 0x0U, pciexbar_high = 0x0U, phys_did, i;
	/* Refer to Section 9 C-Unit in Intel® Pentium® and Celeron® Processor N- and J- Series, Datasheet Volume 2 */
	/* PCI config space */
	pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, 0x8086U);
	pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, 0x5af0U);
	pci_vdev_write_vcfg(vdev, PCIR_REVID, 1U, 0xbU);
	pci_vdev_write_vcfg(vdev, PCIR_SUBCLASS, 1U, PCIS_BRIDGE_HOST);
	pci_vdev_write_vcfg(vdev, PCIR_CLASS, 1U, PCIC_BRIDGE);
	pci_vdev_write_vcfg(vdev, PCIR_HDRTYPE, 1U, (PCIM_HDRTYPE_NORMAL | PCIM_MFDEV));
	/* First Capability Register is CAPID0_CAPCTRL0 */
	pci_vdev_write_vcfg(vdev, PCIR_CAP_PTR, 1U, 0xe0U);
	pci_vdev_write_vcfg(vdev, PCIR_INTERRUPT_LINE, 1U, 0xe0U);

	/* Memory Controller Hub Base Address Register, MCHBAR_LO */
	/* MCHBAR[38:15] is {MCHBAR_HI[6:0],MCHBAR_LO[31:15]} */
	pci_vdev_write_vcfg(vdev, 0x48U, 4U, 0xfed10001U);
	/* Graphics and Memory Controller Hub Graphics Control Register, GGC */
	/* [15:8] is Graphics Memory Select (GMS), 512MB */
	pci_vdev_write_vcfg(vdev, 0x50U, 4U, 0x000002c1U);
	/* Device Enable Register, DEVEN */
	pci_vdev_write_vcfg(vdev, 0x54U, 4U, 0x00000033U);
	/* Protected Audio Video Path Control, PAVPC */
	pci_vdev_write_vcfg(vdev, 0x58U, 4U, 0x7ff00007U);
	/* Top of Upper Usable DRAM Low, TOUUD_LO */
	pci_vdev_write_vcfg(vdev, 0xa8U, 4U, 0x80000000U);
	/* Top of Upper Usable DRAM High, TOUUD_HI */
	pci_vdev_write_vcfg(vdev, 0xacU, 4U, 0x00000002U);
	/* Base of Data Stolen Memory, BDSM */
	pci_vdev_write_vcfg(vdev, 0xb0U, 4U, 0x7c000001U);
	/* Base of Graphics Stolen Memory, BGSM */
	pci_vdev_write_vcfg(vdev, 0xb4U, 4U, 0x7b800001U);
	/* Top Segment Memory Base, TSEGMB */
	pci_vdev_write_vcfg(vdev, 0xb8U, 4U, 0x7b000001U);
	/* Top of Lower Usable DRAM, TOLUD */
	pci_vdev_write_vcfg(vdev, 0xbcU, 4U, 0x80000001U);
	/* Capability ID0 Capability Control, CAPID0_CAPCTRL0 */
	/* CAP_ID: 9h, NEXT_CAP: 0h, CAPIDLEN: Ch, CAPID_VER: 1h */
	pci_vdev_write_vcfg(vdev, 0xe0U, 4U, 0x010c0009U);
	pci_vdev_write_vcfg(vdev, 0xf4U, 4U, 0x011c0f00U);

	if (is_prelaunched_vm(container_of(vdev->vpci, struct acrn_vm, vpci))) {
		/* For pre-launched VMs, we only need to write an GPA that's reserved in guest ve820,
		 * and USER_VM_VIRT_PCI_MMCFG_BASE(0xE0000000) is fine. The trailing 1 is a ECAM enable-bit
		 */
		pciexbar_low = USER_VM_VIRT_PCI_MMCFG_BASE | 0x1U;
	} else {
		/* Inject physical ECAM value to Service VM vhostbridge since Service VM may check PCIe-MMIO Base
		Address with it */
		phys_did = pci_pdev_read_cfg(hostbridge_bdf, PCIR_DEVICE, 2);
		for (i = 0U; i < (sizeof(hostbridge_did_highbytes) / sizeof(uint32_t)); i++) {
			if (((phys_did & 0xff00U) >> 8) == hostbridge_did_highbytes[i]) {
				/* The offset of PCIEXBAR register is 0x60 on Intel platforms, and no counter-case is
				encountered yet */
				pciexbar_low = pci_pdev_read_cfg(hostbridge_bdf, 0x60U, 4);
				pciexbar_high = pci_pdev_read_cfg(hostbridge_bdf, 0x64U, 4);
				break;
			}
		}
	}
	/* PCI Express Enhanced Configuration Range Base Address Low, PCIEXBAR_LO */
	pci_vdev_write_vcfg(vdev, 0x60U, 4, pciexbar_low);
	/* PCI Express Enhanced Configuration Range Base Address High, PCIEXBAR_HI */
	pci_vdev_write_vcfg(vdev, 0x64U, 4, pciexbar_high);
	vdev->parent_user = NULL;
	vdev->user = vdev;
}

/**
 * @brief Deinitialize the virtual host bridge.
 *
 * This function deinitializes the specified virtual PCI device that was previously initialized as a host bridge.
 *
 * For the specified vdev, it sets the fields parent_user and user to NULL, indicating that this virtual device is not
 * owned by any VM.
 *
 * @param[inout] vdev Pointer to the virtual PCI device.
 *
 * @return None
 *
 * @pre vdev != NULL
 *
 * @post N/A
 */
static void deinit_vhostbridge(struct pci_vdev *vdev)
{
	vdev->parent_user = NULL;
	vdev->user = NULL;
}

/**
 * @brief Read the configuration space of the virtual host bridge.
 *
 * This function reads the configuration space of the specified virtual PCI device that is configured as a host bridge.
 * It is used to retrieve specific configuration data of the virtual host bridge for further processing or validation.
 *
 * It reads the configuration space of the virtual host bridge and stores the read configuration data in the provided
 * buffer.
 *
 * @param[in] vdev Pointer to the virtual PCI device whose configuration space is to be read.
 * @param[in] offset Offset within the configuration space to read from.
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
static int32_t read_vhostbridge_cfg(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t *val)
{
	*val = pci_vdev_read_vcfg(vdev, offset, bytes);
	return 0;
}

/**
 * @brief Write to the virtual host bridge configuration space.
 *
 * This function writes to the configuration space of the specified virtual PCI device that is configured as a host
 * bridge. It is used to update specific configuration settings based on the provided parameters.
 *
 * For the non-BAR configuration space, it writes the provided value to the configuration space of the virtual host
 * bridge. For the BAR configuration space, it is read-only and the write operation is ignored.
 *
 * @param[inout] vdev Pointer to the virtual PCI device whose configuration space is to be written.
 * @param[in] offset Offset within the configuration space to start writing to.
 * @param[in] bytes Number of bytes to write to the configuration space.
 * @param[in] val Value to be written to the configuration space.
 *
 * @return Always return 0.
 *
 * @pre vdev != NULL
 *
 * @post retval == 0
 */
static int32_t write_vhostbridge_cfg(struct pci_vdev *vdev, uint32_t offset,
	uint32_t bytes, uint32_t val)
{
	if (!is_bar_offset(PCI_BAR_COUNT, offset)) {
		pci_vdev_write_vcfg(vdev, offset, bytes, val);
	}
	return 0;
}

/**
 * @brief Data structure implementation for virtual host bridge operations.
 *
 * Struct pci_vdev_ops is used to define the operations of virtual PCI device and definition here is used to support
 * virtual host bridge.
 *
 * A pre-launched VM may have some pci devices and a host bridge is needed to support these devices. This struct is used
 * to define the operations of virtual host bridge in this case for now.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
const struct pci_vdev_ops vhostbridge_ops = {
	.init_vdev	= init_vhostbridge,
	.deinit_vdev	= deinit_vhostbridge,
	.write_vdev_cfg	= write_vhostbridge_cfg,
	.read_vdev_cfg	= read_vhostbridge_cfg,
};

/**
 * @}
 */