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

#ifndef VPCI_H_
#define VPCI_H_

#include <spinlock.h>
#include <pci.h>


struct pci_bar {
	enum pci_bar_type type;
	uint64_t size;		/* BAR size */
	uint64_t base;		/* BAR guest physical address */
	uint64_t base_hpa;	/* BAR host physical address */
	uint32_t fixed;		/* BAR fix memory type encoding */
	uint32_t mask;		/* BAR size mask */
};

struct msix_table_entry {
	uint64_t	addr;
	uint32_t	data;
	uint32_t	vector_control;
};

/* MSI capability structure */
struct pci_msi {
	bool      is_64bit;
	uint32_t  capoff;
	uint32_t  caplen;
};

/* MSI-X capability structure */
struct pci_msix {
	struct msix_table_entry table_entries[CONFIG_MAX_MSIX_TABLE_NUM];
	uint64_t  mmio_gpa;
	uint64_t  mmio_hpa;
	uint64_t  mmio_size;
	uint32_t  capoff;
	uint32_t  caplen;
	uint32_t  table_bar;
	uint32_t  table_offset;
	uint32_t  table_count;
};

union pci_cfgdata {
	uint8_t data_8[PCI_REGMAX + 1U];
	uint16_t data_16[(PCI_REGMAX + 1U) >> 1U];
	uint32_t data_32[(PCI_REGMAX + 1U) >> 2U];
};

struct pci_vdev;
struct pci_vdev_ops {
       void    (*init_vdev)(struct pci_vdev *vdev);
       void    (*deinit_vdev)(struct pci_vdev *vdev);
       int32_t (*write_vdev_cfg)(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
       int32_t (*read_vdev_cfg)(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val);
};

struct pci_vdev {
	const struct acrn_vpci *vpci;
	/* The bus/device/function triple of the virtual PCI device. */
	union pci_bdf bdf;

	struct pci_pdev *pdev;

	union pci_cfgdata cfgdata;

	/* The bar info of the virtual PCI device. */
	uint32_t nr_bars; /* 6 for normal device, 2 for bridge, 1 for cardbus */
	struct pci_bar bar[PCI_BAR_COUNT];

	struct pci_msi msi;
	struct pci_msix msix;

	bool has_flr;
	uint32_t pcie_capoff;

	/* Pointer to corresponding PCI device's vm_config */
	struct acrn_vm_pci_dev_config *pci_dev_config;

	/* Pointer to corressponding operations */
	const struct pci_vdev_ops *vdev_ops;

	/* For SOS, if the device is latterly assigned to a UOS, we use this field to track the new owner. */
	struct pci_vdev *new_owner;
};

union pci_cfg_addr_reg {
	uint32_t value;
	struct {
		uint32_t reg_num : 8;	/* BITs 0-7, Register Number (BITs 0-1, always reserve to 0) */
		uint32_t bdf : 16;	/* BITs 8-23, BDF Number */
		uint32_t resv : 7;	/* BITs 24-30, Reserved */
		uint32_t enable : 1;	/* BITs 31, Enable bit */
	} bits;
};

struct acrn_vpci {
	spinlock_t lock;
	struct acrn_vm *vm;
	union pci_cfg_addr_reg addr;
	uint32_t pci_vdev_cnt;
	struct pci_vdev pci_vdevs[CONFIG_MAX_PCI_DEV_NUM];
};

extern const struct pci_vdev_ops vhostbridge_ops;
void vpci_init(struct acrn_vm *vm);
void vpci_cleanup(struct acrn_vm *vm);
void vpci_set_ptdev_intr_info(struct acrn_vm *target_vm, uint16_t vbdf, uint16_t pbdf);
void vpci_reset_ptdev_intr_info(struct acrn_vm *target_vm, uint16_t vbdf, uint16_t pbdf);

#endif /* VPCI_H_ */
