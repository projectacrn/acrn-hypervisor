/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
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
 *
 * Only support Type 0 and Type 1 PCI(e) device. Remove PC-Card type support.
 *
 */
#include <types.h>
#include <spinlock.h>
#include <io.h>
#include <pci.h>
#include <uart16550.h>
#include <logmsg.h>
#include <pci_dev.h>
#include <vtd.h>
#include <bits.h>
#include <board.h>
#include <platform_acpi_info.h>
#include <timer.h>

static spinlock_t pci_device_lock;
static uint32_t num_pci_pdev;
static struct pci_pdev pci_pdev_array[CONFIG_MAX_PCI_DEV_NUM];
static uint64_t pci_mmcfg_base = DEFAULT_PCI_MMCFG_BASE;

static void init_pdev(uint16_t pbdf, uint32_t drhd_index);

#ifdef CONFIG_ACPI_PARSE_ENABLED
void set_mmcfg_base(uint64_t mmcfg_base)
{
	pci_mmcfg_base = mmcfg_base;
}
#endif

uint64_t get_mmcfg_base(void)
{
	return pci_mmcfg_base;
}

/* @brief: Find the DRHD index corresponding to a PCI device
 * Runs through the pci_pdev_array and returns the value in drhd_idx
 * member from pdev structure that matches matches B:D.F
 *
 * @pbdf[in]	B:D.F of a PCI device
 *
 * @return if there is a matching pbdf in pci_pdev_array, pdev->drhd_idx, else INVALID_DRHD_INDEX
 */

uint32_t pci_lookup_drhd_for_pbdf(uint16_t pbdf)
{
	uint32_t drhd_index = INVALID_DRHD_INDEX;
	uint32_t index;

	for (index = 0U; index < num_pci_pdev; index++) {
		if (pci_pdev_array[index].bdf.value == pbdf) {
			drhd_index = pci_pdev_array[index].drhd_index;
			break;
		}
	}

	return drhd_index;
}

static uint32_t pci_pdev_calc_address(union pci_bdf bdf, uint32_t offset)
{
	uint32_t addr = (uint32_t)bdf.value;

	addr <<= 8U;
	addr |= (offset | PCI_CFG_ENABLE);
	return addr;
}

uint32_t pci_pdev_read_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes)
{
	uint32_t addr;
	uint32_t val;

	addr = pci_pdev_calc_address(bdf, offset);

	spinlock_obtain(&pci_device_lock);

	/* Write address to ADDRESS register */
	pio_write32(addr, (uint16_t)PCI_CONFIG_ADDR);

	/* Read result from DATA register */
	switch (bytes) {
	case 1U:
		val = (uint32_t)pio_read8((uint16_t)PCI_CONFIG_DATA + ((uint16_t)offset & 3U));
		break;
	case 2U:
		val = (uint32_t)pio_read16((uint16_t)PCI_CONFIG_DATA + ((uint16_t)offset & 2U));
		break;
	default:
		val = pio_read32((uint16_t)PCI_CONFIG_DATA);
		break;
	}
	spinlock_release(&pci_device_lock);

	return val;
}

void pci_pdev_write_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val)
{
	uint32_t addr;

	spinlock_obtain(&pci_device_lock);

	addr = pci_pdev_calc_address(bdf, offset);

	/* Write address to ADDRESS register */
	pio_write32(addr, (uint16_t)PCI_CONFIG_ADDR);

	/* Write value to DATA register */
	switch (bytes) {
	case 1U:
		pio_write8((uint8_t)val, (uint16_t)PCI_CONFIG_DATA + ((uint16_t)offset & 3U));
		break;
	case 2U:
		pio_write16((uint16_t)val, (uint16_t)PCI_CONFIG_DATA + ((uint16_t)offset & 2U));
		break;
	default:
		pio_write32(val, (uint16_t)PCI_CONFIG_DATA);
		break;
	}
	spinlock_release(&pci_device_lock);
}

/* enable: 1: enable INTx; 0: Disable INTx */
void enable_disable_pci_intx(union pci_bdf bdf, bool enable)
{
	uint32_t cmd, new_cmd;

	/* Set or clear the INTXDIS bit in COMMAND register */
	cmd = pci_pdev_read_cfg(bdf, PCIR_COMMAND, 2U);
	if (enable) {
		new_cmd = cmd & ~PCIM_CMD_INTxDIS;
	} else {
		new_cmd = cmd | PCIM_CMD_INTxDIS;
	}

	if ((cmd ^ new_cmd) != 0U) {
		pci_pdev_write_cfg(bdf, PCIR_COMMAND, 0x2U, new_cmd);
	}
}

static bool is_hidden_pdev(union pci_bdf pbdf)
{
	bool hidden = false;
	/* if it is debug uart, hide it*/
	if (is_pci_dbg_uart(pbdf)) {
		pr_info("hide pci uart dev: (%x:%x:%x)", pbdf.bits.b, pbdf.bits.d, pbdf.bits.f);
		hidden = true;
	}

	return hidden;
}

static void pci_init_pdev(union pci_bdf pbdf, uint32_t drhd_index)
{
	if (!is_hidden_pdev(pbdf)) {
		init_pdev(pbdf.value, drhd_index);
	}
}

/*
 * quantity of uint64_t to encode a bitmap of all bus values
 * TODO: PCI_BUSMAX is a good candidate to move to
 * generated platform files.
 */
#define BUSES_BITMAP_LEN        ((PCI_BUSMAX + 1U) >> 6U)

/*
 * must be >= total Endpoints in all DRDH devscope
 * TODO: BDF_SET_LEN is a good candidate to move to
 * generated platform files.
 */
#define BDF_SET_LEN			32U

struct pci_bdf_to_iommu {
	union pci_bdf dev_scope_bdf;
	uint32_t dev_scope_drhd_index;
};

struct pci_bdf_set {
	uint32_t pci_bdf_map_count;
	struct pci_bdf_to_iommu bdf_map[BDF_SET_LEN];
};

struct pci_bus_set {
	uint8_t bus_under_scan;
	uint32_t bus_drhd_index;
};

static uint32_t pci_check_override_drhd_index(union pci_bdf pbdf,
						const struct pci_bdf_set *const bdfs_from_drhds,
						uint32_t current_drhd_index)
{
	uint16_t bdfi;
	uint32_t bdf_drhd_index = current_drhd_index;

	for (bdfi = 0U; bdfi < bdfs_from_drhds->pci_bdf_map_count; bdfi++) {
		if (bdfs_from_drhds->bdf_map[bdfi].dev_scope_bdf.value == pbdf.value) {
			/*
			 * Override current_drhd_index
			 */
				bdf_drhd_index =
					bdfs_from_drhds->bdf_map[bdfi].dev_scope_drhd_index;
			}
	}

	return bdf_drhd_index;
}

/* Scan part of PCI hierarchy, starting with the given bus. */
static void init_pci_hierarchy(uint8_t bus, uint64_t buses_visited[BUSES_BITMAP_LEN],
				const struct pci_bdf_set *const bdfs_from_drhds, uint32_t drhd_index)
{
	bool is_mfdev;
	uint32_t vendor;
	uint8_t hdr_type, dev, func;
	union pci_bdf pbdf;
	uint8_t current_bus_index;
	uint32_t current_drhd_index, bdf_drhd_index;

	struct pci_bus_set bus_map[PCI_BUSMAX + 1U]; /* FIFO queue of buses to walk */
	uint32_t s = 0U, e = 0U; /* start and end index into queue */

	bus_map[e].bus_under_scan = bus;
	bus_map[e].bus_drhd_index = drhd_index;
	e = e + 1U;
	while (s < e) {
		current_bus_index = bus_map[s].bus_under_scan;
		current_drhd_index = bus_map[s].bus_drhd_index;
		s = s + 1U;

		bitmap_set_nolock(current_bus_index,
					&buses_visited[current_bus_index >> 6U]);

		pbdf.bits.b = current_bus_index;
		for (dev = 0U; dev <= PCI_SLOTMAX; dev++) {
			pbdf.bits.d = dev;
			is_mfdev = false;

			for (func = 0U; func <= PCI_FUNCMAX; func++) {

				pbdf.bits.f = func;

				vendor = read_pci_pdev_cfg_vendor(pbdf);
				hdr_type = read_pci_pdev_cfg_headertype(pbdf);

				if (func == 0U) {
					is_mfdev = is_pci_cfg_multifunction(hdr_type);
				}

				/* Do not probe beyond function 0 if not a multi-function device
				 * TODO unless device supports ARI or SR-IOV
				 * (PCIe spec r5.0 §7.5.1.1.9)
				 */
				if (((func == 0U) && !is_pci_vendor_valid(vendor)) ||
						((func > 0U) && (!is_mfdev))) {
					break;
				}

				if (!is_pci_vendor_valid(vendor)) {
					continue;
				}

				bdf_drhd_index = pci_check_override_drhd_index(pbdf, bdfs_from_drhds,
									current_drhd_index);
				pci_init_pdev(pbdf, bdf_drhd_index);

				if (is_pci_cfg_bridge(hdr_type)) {
					bus_map[e].bus_under_scan = read_pci_pdev_cfg_secbus(pbdf);
					bus_map[e].bus_drhd_index = bdf_drhd_index;
					e = e + 1U;
				}
			}
		}
	}
}

/*
 * @brief: Setup bdfs_from_drhds data structure as the DMAR tables are walked searching
 * for PCI device scopes. bdfs_from_drhds is used later in init_pci_hierarchy
 * to map the right DRHD unit to the PCI device
 */
static void pci_add_bdf_from_drhd(union pci_bdf bdf, struct pci_bdf_set *const bdfs_from_drhds,
					uint32_t drhd_index)
{
	if (bdfs_from_drhds->pci_bdf_map_count < BDF_SET_LEN) {
		bdfs_from_drhds->bdf_map[bdfs_from_drhds->pci_bdf_map_count].dev_scope_bdf = bdf;
		bdfs_from_drhds->bdf_map[bdfs_from_drhds->pci_bdf_map_count].dev_scope_drhd_index = drhd_index;
		bdfs_from_drhds->pci_bdf_map_count++;
	} else {
		ASSERT(bdfs_from_drhds->pci_bdf_map_count < BDF_SET_LEN,
				"Compare value in BDF_SET_LEN against those in ACPI DMAR tables");
	}
}


/*
 * @brief: Setup bdfs_from_drhds data structure as the DMAR tables are walked searching
 * for PCI device scopes. bdfs_from_drhds is used later in init_pci_hierarchy
 * to map the right DRHD unit to the PCI device.
 * TODO: bdfs_from_drhds is a good candidate to be part of generated platform
 * info.
 */
static void pci_parse_iommu_devscopes(struct pci_bdf_set *const bdfs_from_drhds,
						uint32_t *drhd_idx_pci_all)
{
	union pci_bdf bdf;
	uint32_t drhd_index, devscope_index;

	for (drhd_index = 0U; drhd_index < plat_dmar_info.drhd_count; drhd_index++) {
		for (devscope_index = 0U; devscope_index < plat_dmar_info.drhd_units[drhd_index].dev_cnt;
						devscope_index++) {
			bdf.fields.bus = plat_dmar_info.drhd_units[drhd_index].devices[devscope_index].bus;
			bdf.fields.devfun = plat_dmar_info.drhd_units[drhd_index].devices[devscope_index].devfun;

			if ((plat_dmar_info.drhd_units[drhd_index].devices[devscope_index].type ==
						ACPI_DMAR_SCOPE_TYPE_ENDPOINT) ||
				(plat_dmar_info.drhd_units[drhd_index].devices[devscope_index].type ==
						ACPI_DMAR_SCOPE_TYPE_BRIDGE)) {
				pci_add_bdf_from_drhd(bdf, bdfs_from_drhds, drhd_index);
			} else {
				/*
				 * Do nothing for IOAPIC, ACPI namespace and
				 * MSI Capable HPET device scope
				 */
			}
		}
	}

	if ((plat_dmar_info.drhd_units[plat_dmar_info.drhd_count - 1U].flags & DRHD_FLAG_INCLUDE_PCI_ALL_MASK)
			== DRHD_FLAG_INCLUDE_PCI_ALL_MASK) {
		*drhd_idx_pci_all = plat_dmar_info.drhd_count - 1U;
	}
}

/*
 * @brief Walks the PCI heirarchy and initializes array of pci_pdev structs
 * Uses DRHD info from ACPI DMAR tables to cover the endpoints and
 * bridges along with their hierarchy captured in the device scope entries
 * Walks through rest of the devices starting at bus 0 and thru PCI_BUSMAX
 */
void init_pci_pdev_list(void)
{
	uint64_t buses_visited[BUSES_BITMAP_LEN] = {0UL};
	struct pci_bdf_set bdfs_from_drhds;
	uint32_t drhd_idx_pci_all = INVALID_DRHD_INDEX;
	uint16_t bus;
	bool was_visited = false;

	pci_parse_iommu_devscopes(&bdfs_from_drhds, &drhd_idx_pci_all);

	/* TODO: iterate over list of PCI Host Bridges found in ACPI namespace */
	for (bus = 0U; bus <= PCI_BUSMAX; bus++) {
		was_visited = bitmap_test((bus & 0x3FU), &buses_visited[bus >> 6U]);
		if (!was_visited) {
			init_pci_hierarchy((uint8_t)bus, buses_visited, &bdfs_from_drhds, drhd_idx_pci_all);
		}
	}
}

static inline uint32_t pci_pdev_get_nr_bars(uint8_t hdr_type)
{
	uint32_t nr_bars = 0U;

	switch (hdr_type) {
	case PCIM_HDRTYPE_NORMAL:
		nr_bars = PCI_STD_NUM_BARS;
		break;

	case PCIM_HDRTYPE_BRIDGE:
		nr_bars = 2U;
		break;

	default:
		/*no actions are required for other cases.*/
		break;
	}

	return nr_bars;
}

/*
 * @pre pdev != NULL
 */
static void pci_read_cap(struct pci_pdev *pdev)
{
	uint8_t ptr, cap;
	uint32_t msgctrl;
	uint32_t len, offset, idx;
	uint32_t table_info;
	uint32_t pcie_devcap, val;

	ptr = (uint8_t)pci_pdev_read_cfg(pdev->bdf, PCIR_CAP_PTR, 1U);

	while ((ptr != 0U) && (ptr != 0xFFU)) {
		cap = (uint8_t)pci_pdev_read_cfg(pdev->bdf, ptr + PCICAP_ID, 1U);

		/* Ignore all other Capability IDs for now */
		if ((cap == PCIY_MSI) || (cap == PCIY_MSIX) || (cap == PCIY_PCIE) || (cap == PCIY_AF)) {
			offset = ptr;
			if (cap == PCIY_MSI) {
				pdev->msi_capoff = offset;
			} else if (cap == PCIY_MSIX) {
				pdev->msix.capoff = offset;
				pdev->msix.caplen = MSIX_CAPLEN;
				len = pdev->msix.caplen;

				msgctrl = pci_pdev_read_cfg(pdev->bdf, pdev->msix.capoff + PCIR_MSIX_CTRL, 2U);

				/* Read Table Offset and Table BIR */
				table_info = pci_pdev_read_cfg(pdev->bdf, pdev->msix.capoff + PCIR_MSIX_TABLE, 4U);

				pdev->msix.table_bar = (uint8_t)(table_info & PCIM_MSIX_BIR_MASK);

				pdev->msix.table_offset = table_info & ~PCIM_MSIX_BIR_MASK;
				pdev->msix.table_count = (msgctrl & PCIM_MSIXCTRL_TABLE_SIZE) + 1U;

				ASSERT(pdev->msix.table_count <= CONFIG_MAX_MSIX_TABLE_NUM);

				/* Copy MSIX capability struct into buffer */
				for (idx = 0U; idx < len; idx++) {
					pdev->msix.cap[idx] = (uint8_t)pci_pdev_read_cfg(pdev->bdf, offset + idx, 1U);
				}
			} else if (cap == PCIY_PCIE) {
				/* PCI Express Capability */
				pdev->pcie_capoff = offset;
				pcie_devcap = pci_pdev_read_cfg(pdev->bdf, offset + PCIR_PCIE_DEVCAP, 4U);
				pdev->has_flr = ((pcie_devcap & PCIM_PCIE_FLRCAP) != 0U) ? true : false;
			} else {
				/* Conventional PCI Advanced Features Capability */
				pdev->af_capoff = offset;
				val = pci_pdev_read_cfg(pdev->bdf, offset, 4U);
				pdev->has_af_flr = ((val & PCIM_AF_FLR_CAP) != 0U) ? true : false;
			}
		}

		ptr = (uint8_t)pci_pdev_read_cfg(pdev->bdf, ptr + PCICAP_NEXTPTR, 1U);
	}
}

static void init_pdev(uint16_t pbdf, uint32_t drhd_index)
{
	uint8_t hdr_type;
	union pci_bdf bdf;
	struct pci_pdev *pdev;

	if (num_pci_pdev < CONFIG_MAX_PCI_DEV_NUM) {
		bdf.value = pbdf;
		hdr_type = (uint8_t)pci_pdev_read_cfg(bdf, PCIR_HDRTYPE, 1U);
		hdr_type &= PCIM_HDRTYPE;

		if ((hdr_type == PCIM_HDRTYPE_NORMAL) || (hdr_type == PCIM_HDRTYPE_BRIDGE)) {
			pdev = &pci_pdev_array[num_pci_pdev];
			pdev->bdf.value = pbdf;
			pdev->nr_bars = pci_pdev_get_nr_bars(hdr_type);

			if ((pci_pdev_read_cfg(bdf, PCIR_STATUS, 2U) & PCIM_STATUS_CAPPRESENT) != 0U) {
				pci_read_cap(pdev);
			}

			pdev->drhd_index = drhd_index;

			fill_pci_dev_config(pdev);

			num_pci_pdev++;
		} else {
			pr_err("%s, %x:%x.%x unsupported headed type: 0x%x\n",
				__func__, bdf.bits.b, bdf.bits.d, bdf.bits.f, hdr_type);
		}
	} else {
		pr_err("%s, failed to alloc pci_pdev!\n", __func__);
	}
}

void pdev_do_flr(union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val)
{
	uint32_t idx;
	uint32_t bars[PCI_STD_NUM_BARS];

	for (idx = 0U; idx < PCI_STD_NUM_BARS; idx++) {
		bars[idx] = pci_pdev_read_cfg(bdf, pci_bar_offset(idx), 4U);
	}

	/* do the real reset */
	pci_pdev_write_cfg(bdf, offset, bytes, val);

	/*
	 * After an FLR has been initiated by writing a 1b to
	 * the Initiate Function Level Reset bit,
	 * the Function must complete the FLR within 100 ms
	 */
	msleep(100U);

	for (idx = 0U; idx < PCI_STD_NUM_BARS; idx++) {
		pci_pdev_write_cfg(bdf, pci_bar_offset(idx), 4U, bars[idx]);
	}
}
