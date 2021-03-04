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
#include <x86/lib/spinlock.h>
#include <x86/io.h>
#include <x86/pgtable.h>
#include <x86/mmu.h>
#include <pci.h>
#include <uart16550.h>
#include <logmsg.h>
#include <x86/pci_dev.h>
#include <x86/vtd.h>
#include <x86/lib/bits.h>
#include <x86/board.h>
#include <platform_acpi_info.h>
#include <hash.h>
#include <util.h>

#define PDEV_HLIST_HASHBITS 6U
#define PDEV_HLIST_HASHSIZE (1U << PDEV_HLIST_HASHBITS)

static uint32_t num_pci_pdev;
static struct pci_pdev pci_pdevs[CONFIG_MAX_PCI_DEV_NUM];
static struct hlist_head pdevs_hlist_heads[PDEV_HLIST_HASHSIZE];

/* For HV owned pdev */
static uint32_t num_hv_owned_pci_pdev;
static struct pci_pdev *hv_owned_pci_pdevs[CONFIG_MAX_PCI_DEV_NUM];

uint32_t get_hv_owned_pdev_num(void)
{
	return num_hv_owned_pci_pdev;
}

const struct pci_pdev **get_hv_owned_pdevs(void)
{
	return (const struct pci_pdev **)hv_owned_pci_pdevs;
}

static struct pci_mmcfg_region phys_pci_mmcfg = {
	.address = DEFAULT_PCI_MMCFG_BASE,
	.start_bus = DEFAULT_PCI_MMCFG_START_BUS,
	.end_bus = DEFAULT_PCI_MMCFG_END_BUS,
};

#ifdef CONFIG_ACPI_PARSE_ENABLED
void set_mmcfg_region(struct pci_mmcfg_region *region)
{
	phys_pci_mmcfg = *region;
}
#endif

struct pci_mmcfg_region *get_mmcfg_region(void)
{
	return &phys_pci_mmcfg;
}

#if defined(HV_DEBUG)
static inline uint32_t pio_off_to_address(union pci_bdf bdf, uint32_t offset)

{
	uint32_t addr = (uint32_t)bdf.value;

	addr <<= 8U;
	addr |= (offset | PCI_CFG_ENABLE);
	return addr;
}

static uint32_t pci_pio_read_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes)
{
	uint32_t addr;
	uint32_t val;

	addr = pio_off_to_address(bdf, offset);

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

	return val;
}

static void pci_pio_write_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val)
{
	uint32_t addr = pio_off_to_address(bdf, offset);

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
}
#else
static uint32_t pci_pio_read_cfg(__unused union pci_bdf bdf,
		__unused uint32_t offset, __unused uint32_t bytes)
{
	return ~0U;
}

static void pci_pio_write_cfg(__unused union pci_bdf bdf,
		__unused uint32_t offset, __unused uint32_t bytes, __unused uint32_t val)
{
}
#endif

static const struct pci_cfg_ops pci_pio_cfg_ops = {
	.pci_read_cfg = pci_pio_read_cfg,
	.pci_write_cfg = pci_pio_write_cfg,
};

/*
 * @pre offset < 0x1000U
 * @pre phys_pci_mmcfg.address 4K-byte alignment
 */
static inline uint32_t mmcfg_off_to_address(union pci_bdf bdf, uint32_t offset)
{
	return (uint32_t)phys_pci_mmcfg.address + (((uint32_t)bdf.value << 12U) | offset);
}

/*
 * @pre bytes == 1U || bytes == 2U || bytes == 4U
 * @pre offset 1-byte  alignment if byte == 1U
 *             2-byte alignment if byte == 2U
 *             4-byte alignment if byte == 4U
 */
static uint32_t pci_mmcfg_read_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes)
{
	uint32_t addr = mmcfg_off_to_address(bdf, offset);
	void *hva = hpa2hva(addr);
	uint32_t val;


	ASSERT(pci_is_valid_access(offset, bytes), "the offset should be aligned with 2/4 byte\n");

	switch (bytes) {
	case 1U:
		val = (uint32_t)mmio_read8(hva);
		break;
	case 2U:
		val = (uint32_t)mmio_read16(hva);
		break;
	default:
		val = mmio_read32(hva);
		break;
	}

	return val;
}

/*
 * @pre bytes == 1U || bytes == 2U || bytes == 4U
 * @pre offset 1-byte alignment  if byte == 1U
 *             2-byte alignment if byte == 2U
 *             4-byte alignment if byte == 4U
 */
static void pci_mmcfg_write_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val)
{
	uint32_t addr = mmcfg_off_to_address(bdf, offset);
	void *hva = hpa2hva(addr);

	ASSERT(pci_is_valid_access(offset, bytes), "the offset should be aligned with 2/4 byte\n");
	switch (bytes) {
	case 1U:
		mmio_write8((uint8_t)val, hva);
		break;
	case 2U:
		mmio_write16((uint16_t)val, hva);
		break;
	default:
		mmio_write32(val, hva);
		break;
	}
}

static const struct pci_cfg_ops pci_mmcfg_cfg_ops = {
	.pci_read_cfg = pci_mmcfg_read_cfg,
	.pci_write_cfg = pci_mmcfg_write_cfg,
};

static const struct pci_cfg_ops *acrn_pci_cfg_ops = &pci_pio_cfg_ops;

void pci_switch_to_mmio_cfg_ops(void)
{
	acrn_pci_cfg_ops = &pci_mmcfg_cfg_ops;
}

/*
 * @pre bytes == 1U || bytes == 2U || bytes == 4U
 * @pre must not be called before pci_switch_to_mmio_cfg_ops in release version.
 */
uint32_t pci_pdev_read_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes)
{
	uint32_t val = ~0U;

	val = acrn_pci_cfg_ops->pci_read_cfg(bdf, offset, bytes);

	return val;
}

/*
 * @pre bytes == 1U || bytes == 2U || bytes == 4U
 * @pre must not be called before pci_switch_to_mmio_cfg_ops in release version.
 */
void pci_pdev_write_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val)
{
	acrn_pci_cfg_ops->pci_write_cfg(bdf, offset, bytes, val);
}

bool pdev_need_bar_restore(const struct pci_pdev *pdev)
{
	bool need_restore = false;
	uint32_t idx, bar;

	for (idx = 0U; idx < pdev->nr_bars; idx++) {
		bar = pci_pdev_read_cfg(pdev->bdf, pci_bar_offset(idx), 4U);
		if (bar != pdev->bars[idx].phy_bar) {
			need_restore = true;
			break;
		}
	}

	return need_restore;
}

static void get_pci_bar_resource(union pci_bdf bdf, uint32_t offset, struct pci_bar_resource *res)
{
       res->phy_bar = pci_pdev_read_cfg(bdf, offset, 4U);

       pci_pdev_write_cfg(bdf, offset, 4U, ~0U);
       res->size_mask = pci_pdev_read_cfg(bdf, offset, 4U);
       pci_pdev_write_cfg(bdf, offset, 4U, res->phy_bar);
}

static inline void pdev_save_bar(struct pci_pdev *pdev)
{
	uint32_t idx;

	for (idx = 0U; idx < pdev->nr_bars; idx++) {
		get_pci_bar_resource(pdev->bdf, pci_bar_offset(idx), &pdev->bars[idx]);
	}
}

void pdev_restore_bar(const struct pci_pdev *pdev)
{
	uint32_t idx;

	for (idx = 0U; idx < pdev->nr_bars; idx++) {
		pci_pdev_write_cfg(pdev->bdf, pci_bar_offset(idx), 4U, pdev->bars[idx].phy_bar);
	}
}

static const struct pci_pdev *pci_find_pdev(uint16_t pbdf)
{
	struct hlist_node *n;
	const struct pci_pdev *found = NULL, *tmp;

	hlist_for_each (n, &pdevs_hlist_heads[hash64(pbdf, PDEV_HLIST_HASHBITS)]) {
		tmp = hlist_entry(n, struct pci_pdev, link);
		if (pbdf == tmp->bdf.value) {
			found = tmp;
			break;
		}
	}
	return found;
}

/* @brief: Find the DRHD index corresponding to a PCI device
 * Runs through the pci_pdevs and returns the value in drhd_idx
 * member from pdev structure that matches matches B:D.F
 *
 * @pbdf[in]	B:D.F of a PCI device
 *
 * @return if there is a matching pbdf in pci_pdevs, pdev->drhd_idx, else INVALID_DRHD_INDEX
 */

uint32_t pci_lookup_drhd_for_pbdf(uint16_t pbdf)
{
	const struct pci_pdev *pdev = pci_find_pdev(pbdf);
	return (pdev != NULL) ? pdev->drhd_index : INVALID_DRHD_INDEX;
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

bool is_plat_hidden_pdev(union pci_bdf bdf)
{
	static uint32_t hidden_pdevs_num = MAX_HIDDEN_PDEVS_NUM;
	uint32_t hidden_idx;
	bool hidden = false;

	for (hidden_idx = 0U; hidden_idx < hidden_pdevs_num; hidden_idx++) {
		if (bdf_is_equal(plat_hidden_pdevs[hidden_idx], bdf)) {
			hidden = true;
			break;
		}
	}

	return hidden;
}

bool is_hv_owned_pdev(union pci_bdf pbdf)
{
	bool ret = false;
	uint32_t i;

	for (i = 0U; i < num_hv_owned_pci_pdev; i++) {
		if (bdf_is_equal(pbdf, hv_owned_pci_pdevs[i]->bdf)) {
			pr_info("hv owned dev: (%x:%x:%x)", pbdf.bits.b, pbdf.bits.d, pbdf.bits.f);
			ret = true;
			break;
		}
	}
	return ret;
}

/*
 * quantity of uint64_t to encode a bitmap of all bus values
 * TODO: PCI_BUSMAX is a good candidate to move to
 * generated platform files.
 */
#define BUSES_BITMAP_LEN        ((PCI_BUSMAX + 1U) >> 6U)

/*
 * must be >= total Endpoints in all DRDH devscope
 * TODO: BDF_MAPPING_NUM is a good candidate to move to
 * generated platform files.
 */
#define BDF_MAPPING_NUM			32U

struct pci_bdf_to_drhd_index_mapping {
	union pci_bdf dev_scope_bdf;
	uint32_t dev_scope_drhd_index;
};

struct pci_bdf_mapping_group {
	uint32_t pci_bdf_map_count;
	struct pci_bdf_to_drhd_index_mapping bdf_map[BDF_MAPPING_NUM];
};

struct pci_bus_num_to_drhd_index_mapping {
	uint8_t bus_under_scan;
	uint32_t bus_drhd_index;
};

static uint32_t pci_check_override_drhd_index(union pci_bdf pbdf,
		const struct pci_bdf_mapping_group *const bdfs_from_drhds,
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

/* @brief: scan PCI physical devices from specific bus.
 * walks through the bus to scan PCI physical devices and using the discovered physical devices
 * to initialize pdevs, each pdev can only provdie a physical device information (like bdf, bar,
 * capabilities) before using this pdev, it needs to use the pdev to initialize a per VM device
 * configuration(acrn_vm_pci_dev_config), call init_one_dev_config or init_all_dev_config to do this.
 */
static void scan_pci_hierarchy(uint8_t bus, uint64_t buses_visited[BUSES_BITMAP_LEN],
		const struct pci_bdf_mapping_group *const bdfs_from_drhds, uint32_t drhd_index)
{
	uint32_t vendor;
	uint8_t hdr_type, dev, func, func_max;
	union pci_bdf pbdf;
	uint8_t current_bus_index;
	uint32_t current_drhd_index, bdf_drhd_index;
	struct pci_pdev *pdev;

	struct pci_bus_num_to_drhd_index_mapping bus_map[PCI_BUSMAX + 1U]; /* FIFO queue of buses to walk */
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
		if (pbdf.bits.b < phys_pci_mmcfg.start_bus || pbdf.bits.b > phys_pci_mmcfg.end_bus) {
			continue;
		}

		for (dev = 0U; dev <= PCI_SLOTMAX; dev++) {
			pbdf.bits.d = dev;
			pbdf.bits.f = 0U;
			hdr_type = (uint8_t)pci_pdev_read_cfg(pbdf, PCIR_HDRTYPE, 1U);

			/* Do not probe beyond function 0 if not a multi-function device
			 * unless device supports ARI or SR-IOV (PCIe spec r5.0 §7.5.1.1.9)
			 */
			func_max = is_pci_cfg_multifunction(hdr_type) ? PCI_FUNCMAX : 0U;
			for (func = 0U; func <= func_max; func++) {

				pbdf.bits.f = func;

				vendor = pci_pdev_read_cfg(pbdf, PCIR_VENDOR, 2U);

				if (!is_pci_vendor_valid(vendor)) {
					continue;
				}

				bdf_drhd_index = pci_check_override_drhd_index(pbdf, bdfs_from_drhds,
									current_drhd_index);
				pdev = pci_init_pdev(pbdf, bdf_drhd_index);
				/* NOTE: This touch logic change: if a bridge own by HV as its children */
				if ((pdev != NULL) && is_bridge(pdev)) {
					bus_map[e].bus_under_scan =
						(uint8_t)pci_pdev_read_cfg(pbdf, PCIR_SECBUS_1, 1U);
					bus_map[e].bus_drhd_index = bdf_drhd_index;
					e = e + 1U;
				}
			}
		}
	}
}

/*
 * @brief: Setup bdfs_from_drhds data structure as the DMAR tables are walked searching
 * for PCI device scopes. bdfs_from_drhds is used later in scan_pci_hierarchy
 * to map the right DRHD unit to the PCI device
 */
static void pci_add_bdf_from_drhd(union pci_bdf bdf, struct pci_bdf_mapping_group *const bdfs_from_drhds,
					uint32_t drhd_index)
{
	if (bdfs_from_drhds->pci_bdf_map_count < BDF_MAPPING_NUM) {
		bdfs_from_drhds->bdf_map[bdfs_from_drhds->pci_bdf_map_count].dev_scope_bdf = bdf;
		bdfs_from_drhds->bdf_map[bdfs_from_drhds->pci_bdf_map_count].dev_scope_drhd_index = drhd_index;
		bdfs_from_drhds->pci_bdf_map_count++;
	} else {
		ASSERT(bdfs_from_drhds->pci_bdf_map_count < BDF_MAPPING_NUM,
				"Compare value in BDF_MAPPING_NUM against those in ACPI DMAR tables");
	}
}


/*
 * @brief: Setup bdfs_from_drhds data structure as the DMAR tables are walked searching
 * for PCI device scopes. bdfs_from_drhds is used later in scan_pci_hierarchy
 * to map the right DRHD unit to the PCI device.
 * TODO: bdfs_from_drhds is a good candidate to be part of generated platform
 * info.
 */
static void pci_parse_iommu_devscopes(struct pci_bdf_mapping_group *const bdfs_from_drhds,
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

		if ((plat_dmar_info.drhd_units[drhd_index].flags & DRHD_FLAG_INCLUDE_PCI_ALL_MASK)
				== DRHD_FLAG_INCLUDE_PCI_ALL_MASK) {
			*drhd_idx_pci_all = drhd_index;
		}
	}
}

/*
 * There are some rules to config PCI bridge: try to avoid interference between SOS and RTVM or
 * pre-launched VM; and to support some features like SRIOV by default, so as following:
 *	1. disable interrupt, including INTx and MSI.
 *	2. enable ARI if it's a PCIe bridge and all its sub devices support ARI (need check further).
 *  3. enable ACS. (now assume BIOS does it), could check and do it in HV in the future.
 *
 */
static void	config_pci_bridge(const struct pci_pdev *pdev)
{
	uint32_t offset, val, msgctrl;

	/* for command regsiters, disable INTx */
	val = pci_pdev_read_cfg(pdev->bdf, PCIR_COMMAND, 2U);
	pci_pdev_write_cfg(pdev->bdf, PCIR_COMMAND, 2U, (uint16_t)val | PCIM_CMD_INTxDIS);

	/* disale MSI */
	if (pdev->msi_capoff != 0x00UL) {
		offset = pdev->msi_capoff + PCIR_MSI_CTRL;

		msgctrl = pci_pdev_read_cfg(pdev->bdf, offset, 2U);
		msgctrl &= ~PCIM_MSICTRL_MSI_ENABLE;
		pci_pdev_write_cfg(pdev->bdf, offset, 2U, msgctrl);
	}

	/* Enable ARI if PCIe bridge could support it for SRIOV needs it */
	if (pdev->pcie_capoff != 0x00UL) {
		offset = pdev->pcie_capoff + PCIR_PCIE_DEVCAP2;
		val = pci_pdev_read_cfg(pdev->bdf, offset, 2U);

		if ((val & PCIM_PCIE_DEVCAP2_ARI) != 0U) {
			offset = pdev->pcie_capoff + PCIR_PCIE_DEVCTL2;

			val = pci_pdev_read_cfg(pdev->bdf, offset, 2U);
			val |= PCIM_PCIE_DEVCTL2_ARI;
			pci_pdev_write_cfg(pdev->bdf, offset, 2U, val);
		}
	}
}

/*
 * @brief: walks through all pdevs that have been initialized and determine
 * which pdevs need to be added to pci dev_config. The pdevs added to pci
 * dev_config will be exposed to SOS finally.
 */
static void init_all_dev_config(void)
{
	uint32_t idx, cnt = 0U;
	uint32_t total = num_pci_pdev;
	struct pci_pdev *pdev = NULL;

	for (idx = 0U; idx < num_pci_pdev; idx++) {
		pdev = &pci_pdevs[idx];

		if (is_bridge(pdev)) {
			config_pci_bridge(pdev);
		}

		/*
		 * FIXME: Mask the SR-IOV capability instead drop the device
		 * when supporting PCIe extended capabilities whitelist.
		 */
		if (pdev->sriov.capoff != 0U) {
			cnt = pci_pdev_read_cfg(pdev->bdf,
				pdev->sriov.capoff + PCIR_SRIOV_TOTAL_VFS, 2U);
			/*
			 * For SRIOV-Capable device, drop the device
			 * if no room for its all of virtual functions.
			 */
			if ((total + cnt) > CONFIG_MAX_PCI_DEV_NUM) {
				pr_err("%s, %x:%x.%x is dropped since no room for %u VFs",
						__func__, pdev->bdf.bits.b, pdev->bdf.bits.d, pdev->bdf.bits.f, cnt);
				continue;
			} else {
				total += cnt;
			}
		}
		(void)init_one_dev_config(pdev);
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
	struct pci_bdf_mapping_group bdfs_from_drhds = {.pci_bdf_map_count = 0U};
	uint32_t drhd_idx_pci_all = INVALID_DRHD_INDEX;
	uint16_t bus;
	bool was_visited = false;

	ppt_clear_user_bit(phys_pci_mmcfg.address, get_pci_mmcfg_size(&phys_pci_mmcfg));

	pci_parse_iommu_devscopes(&bdfs_from_drhds, &drhd_idx_pci_all);

	/* TODO: iterate over list of PCI Host Bridges found in ACPI namespace */
	for (bus = 0U; bus <= PCI_BUSMAX; bus++) {
		was_visited = bitmap_test((bus & 0x3FU), &buses_visited[bus >> 6U]);
		if (!was_visited) {
			scan_pci_hierarchy((uint8_t)bus, buses_visited, &bdfs_from_drhds, drhd_idx_pci_all);
		}
	}
	init_all_dev_config();
}

static inline uint32_t pci_pdev_get_nr_bars(uint8_t hdr_type)
{
	uint32_t nr_bars = 0U;

	switch (hdr_type & PCIM_HDRTYPE) {
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

/**
 * @pre pdev != NULL
 */
static void pci_enumerate_ext_cap(struct pci_pdev *pdev) {

	uint32_t hdr, pos, pre_pos = 0U;

	pos = PCI_ECAP_BASE_PTR;

	/* PCI Express Extended Capability must have 4 bytes header */
	hdr = pci_pdev_read_cfg(pdev->bdf, pos, 4U);
	while (hdr != 0U) {
		if (PCI_ECAP_ID(hdr) == PCIZ_SRIOV) {
			pdev->sriov.capoff = pos;
			pdev->sriov.caplen = PCI_SRIOV_CAP_LEN;
			pdev->sriov.pre_pos = pre_pos;
		}
		pre_pos = pos;
		pos = PCI_ECAP_NEXT(hdr);
		if (pos == 0U) {
			break;
		}
		hdr = pci_pdev_read_cfg(pdev->bdf, pos, 4U);
	};
}

/*
 * @pre pdev != NULL
 */
static void pci_enumerate_cap(struct pci_pdev *pdev)
{
	uint8_t pos, cap;
	uint32_t msgctrl;
	uint32_t len, idx;
	uint32_t table_info;
	uint32_t pcie_devcap, val;
	bool is_pcie = false;

	pos = (uint8_t)pci_pdev_read_cfg(pdev->bdf, PCIR_CAP_PTR, 1U);

	while ((pos != 0U) && (pos != 0xFFU)) {
		cap = (uint8_t)pci_pdev_read_cfg(pdev->bdf, pos + PCICAP_ID, 1U);

		if (cap == PCIY_MSI) {
			pdev->msi_capoff = pos;
		} else if (cap == PCIY_MSIX) {
			pdev->msix.capoff = pos;
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
				pdev->msix.cap[idx] = (uint8_t)pci_pdev_read_cfg(pdev->bdf, (uint32_t)pos + idx, 1U);
			}
		} else if (cap == PCIY_PMC) {
			val = pci_pdev_read_cfg(pdev->bdf, pos + PCIR_PMCSR, 4U);
			pdev->has_pm_reset = ((val & PCIM_PMCSR_NO_SOFT_RST) == 0U);
		} else if (cap == PCIY_PCIE) {
			is_pcie = true;
			pcie_devcap = pci_pdev_read_cfg(pdev->bdf, pos + PCIR_PCIE_DEVCAP, 4U);
			pdev->pcie_capoff = pos;
			pdev->has_flr = ((pcie_devcap & PCIM_PCIE_FLRCAP) != 0U);
		} else if (cap == PCIY_AF) {
			val = pci_pdev_read_cfg(pdev->bdf, pos, 4U);
			pdev->has_af_flr = ((val & PCIM_AF_FLR_CAP) != 0U);
		} else {
			/* Ignore all other Capability IDs for now */
		}

		pos = (uint8_t)pci_pdev_read_cfg(pdev->bdf, pos + PCICAP_NEXTPTR, 1U);
	}

	if (is_pcie) {
		pci_enumerate_ext_cap(pdev);
	}
}

/*
 * @brief Initialize a pdev data structure.
 *
 * Initialize a pdev data structure with a physical device BDF(pbdf) and DRHD index(drhd_index).
 * The caller of the function init_pdev should guarantee execution atomically.
 *
 * @param pbdf        Physical device BDF
 * @param drhd_index  DRHD index
 *
 * @return If there's a successfully initialized pdev return it, otherwise return NULL;
 */
struct pci_pdev *pci_init_pdev(union pci_bdf bdf, uint32_t drhd_index)
{
	uint8_t hdr_type, hdr_layout;
	struct pci_pdev *pdev = NULL;
	bool is_hv_owned = false;

	if (num_pci_pdev < CONFIG_MAX_PCI_DEV_NUM) {
		hdr_type = (uint8_t)pci_pdev_read_cfg(bdf, PCIR_HDRTYPE, 1U);
		hdr_layout = (hdr_type & PCIM_HDRTYPE);

		if ((hdr_layout == PCIM_HDRTYPE_NORMAL) || (hdr_layout == PCIM_HDRTYPE_BRIDGE)) {
			pdev = &pci_pdevs[num_pci_pdev];
			pdev->bdf = bdf;
			pdev->hdr_type = hdr_type;
			pdev->base_class = (uint8_t)pci_pdev_read_cfg(bdf, PCIR_CLASS, 1U);
			pdev->sub_class = (uint8_t)pci_pdev_read_cfg(bdf, PCIR_SUBCLASS, 1U);
			pdev->nr_bars = pci_pdev_get_nr_bars(hdr_type);
			pdev_save_bar(pdev);

			if ((pci_pdev_read_cfg(bdf, PCIR_STATUS, 2U) & PCIM_STATUS_CAPPRESENT) != 0U) {
				pci_enumerate_cap(pdev);
			}

#if (PRE_VM_NUM != 0U)
			/* HV owned pdev: 1.typ1 pdev if pre-launched VM exist; 2.pci debug uart */
			is_hv_owned = (hdr_layout == PCIM_HDRTYPE_BRIDGE) || is_pci_dbg_uart(bdf);
#else
			/* HV owned pdev: 1.pci debug uart */
			is_hv_owned = is_pci_dbg_uart(bdf);
#endif
			if (is_hv_owned) {
				hv_owned_pci_pdevs[num_hv_owned_pci_pdev] = pdev;
				num_hv_owned_pci_pdev++;
			}
			hlist_add_head(&pdev->link, &pdevs_hlist_heads[hash64(bdf.value, PDEV_HLIST_HASHBITS)]);
			pdev->drhd_index = drhd_index;
			num_pci_pdev++;
			reserve_vmsix_on_msi_irtes(pdev);
		} else {
			pr_err("%s, %x:%x.%x unsupported headed type: 0x%x\n",
				__func__, bdf.bits.b, bdf.bits.d, bdf.bits.f, hdr_type);
		}
	} else {
		pr_err("%s, failed to alloc pci_pdev!\n", __func__);
	}

	return pdev;
}
