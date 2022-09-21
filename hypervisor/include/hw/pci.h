/*-
* Copyright (c) 1997, Stefan Esser <se@freebsd.org>
* Copyright (c) 2011 NetApp, Inc.
* Copyright (c) 2018-2022 Intel Corporation.
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

#ifndef PCI_H_
#define PCI_H_

#include <list.h>

/*
 * PCIM_xxx: mask to locate subfield in register
 * PCIR_xxx: config register offset
 * PCIC_xxx: device class
 * PCIS_xxx: device subclass
 * PCIP_xxx: device programming interface
 * PCIV_xxx: PCI vendor ID (only required to fixup ancient devices)
 * PCID_xxx: device ID
 * PCIY_xxx: capability identification number
 * PCIZ_xxx: extended capability identification number
 */

#define PCI_CFG_HEADER_LENGTH 0x40U

/* some PCI bus constants */
#define PCI_BUSMAX            0xFFU
#define PCI_SLOTMAX           0x1FU
#define PCI_FUNCMAX           0x7U
#define PCI_BAR_COUNT         0x6U
#define PCI_REGMASK           0xFCU

#define PCI_CONFIG_SPACE_SIZE 0x100U
#define PCIE_CONFIG_SPACE_SIZE 0x1000U
#define PCI_MMCONFIG_SIZE     0x10000000U

/* I/O ports */
#define PCI_CONFIG_ADDR       0xCF8U
#define PCI_CONFIG_DATA       0xCFCU

#define PCI_CFG_ENABLE        0x80000000U

/* PCI config header registers for all devices */
#define PCIR_VENDOR           0x00U
#define PCIR_DEVICE           0x02U
#define PCIR_COMMAND          0x04U
#define	PCIM_CMD_PORTEN       0x01U
#define	PCIM_CMD_MEMEN        0x02U
#define PCIM_CMD_INTxDIS      0x400U
#define PCIR_STATUS           0x06U
#define PCIM_STATUS_CAPPRESENT    0x0010U
#define PCIR_REVID            0x08U
#define PCIR_CLASS_CODE	      0x09U
#define PCIR_SUBCLASS         0x0AU
#define PCIR_CLASS            0x0BU
#define PCIR_HDRTYPE          0x0EU
#define PCIM_HDRTYPE          0x7FU
#define PCIM_HDRTYPE_NORMAL   0x00U
#define PCIM_HDRTYPE_BRIDGE   0x01U
#define	PCIM_HDRTYPE_CARDBUS  0x02U
#define PCIM_MFDEV            0x80U
#define PCIR_BARS             0x10U
#define PCIM_BAR_SPACE        0x01U
#define PCIM_BAR_IO_SPACE     0x01U
#define PCIM_BAR_MEM_TYPE     0x06U
#define PCIM_BAR_MEM_32       0x00U
#define PCIM_BAR_MEM_1MB      0x02U
#define PCIM_BAR_MEM_64       0x04U
#define PCIM_BAR_MEM_BASE     0xFFFFFFF0U
#define PCIV_SUB_VENDOR_ID    0x2CU
#define PCIV_SUB_SYSTEM_ID    0x2EU
#define PCIR_BIOS	      0x30U
#define PCIR_CAP_PTR          0x34U
#define PCIR_CAP_PTR_CARDBUS  0x14U
#define PCI_BASE_ADDRESS_MEM_MASK (~0x0fUL)
#define PCI_BASE_ADDRESS_IO_MASK  (~0x03UL)
#define PCIR_INTERRUPT_LINE   0x3cU
#define PCIR_INTERRUPT_PIN    0x3dU

#define PCIC_SIMPLECOMM       0x07U
/* config registers for header type 1 (PCI-to-PCI bridge) devices */
#define PCIR_PRIBUS_1         0x18U
#define PCIR_SECBUS_1         0x19U
#define PCIR_SUBBUS_1         0x1AU

/* Capability Register Offsets */
#define PCICAP_ID             0x0U
#define PCICAP_NEXTPTR        0x1U
#define PCICAP_EXP_CAP        0x2U

/* Capability Identification Numbers */
#define PCIY_MSI              0x05U
#define PCIY_MSIX             0x11U

/* PCIe Extended Capability*/
#define PCI_ECAP_BASE_PTR	0x100U
#define PCI_ECAP_ID(hdr)	((uint32_t)((hdr) & 0xFFFFU))
#define PCI_ECAP_NEXT(hdr)	((uint32_t)(((hdr) >> 20U) & 0xFFCU))
#define PCIZ_SRIOV		0x10U
#define PCIZ_PTM 		0x1fU

/* SRIOV Definitions */
#define PCI_SRIOV_CAP_LEN	0x40U
#define PCIR_SRIOV_CONTROL	0x8U
#define PCIR_SRIOV_TOTAL_VFS	0xEU
#define PCIR_SRIOV_NUMVFS	0x10U
#define PCIR_SRIOV_FST_VF_OFF	0x14U
#define PCIR_SRIOV_VF_STRIDE	0x16U
#define PCIR_SRIOV_VF_DEV_ID	0x1AU
#define PCIR_SRIOV_VF_BAR_OFF	0x24U
#define PCIM_SRIOV_VF_ENABLE	0x1U

/* PTM Definitions */
#define PCI_PTM_CAP_LEN				0x04U
#define PCIR_PTM_CAP				0x04U
#define PCIM_PTM_CAP_ROOT_CAPABLE	0x4U
#define PCIM_PTM_GRANULARITY_MASK	0xFF00U
#define PCIR_PTM_CTRL				0x08U
#define PCIM_PTM_CTRL_ENABLED		0x1U
#define PCIM_PTM_CTRL_ROOT_SELECTED	0x2U

/* PCI Message Signalled Interrupts (MSI) */
#define PCIR_MSI_CTRL         0x02U
#define PCIM_MSICTRL_64BIT    0x80U
#define PCIM_MSICTRL_MSI_ENABLE  0x01U
#define PCIR_MSI_ADDR         0x4U
#define PCIR_MSI_ADDR_HIGH    0x8U
#define PCIR_MSI_DATA         0x8U
#define PCIR_MSI_DATA_64BIT   0xCU
#define PCIR_MSI_MASK         0x10U
#define PCIM_MSICTRL_MMC_MASK 0x000EU
#define PCIM_MSICTRL_MME_MASK 0x0070U

/* PCI device class */
#define PCIC_BRIDGE           0x06U
#define PCIS_BRIDGE_HOST      0x00U

/* PCI device subclass */
#define PCIS_BRIDGE_PCI       0x04U

/* MSI-X definitions */
#define PCIR_MSIX_CTRL        0x2U
#define PCIR_MSIX_TABLE       0x4U
#define PCIR_MSIX_PBA         0x8U

#define PCIM_MSIXCTRL_MSIX_ENABLE    0x8000U
#define PCIM_MSIXCTRL_FUNCTION_MASK  0x4000U
#define PCIM_MSIXCTRL_TABLE_SIZE     0x07FFU
#define PCIM_MSIX_BIR_MASK    0x7U
#define PCIM_MSIX_VCTRL_MASK  0x1U

#define MSIX_CAPLEN           12U
#define MSIX_TABLE_ENTRY_SIZE 16U

/* PCI Power Management Capability */
#define PCIY_PMC              0x01U
/* Power Management Control/Status Register */
#define PCIR_PMCSR            0x04U
#define PCIM_PMCSR_NO_SOFT_RST (0x1U << 3U)

/* PCI Express Capability */
#define PCIY_PCIE             0x10U
#define PCIR_PCIE_DEVCAP      0x04U
#define PCIR_PCIE_DEVCTRL     0x08U
#define PCIM_PCIE_DEV_CTRL_MAX_PAYLOAD    0x00E0U
#define PCIM_PCIE_FLRCAP      (0x1U << 28U)
#define PCIM_PCIE_FLR         (0x1U << 15U)

/* PCI Express Device Type definitions */
#define PCIER_FLAGS                    0x2U
#define PCIEM_FLAGS_TYPE               0x00F0U
#define PCIEM_TYPE_ENDPOINT            0x0000U
#define PCIEM_TYPE_ROOTPORT            0x0004U
#define PCIEM_TYPE_ROOT_INT_EP         0x0009U

#define PCIR_PCIE_DEVCAP2     0x24U
#define PCIM_PCIE_DEVCAP2_ARI (0x1U << 5U)
#define PCIR_PCIE_DEVCTL2     0x28U
#define PCIM_PCIE_DEVCTL2_ARI (0x1U << 5U)

/* Conventional PCI Advanced Features Capability */
#define PCIY_AF               0x13U
#define PCIM_AF_FLR_CAP       (0x1U << 25U)
#define PCIR_AF_CTRL          0x4U
#define PCIM_AF_FLR           0x1U

#define PCI_STD_NUM_BARS        6U

union pci_bdf {
	uint16_t value;
	struct {
		uint8_t f : 3; /* BITs 0-2 */
		uint8_t d : 5; /* BITs 3-7 */
		uint8_t b; /* BITs 8-15 */
	} bits;
	struct {
		uint8_t devfun; /* BITs 0-7 */
		uint8_t bus;   /* BITs 8-15 */
	} fields;
};

/*
 * The next data structure is to reflect the format of PCI BAR base on the PCI sepc.
 */

union pci_bar_type {
	uint32_t bits;
	struct {
		uint32_t indicator :1;               /* BITs[0], mapped to I/O space if read as 1 */
		uint32_t reserved :1;               /* BITs[1], reserved and must be "0" per spec. */
		uint32_t reserved2 : 30;
	} io_space;
	struct {
		uint32_t indicator :1;               /* BITs[0], mapped to memory space if read as 0 */
		uint32_t mem_type :2;            /* BITs[1:2], 32-bit address if read as 00b, 64-bit address as 01b */
		uint32_t prefetchable :1;        /* BITs[3], set to 1b if the data is prefetchable and set to 0b otherwise */
		uint32_t reserved2 : 28;
	} mem_space;
};

struct pci_mmcfg_region {
	uint64_t address;	/* Base address, processor-relative */
	uint16_t pci_segment;	/* PCI segment group number */
	uint8_t start_bus;	/* Starting PCI Bus number */
	uint8_t end_bus;	/* Final PCI Bus number */
} __packed;

/* Basic MSIX capability info */
struct pci_msix_cap {
	uint32_t  capoff;
	uint32_t  caplen;
	uint8_t   table_bar;
	uint32_t  table_offset;
	uint32_t  table_count;
	uint8_t   cap[MSIX_CAPLEN];
};

struct pci_sriov_cap {
	uint32_t  capoff;
	uint32_t  caplen;
	uint32_t  pre_pos;
	bool hide_sriov;
};

/* PCI BAR size is detected at run time. We don't want to re-detect it to avoid malfunction of
 * the device. We have record physical bar values, we need to record size_mask.
 */
struct pci_bar_resource {
	uint32_t phy_bar;	/* the origional raw data read from physical BAR */
	uint32_t size_mask;	/* read value of physical BAR after write 0xffffffff */
};

struct pci_pdev {
	uint8_t hdr_type;
	uint8_t base_class;
	uint8_t sub_class;

	/* IOMMU responsible for DMA and Interrupt Remapping for this device */
	uint32_t drhd_index;
	/* Used for vMSI-x on MSI emulation */
	uint16_t irte_start;
	uint16_t irte_count;

	/* The bar info of the physical PCI device. */
	uint32_t nr_bars; /* 6 for normal device, 2 for bridge, 1 for cardbus */
	struct pci_bar_resource bars[PCI_STD_NUM_BARS];	/* For common bar resource recording */

	/* The bus/device/function triple of the physical PCI device. */
	union pci_bdf bdf;

	uint32_t msi_capoff;
	uint32_t pcie_capoff;

	struct pci_msix_cap msix;
	struct pci_sriov_cap sriov;

	bool has_pm_reset;
	bool has_flr;
	bool has_af_flr;
	struct hlist_node link;
};

struct pci_cfg_ops {
	uint32_t (*pci_read_cfg)(union pci_bdf bdf, uint32_t offset, uint32_t bytes);
	void (*pci_write_cfg)(union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val);
};

static inline bool is_host_bridge(const struct pci_pdev *pdev)
{
	return (pdev->base_class == PCIC_BRIDGE) && (pdev->sub_class == PCIS_BRIDGE_HOST);
}

static inline bool is_bridge(const struct pci_pdev *pdev)
{
	return ((pdev->hdr_type & PCIM_HDRTYPE) == PCIM_HDRTYPE_BRIDGE);
}

static inline uint32_t pci_bar_offset(uint32_t idx)
{
	return PCIR_BARS + (idx << 2U);
}

static inline uint32_t pci_bar_index(uint32_t offset)
{
	return (offset - PCIR_BARS) >> 2U;
}

static inline bool is_bar_offset(uint32_t nr_bars, uint32_t offset)
{
	bool ret;

	if ((offset >= pci_bar_offset(0U))
		&& (offset < pci_bar_offset(nr_bars))) {
		ret = true;
	} else {
	    ret = false;
	}

	return ret;
}

static inline bool bdf_is_equal(union pci_bdf a, union pci_bdf b)
{
	return (a.value == b.value);
}

static inline uint64_t get_pci_mmcfg_size(struct pci_mmcfg_region *pci_mmcfg)
{
	return 0x100000UL * (pci_mmcfg->end_bus - pci_mmcfg->start_bus + 1U);
}

#ifdef CONFIG_ACPI_PARSE_ENABLED
void set_mmcfg_region(struct pci_mmcfg_region *region);
#endif
struct pci_mmcfg_region *get_mmcfg_region(void);

struct pci_pdev *pci_init_pdev(union pci_bdf pbdf, uint32_t drhd_index);
uint32_t pci_pdev_read_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes);
void pci_pdev_write_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val);
void enable_disable_pci_intx(union pci_bdf bdf, bool enable);

bool is_hv_owned_pdev(union pci_bdf pbdf);
uint32_t get_hv_owned_pdev_num(void);
const struct pci_pdev **get_hv_owned_pdevs(void);
/*
 * @brief Walks the PCI heirarchy and initializes array of pci_pdev structs
 * Uses DRHD info from ACPI DMAR tables to cover the endpoints and
 * bridges along with their hierarchy captured in the device scope entries
 * Walks through rest of the devices starting at bus 0 and thru PCI_BUSMAX
 */
void init_pci_pdev_list(void);

/* @brief: Find the DRHD index corresponding to a PCI device
 * Runs through the pci_pdevs and returns the value in drhd_idx
 * member from pdev strucutre that matches matches B:D.F
 *
 * @pbdf[in]	B:D.F of a PCI device
 *
 * @return if there is a matching pbdf in pci_pdevs, pdev->drhd_idx, else -1U
 */
uint32_t pci_lookup_drhd_for_pbdf(uint16_t pbdf);

static inline bool is_pci_vendor_valid(uint32_t vendor_id)
{
	return !((vendor_id == 0xFFFFFFFFU) || (vendor_id == 0U) ||
		 (vendor_id == 0xFFFF0000U) || (vendor_id == 0xFFFFU));
}

static inline bool is_pci_cfg_multifunction(uint8_t header_type)
{
	return ((header_type != 0xffU) && ((header_type & PCIM_MFDEV) == PCIM_MFDEV));
}

static inline bool pci_is_valid_access_offset(uint32_t offset, uint32_t bytes)
{
	return ((offset & (bytes - 1U)) == 0U);
}

static inline bool pci_is_valid_access_byte(uint32_t bytes)
{
	return ((bytes == 1U) || (bytes == 2U) || (bytes == 4U));
}

static inline bool pci_is_valid_access(uint32_t offset, uint32_t bytes)
{
	return (pci_is_valid_access_byte(bytes) && pci_is_valid_access_offset(offset, bytes));
}

bool is_plat_hidden_pdev(union pci_bdf bdf);
bool pdev_need_bar_restore(const struct pci_pdev *pdev);
void pdev_restore_bar(const struct pci_pdev *pdev);
void pci_switch_to_mmio_cfg_ops(void);
void reserve_vmsix_on_msi_irtes(struct pci_pdev *pdev);

#endif /* PCI_H_ */
