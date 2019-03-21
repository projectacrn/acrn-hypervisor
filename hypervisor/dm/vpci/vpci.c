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

#include <vm.h>
#include <errno.h>
#include <logmsg.h>
#include "pci_priv.h"

static void pci_cfg_clear_cache(struct pci_addr_info *pi)
{
	pi->cached_bdf.value = 0xFFFFU;
	pi->cached_reg = 0U;
	pi->cached_enable = false;
}

/**
 * @pre vm != NULL
 */
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

/**
 * @pre vm != NULL
 */
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

static inline bool vpci_is_valid_access_offset(uint32_t offset, uint32_t bytes)
{
	return ((offset & (bytes - 1U)) == 0U);
}

static inline bool vpci_is_valid_access_byte(uint32_t bytes)
{
	return ((bytes == 1U) || (bytes == 2U) || (bytes == 4U));
}

static inline bool vpci_is_valid_access(uint32_t offset, uint32_t bytes)
{
	return (vpci_is_valid_access_byte(bytes) && vpci_is_valid_access_offset(offset, bytes));
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vm->vm_id)->type == PRE_LAUNCHED_VM) || (get_vm_config(vm->vm_id)->type == SOS_VM)
 */
static uint32_t pci_cfgdata_io_read(struct acrn_vm *vm, uint16_t addr, size_t bytes)
{
	struct acrn_vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;
	uint16_t offset = addr - PCI_CONFIG_DATA;
	uint32_t val = ~0U;
	struct acrn_vm_config *vm_config;

	if (pi->cached_enable) {
		if (vpci_is_valid_access(pi->cached_reg + offset, bytes)) {
			vm_config = get_vm_config(vm->vm_id);

			switch (vm_config->type) {
			case PRE_LAUNCHED_VM:
				partition_mode_cfgread(vpci, pi->cached_bdf, pi->cached_reg + offset, bytes, &val);
				break;

			case SOS_VM:
				sharing_mode_cfgread(vpci, pi->cached_bdf, pi->cached_reg + offset, bytes, &val);
				break;

			default:
				ASSERT(false, "Error, pci_cfgdata_io_read should only be called for PRE_LAUNCHED_VM and SOS_VM");
				break;
			}
		}
		pci_cfg_clear_cache(pi);
	}

	return val;
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vm->vm_id)->type == PRE_LAUNCHED_VM) || (get_vm_config(vm->vm_id)->type == SOS_VM)
 */
static void pci_cfgdata_io_write(struct acrn_vm *vm, uint16_t addr, size_t bytes, uint32_t val)
{
	struct acrn_vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;
	uint16_t offset = addr - PCI_CONFIG_DATA;
	struct acrn_vm_config *vm_config;


	if (pi->cached_enable) {
		if (vpci_is_valid_access(pi->cached_reg + offset, bytes)) {
			vm_config = get_vm_config(vm->vm_id);

			switch (vm_config->type) {
			case PRE_LAUNCHED_VM:
				partition_mode_cfgwrite(vpci, pi->cached_bdf, pi->cached_reg + offset, bytes, val);
				break;

			case SOS_VM:
				sharing_mode_cfgwrite(vpci, pi->cached_bdf, pi->cached_reg + offset, bytes, val);
				break;

			default:
				ASSERT(false, "Error, pci_cfgdata_io_write should only be called for PRE_LAUNCHED_VM and SOS_VM");
				break;
			}
		}
		pci_cfg_clear_cache(pi);
	}
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void vpci_init(struct acrn_vm *vm)
{
	struct acrn_vpci *vpci = &vm->vpci;
	int32_t ret = -EINVAL;

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

	struct acrn_vm_config *vm_config;

	vpci->vm = vm;

	vm_config = get_vm_config(vm->vm_id);
	switch (vm_config->type) {
	case PRE_LAUNCHED_VM:
		ret = partition_mode_vpci_init(vm);
		break;

	case SOS_VM:
		ret = sharing_mode_vpci_init(vm);
		break;

	default:
		/* Nothing to do for other vm types */
		break;
	}

	if (ret == 0) {
		/*
		 * SOS: intercept port CF8 only.
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

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void vpci_cleanup(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vm->vm_id);
	switch (vm_config->type) {
	case PRE_LAUNCHED_VM:
		partition_mode_vpci_deinit(vm);
		break;

	case SOS_VM:
		sharing_mode_vpci_deinit(vm);
		break;

	default:
		/* Nothing to do for other vm types */
		break;
	}
}
