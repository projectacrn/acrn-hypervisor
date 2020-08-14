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
#include <list.h>

#define VDEV_LIST_HASHBITS 4U
#define VDEV_LIST_HASHSIZE (1U << VDEV_LIST_HASHBITS)

struct pci_vbar {
	enum pci_bar_type type;
	uint64_t size;		/* BAR size */
	uint64_t base_gpa;	/* BAR guest physical address */
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
	bool      is_vmsix_on_msi;
	bool	  is_vmsix_on_msi_programmed;
};

/* SRIOV capability structure */
struct pci_cap_sriov {
	uint32_t  capoff;
	uint32_t  caplen;

	/*
	 * If the vdev is a SRIOV PF vdev, the vbars is used to store
	 * the bar information that is using to initialize SRIOV VF vdev bar.
	 */
	struct pci_vbar vbars[PCI_BAR_COUNT];
};

union pci_cfgdata {
	uint8_t data_8[PCIE_CONFIG_SPACE_SIZE];
	uint16_t data_16[PCIE_CONFIG_SPACE_SIZE >> 1U];
	uint32_t data_32[PCIE_CONFIG_SPACE_SIZE >> 2U];
};

struct pci_vdev;
struct pci_vdev_ops {
       void    (*init_vdev)(struct pci_vdev *vdev);
       void    (*deinit_vdev)(struct pci_vdev *vdev);
       int32_t (*write_vdev_cfg)(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val);
       int32_t (*read_vdev_cfg)(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val);
};

struct pci_vdev {
	struct acrn_vpci *vpci;
	/* The bus/device/function triple of the virtual PCI device. */
	union pci_bdf bdf;

	struct pci_pdev *pdev;

	union pci_cfgdata cfgdata;

	uint32_t flags;

	/* The bar info of the virtual PCI device. */
	uint32_t nr_bars; /* 6 for normal device, 2 for bridge, 1 for cardbus */
	struct pci_vbar vbars[PCI_BAR_COUNT];

	struct pci_msi msi;
	struct pci_msix msix;
	struct pci_cap_sriov sriov;

	/* Pointer to the SRIOV VF associated PF's vdev */
	struct pci_vdev *phyfun;

	/* Pointer to corresponding PCI device's vm_config */
	struct acrn_vm_pci_dev_config *pci_dev_config;

	/* Pointer to corressponding operations */
	const struct pci_vdev_ops *vdev_ops;

	/*
	 * vdev in    |   HV       |   pre-VM       |               SOS                   | post-VM
	 *            |            |                |vdev used by SOS|vdev used by post-VM|
	 * -----------------------------------------------------------------------------------------------
	 * parent_user| NULL(HV)   |   NULL(HV)     |   NULL(HV)     |   NULL(HV)         | vdev in SOS
	 * -----------------------------------------------------------------------------------------------
	 * user       | vdev in HV | vdev in pre-VM |   vdev in SOS  |   vdev in post-VM  | vdev in post-VM
	 */
	struct pci_vdev *parent_user;
	struct pci_vdev *user;	/* NULL means this device is not used or is a zombie VF */
	struct hlist_node link;
	void *priv_data;
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
	union pci_cfg_addr_reg addr;
	uint64_t pci_mmcfg_base;
	uint32_t pci_vdev_cnt;
	struct pci_vdev pci_vdevs[CONFIG_MAX_PCI_DEV_NUM];
	struct hlist_head vdevs_hlist_heads [VDEV_LIST_HASHSIZE];
};

struct acrn_vm;

extern const struct pci_vdev_ops vhostbridge_ops;
extern const struct pci_vdev_ops vpci_bridge_ops;
void init_vpci(struct acrn_vm *vm);
void deinit_vpci(struct acrn_vm *vm);
struct pci_vdev *pci_find_vdev(struct acrn_vpci *vpci, union pci_bdf vbdf);
struct acrn_assign_pcidev;
int32_t vpci_assign_pcidev(struct acrn_vm *tgt_vm, struct acrn_assign_pcidev *pcidev);
int32_t vpci_deassign_pcidev(struct acrn_vm *tgt_vm, struct acrn_assign_pcidev *pcidev);
struct pci_vdev *vpci_init_vdev(struct acrn_vpci *vpci, struct acrn_vm_pci_dev_config *dev_config, struct pci_vdev *parent_pf_vdev);

#endif /* VPCI_H_ */
