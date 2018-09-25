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

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#include "pci_priv.h"


static bool is_cfg_addr(uint16_t addr)
{
	return (addr >= PCI_CONFIG_ADDR) && (addr < (PCI_CONFIG_ADDR + 4U));
}

static bool is_cfg_data(uint16_t addr)
{
	return (addr >= PCI_CONFIG_DATA) && (addr < (PCI_CONFIG_DATA + 4U));
}

static void pci_cfg_clear_cache(struct pci_addr_info *pi)
{
	pi->cached_bdf.value = 0xFFFFU;
	pi->cached_reg = 0U;
	pi->cached_enable = 0U;
}

static uint32_t pci_cfg_io_read(struct vm *vm, uint16_t addr, size_t bytes)
{
	uint32_t val = 0xFFFFFFFFU;
	struct vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;

	if (is_cfg_addr(addr)) {
		/* TODO: handling the non 4 bytes access */
		if (bytes == 4U) {
			val = (uint32_t)pi->cached_bdf.value;
			val <<= 8U;
			val |= pi->cached_reg;
			if (pi->cached_enable) {
				val |= PCI_CFG_ENABLE;
			}
		}
	} else if (is_cfg_data(addr)) {
		if (pi->cached_enable) {
			uint16_t offset = addr - PCI_CONFIG_DATA;

			pci_vdev_cfg_handler(vpci, 1U, pi->cached_bdf,
				pi->cached_reg + offset, bytes, &val);

			pci_cfg_clear_cache(pi);
		}
	}  else {
		val = 0xFFFFFFFFU;
	}

	return val;
}

static void pci_cfg_io_write(struct vm *vm, uint16_t addr, size_t bytes,
			uint32_t val)
{
	struct vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;

	if (is_cfg_addr(addr)) {
		/* TODO: handling the non 4 bytes access */
		if (bytes == 4U) {
			pi->cached_bdf.bits.b = (val >> 16U) & PCI_BUSMAX;
			pi->cached_bdf.bits.d = (val >> 11U) & PCI_SLOTMAX;
			pi->cached_bdf.bits.f = (val >> 8U) & PCI_FUNCMAX;

			pi->cached_reg = val & PCI_REGMAX;
			pi->cached_enable =
			(val & PCI_CFG_ENABLE) == PCI_CFG_ENABLE;
		}
	} else if (is_cfg_data(addr)) {
		if (pi->cached_enable) {
			uint16_t offset = addr - PCI_CONFIG_DATA;

			pci_vdev_cfg_handler(vpci, 0U, pi->cached_bdf,
				pi->cached_reg + offset, bytes, &val);

			pci_cfg_clear_cache(pi);
		}
	} else {
		pr_err("Not PCI cfg data/addr port access!");
	}

}

void vpci_init(struct vm *vm)
{
	struct vpci *vpci = &vm->vpci;
	struct vpci_vdev_array *vdev_array;
	struct pci_vdev *vdev;
	int i;
	int ret;
	struct vm_io_range pci_cfg_range = {.flags = IO_ATTR_RW,
		.base = PCI_CONFIG_ADDR, .len = 8U};

	vpci->vm = vm;
	vdev_array = vm->vm_desc->vpci_vdev_array;

	for (i = 0; i < vdev_array->num_pci_vdev; i++) {
		vdev = &vdev_array->vpci_vdev_list[i];
		vdev->vpci = vpci;

		if ((vdev->ops != NULL) && (vdev->ops->init != NULL)) {
			ret = vdev->ops->init(vdev);
			if (ret != 0) {
				pr_err("vdev->ops->init failed!");
			}
		}
	}

	register_io_emulation_handler(vm, &pci_cfg_range,
		&pci_cfg_io_read, &pci_cfg_io_write);
}

void vpci_cleanup(struct vm *vm)
{
	struct vpci_vdev_array *vdev_array;
	struct pci_vdev *vdev;
	int i;
	int ret;

	vdev_array = vm->vm_desc->vpci_vdev_array;

	for (i = 0; i < vdev_array->num_pci_vdev; i++) {
		vdev = &vdev_array->vpci_vdev_list[i];
		if ((vdev->ops != NULL) && (vdev->ops->deinit != NULL)) {
			ret = vdev->ops->deinit(vdev);
			if (ret != 0) {
				pr_err("vdev->ops->deinit failed!");
			}
		}
	}
}
