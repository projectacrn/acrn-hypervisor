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
	return (addr >= PCI_CONFIG_ADDR) && (addr < (PCI_CONFIG_ADDR + 4));
}

static bool is_cfg_data(uint16_t addr)
{
	return (addr >= PCI_CONFIG_DATA) && (addr < (PCI_CONFIG_DATA + 4));
}

static void pci_cfg_clear_cache(struct pci_addr_info *pi)
{
	pi->cached_bdf = 0xffffU;
	pi->cached_reg = 0U;
	pi->cached_enable = 0U;
}

static uint32_t pci_cfg_io_read(__unused struct vm_io_handler *hdlr,
	struct vm *vm, uint16_t addr, size_t bytes)
{
	uint32_t val = 0xffffffffU;
	struct vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;

	if (is_cfg_addr(addr)) {
		/* TODO: handling the non 4 bytes access */
		if (bytes == 4U) {
			val = (PCI_BUS(pi->cached_bdf) << 16)
				| (PCI_SLOT(pi->cached_bdf) << 11)
				| (PCI_FUNC(pi->cached_bdf) << 8)
				| pi->cached_reg;

			if (pi->cached_enable) {
				val |= PCI_CFG_ENABLE;
			}
		}
	} else if (is_cfg_data(addr)) {
		if (pi->cached_enable) {
			uint16_t offset = addr - 0xcfc;

			pci_vdev_cfg_handler(&vm->vpci, 1U, pi->cached_bdf,
				pi->cached_reg + offset, bytes, &val);

			pci_cfg_clear_cache(pi);
		}
	}  else {
		val = 0xffffffffU;
	}

	return val;
}

static void pci_cfg_io_write(__unused struct vm_io_handler *hdlr,
	struct vm *vm, uint16_t addr, size_t bytes, uint32_t val)
{
	struct vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;

	if (is_cfg_addr(addr)) {
		/* TODO: handling the non 4 bytes access */
		if (bytes == 4U) {
			pi->cached_bdf = PCI_BDF(
				((val >> 16) & PCI_BUSMAX),
				((val >> 11) & PCI_SLOTMAX),
				((val >> 8) & PCI_FUNCMAX));

			pi->cached_reg = val & PCI_REGMAX;
			pi->cached_enable =
			(val & PCI_CFG_ENABLE) == PCI_CFG_ENABLE;
		}
	} else if (is_cfg_data(addr)) {
		if (pi->cached_enable) {
			uint16_t offset = addr - 0xcfc;

			pci_vdev_cfg_handler(&vm->vpci, 0U, pi->cached_bdf,
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
			if (ret) {
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
			if (ret) {
				pr_err("vdev->ops->deinit failed!");
			}
		}
	}
}

void pci_vdev_cfg_handler(struct vpci *vpci, uint32_t in, uint16_t vbdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	/* vm-exit handler */
}
