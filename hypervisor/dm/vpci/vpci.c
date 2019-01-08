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
#include "pci_priv.h"

static void pci_cfg_clear_cache(struct pci_addr_info *pi)
{
	pi->cached_bdf.value = 0xFFFFU;
	pi->cached_reg = 0U;
	pi->cached_enable = false;
}

static uint32_t pci_cfgaddr_io_read(struct acrn_vm *vm, uint16_t addr, size_t bytes)
{
	uint32_t val = ~0U;
	struct acrn_vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		val = (uint32_t)pi->cached_bdf.value;
		val <<= 8U;
		val |= pi->cached_reg;
		if (pi->cached_enable) {
			val |= PCI_CFG_ENABLE;
		}
	}

	return val;
}

static void pci_cfgaddr_io_write(struct acrn_vm *vm, uint16_t addr, size_t bytes, uint32_t val)
{
	struct acrn_vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		pi->cached_bdf.value = (uint16_t)(val >> 8U);
		pi->cached_reg = val & PCI_REGMAX;
		pi->cached_enable = ((val & PCI_CFG_ENABLE) == PCI_CFG_ENABLE);
	}
}

static uint32_t pci_cfgdata_io_read(struct acrn_vm *vm, uint16_t addr, size_t bytes)
{
	struct acrn_vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;
	uint16_t offset = addr - PCI_CONFIG_DATA;
	uint32_t val = ~0U;

	if (pi->cached_enable) {
		if ((vpci->ops != NULL) && (vpci->ops->cfgread != NULL)) {
			vpci->ops->cfgread(vpci, pi->cached_bdf, pi->cached_reg + offset, bytes, &val);
		}
		pci_cfg_clear_cache(pi);
	}

	return val;
}

static void pci_cfgdata_io_write(struct acrn_vm *vm, uint16_t addr, size_t bytes, uint32_t val)
{
	struct acrn_vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;
	uint16_t offset = addr - PCI_CONFIG_DATA;

	if (pi->cached_enable) {
		if ((vpci->ops != NULL) && (vpci->ops->cfgwrite != NULL)) {
			vpci->ops->cfgwrite(vpci, pi->cached_bdf, pi->cached_reg + offset, bytes, val);
		}
		pci_cfg_clear_cache(pi);
	}
}

void vpci_init(struct acrn_vm *vm)
{
	struct acrn_vpci *vpci = &vm->vpci;

	struct vm_io_range pci_cfgaddr_range = {
		.flags = IO_ATTR_RW,
		.base = PCI_CONFIG_ADDR,
		.len = 1U
	};

	struct vm_io_range pci_cfgdata_range = {
		.flags = IO_ATTR_RW,
		.base = PCI_CONFIG_DATA,
		.len = 4U
	};

	vpci->vm = vm;

#ifdef CONFIG_PARTITION_MODE
	vpci->ops = &partition_mode_vpci_ops;
#else
	vpci->ops = &sharing_mode_vpci_ops;
#endif

	if ((vpci->ops->init != NULL) && (vpci->ops->init(vm) == 0)) {
		/*
		 * SOS: intercep port CF8 only.
		 * UOS or partition mode: register handler for CF8 only and I/O requests to CF9/CFA/CFB are
		 *      not handled by vpci.
		 */
		register_pio_emulation_handler(vm, PCI_CFGADDR_PIO_IDX, &pci_cfgaddr_range,
			pci_cfgaddr_io_read, pci_cfgaddr_io_write);

		/* Intercept and handle I/O ports CFC -- CFF */
		register_pio_emulation_handler(vm, PCI_CFGDATA_PIO_IDX, &pci_cfgdata_range,
			pci_cfgdata_io_read, pci_cfgdata_io_write);
	}
}

void vpci_cleanup(struct acrn_vm *vm)
{
	struct acrn_vpci *vpci = &vm->vpci;

	if ((vpci->ops != NULL) && (vpci->ops->deinit != NULL)) {
		vpci->ops->deinit(vm);
	}
}
