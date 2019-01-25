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
 */

#include <hypervisor.h>
#include <pci.h>

static spinlock_t pci_device_lock;
static uint32_t num_pci_pdev;
static struct pci_pdev pci_pdev_array[CONFIG_MAX_PCI_DEV_NUM];

static void init_pdev(uint16_t pbdf);


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

#define BUS_SCAN_SKIP		0U
#define BUS_SCAN_PENDING	1U
#define BUS_SCAN_COMPLETE	2U
void init_pci_pdev_list(void)
{
	union pci_bdf pbdf;
	uint8_t hdr_type, secondary_bus, dev, func;
	uint32_t bus, val;
	uint8_t bus_to_scan[PCI_BUSMAX + 1] = { BUS_SCAN_SKIP };

	/* start from bus 0 */
	bus_to_scan[0U] = BUS_SCAN_PENDING;

	for (bus = 0U; bus <= PCI_BUSMAX; bus++) {
		if (bus_to_scan[bus] != BUS_SCAN_PENDING) {
			continue;
		}

		bus_to_scan[bus] = BUS_SCAN_COMPLETE;
		pbdf.bits.b = (uint8_t)bus;

		for (dev = 0U; dev <= PCI_SLOTMAX; dev++) {
			pbdf.bits.d = dev;

			for (func = 0U; func <= PCI_FUNCMAX; func++) {
				pbdf.bits.f = func;
				val = pci_pdev_read_cfg(pbdf, PCIR_VENDOR, 4U);

				if ((val == 0xFFFFFFFFU) || (val == 0U) || (val == 0xFFFF0000U) || (val == 0xFFFFU)) {
					/* If function 0 is not implemented, skip to next device */
					if (func == 0U) {
						break;
					}

					/* continue scan next function */
					continue;
				}

				/* if it is debug uart, hide it from SOS */
				if (is_pci_dbg_uart(pbdf)) {
					pr_info("hide pci uart dev: (%x:%x:%x)", pbdf.bits.b, pbdf.bits.d, pbdf.bits.f);
					continue;
				}

				init_pdev(pbdf.value);

				hdr_type = (uint8_t)pci_pdev_read_cfg(pbdf, PCIR_HDRTYPE, 1U);
				if ((hdr_type & PCIM_HDRTYPE) == PCIM_HDRTYPE_BRIDGE) {

					/* Secondary bus to be scanned */
					secondary_bus = (uint8_t)pci_pdev_read_cfg(pbdf, PCIR_SECBUS_1, 1U);
					if (bus_to_scan[secondary_bus] != BUS_SCAN_SKIP) {
						pr_err("%s, bus %d may be downstream of different PCI bridges",
							secondary_bus);
					} else {
						bus_to_scan[secondary_bus] = BUS_SCAN_PENDING;
					}
				}
			}
		}
	}
}

static uint8_t pci_pdev_get_num_bars(uint8_t hdr_type)
{
	uint8_t num_bars = (uint8_t)0U;

	switch (hdr_type & PCIM_HDRTYPE) {
	case PCIM_HDRTYPE_NORMAL:
		num_bars = (uint8_t)6U;
		break;

	case PCIM_HDRTYPE_BRIDGE:
		num_bars = (uint8_t)2U;
		break;

	case PCIM_HDRTYPE_CARDBUS:
		num_bars = (uint8_t)1U;
		break;

	default:
		/*no actions are required for other cases.*/
		break;
	}

	return num_bars;
}

static enum pci_bar_type pci_pdev_read_bar_type(union pci_bdf bdf, uint8_t idx)
{
	uint32_t bar;
	enum pci_bar_type type = PCIBAR_NONE;

	bar = pci_pdev_read_cfg(bdf, pci_bar_offset(idx), 4U);
	if ((bar & PCIM_BAR_SPACE) == PCIM_BAR_IO_SPACE) {
		type = PCIBAR_IO_SPACE;
	} else {
		switch (bar & PCIM_BAR_MEM_TYPE) {
		case PCIM_BAR_MEM_32:
		case PCIM_BAR_MEM_1MB:
			type = PCIBAR_MEM32;
			break;

		case PCIM_BAR_MEM_64:
			type = PCIBAR_MEM64;
			break;

		default:
			/*no actions are required for other cases.*/
			break;
		}
	}

	return type;
}

static uint8_t pci_pdev_read_bar(union pci_bdf bdf, uint8_t idx, struct pci_bar *bar)
{
	uint64_t base, size;
	enum pci_bar_type type;
	uint32_t bar_lo, bar_hi, val32;
	uint32_t bar_base_mask;

	base = 0UL;
	size = 0UL;
	type = pci_pdev_read_bar_type(bdf, idx);

	if (type != PCIBAR_NONE) {
		if (type == PCIBAR_IO_SPACE) {
			bar_base_mask = ~0x03U;
		} else {
			bar_base_mask = ~0x0fU;
		}

		bar_lo = pci_pdev_read_cfg(bdf, pci_bar_offset(idx), 4U);

		/* Get the base address */
		base = (uint64_t)bar_lo & bar_base_mask;

		if (base != 0UL) {
			if (type == PCIBAR_MEM64) {
				bar_hi = pci_pdev_read_cfg(bdf, pci_bar_offset(idx + 1U), 4U);
				base |= ((uint64_t)bar_hi << 32U);
			}

			/* Sizing the BAR */
			size = 0UL;
			if ((type == PCIBAR_MEM64) && (idx < (PCI_BAR_COUNT - 1U))) {
				pci_pdev_write_cfg(bdf, pci_bar_offset(idx + 1U), 4U, ~0U);
				size = (uint64_t)pci_pdev_read_cfg(bdf, pci_bar_offset(idx + 1U), 4U);
				size <<= 32U;
			}

			pci_pdev_write_cfg(bdf, pci_bar_offset(idx), 4U, ~0U);
			val32 = pci_pdev_read_cfg(bdf, pci_bar_offset(idx), 4U);
			size |= ((uint64_t)val32 & bar_base_mask);

			if (size != 0UL) {
				size = size & ~(size - 1U);
			}

			/* Restore the BAR */
			pci_pdev_write_cfg(bdf, pci_bar_offset(idx), 4U, bar_lo);

			if (type == PCIBAR_MEM64) {
				pci_pdev_write_cfg(bdf, pci_bar_offset(idx + 1U), 4U, bar_hi);
			}
		}
	}

	bar->base = base;
	bar->size = size;
	bar->type = type;

	return (type == PCIBAR_MEM64)?2U:1U;
}


/*
 * @pre nr_bars <= PCI_BAR_COUNT
 */
static void pci_pdev_read_bars(union pci_bdf bdf, uint8_t nr_bars, struct pci_bar *bar)
{
	uint8_t	idx = 0U;

	while (idx < nr_bars) {
		idx += pci_pdev_read_bar(bdf, idx, &bar[idx]);
	}
}

static void pci_read_cap(struct pci_pdev *pdev)
{
	uint8_t ptr, cap;
	uint32_t msgctrl;
	uint32_t len, offset, idx;
	uint32_t table_info;

	ptr = (uint8_t)pci_pdev_read_cfg(pdev->bdf, PCIR_CAP_PTR, 1U);

	while ((ptr != 0U) && (ptr != 0xFFU)) {
		cap = (uint8_t)pci_pdev_read_cfg(pdev->bdf, ptr + PCICAP_ID, 1U);

		/* Ignore all other Capability IDs for now */
		if ((cap == PCIY_MSI) || (cap == PCIY_MSIX)) {
			offset = ptr;
			if (cap == PCIY_MSI) {
				pdev->msi.capoff = offset;
				msgctrl = pci_pdev_read_cfg(pdev->bdf, offset + PCIR_MSI_CTRL, 2U);
				len = ((msgctrl & PCIM_MSICTRL_64BIT) != 0U) ? 14U : 10U;
				pdev->msi.caplen = len;

				/* Copy MSI capability struct into buffer */
				for (idx = 0U; idx < len; idx++) {
					pdev->msi.cap[idx] = (uint8_t)pci_pdev_read_cfg(pdev->bdf, offset + idx, 1U);
				}
			} else {
				pdev->msix.capoff = offset;
				pdev->msix.caplen = MSIX_CAPLEN;
				len = pdev->msix.caplen;

				msgctrl = pci_pdev_read_cfg(pdev->bdf, pdev->msix.capoff + PCIR_MSIX_CTRL, 2U);

				/* Read Table Offset and Table BIR */
				table_info = pci_pdev_read_cfg(pdev->bdf, pdev->msix.capoff + PCIR_MSIX_TABLE, 4U);

				pdev->msix.table_bar = (uint8_t)(table_info & PCIM_MSIX_BIR_MASK);

				pdev->msix.table_offset = table_info & ~PCIM_MSIX_BIR_MASK;
				pdev->msix.table_count = (msgctrl & PCIM_MSIXCTRL_TABLE_SIZE) + 1U;

				/* Copy MSIX capability struct into buffer */
				for (idx = 0U; idx < len; idx++) {
					pdev->msix.cap[idx] = (uint8_t)pci_pdev_read_cfg(pdev->bdf, offset + idx, 1U);
				}
			}
		}

		ptr = (uint8_t)pci_pdev_read_cfg(pdev->bdf, ptr + PCICAP_NEXTPTR, 1U);
	}
}

static void fill_pdev(uint16_t pbdf, struct pci_pdev *pdev)
{
	uint8_t  hdr_type;
	uint8_t  nr_bars;

	pdev->bdf.value = pbdf;

	hdr_type = (uint8_t)pci_pdev_read_cfg(pdev->bdf, PCIR_HDRTYPE, 1U);

	nr_bars = pci_pdev_get_num_bars(hdr_type);

	pci_pdev_read_bars(pdev->bdf, nr_bars, &pdev->bar[0]);

	if ((pci_pdev_read_cfg(pdev->bdf, PCIR_STATUS, 2U) & PCIM_STATUS_CAPPRESENT) != 0U) {
		pci_read_cap(pdev);
	}
}

static void init_pdev(uint16_t pbdf)
{
	if (num_pci_pdev < CONFIG_MAX_PCI_DEV_NUM) {
		fill_pdev(pbdf, &pci_pdev_array[num_pci_pdev]);
		num_pci_pdev++;
	} else {
		pr_err("%s, failed to alloc pci_pdev!\n", __func__);
	}
}

void pci_pdev_foreach(pci_pdev_enumeration_cb cb_func, const void *ctx)
{
	uint32_t idx;

	for (idx = 0U; idx < num_pci_pdev; idx++) {
		if (cb_func != NULL) {
			cb_func(&pci_pdev_array[idx], ctx);
		}
	}
}

