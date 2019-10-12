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
							__func__, secondary_bus);
					} else {
						bus_to_scan[secondary_bus] = BUS_SCAN_PENDING;
					}
				}
			}
		}
	}
}

static inline uint32_t pci_pdev_get_nr_bars(uint8_t hdr_type)
{
	uint32_t nr_bars = 0U;

	switch (hdr_type) {
	case PCIM_HDRTYPE_NORMAL:
		nr_bars = 6U;
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

	ptr = (uint8_t)pci_pdev_read_cfg(pdev->bdf, PCIR_CAP_PTR, 1U);

	while ((ptr != 0U) && (ptr != 0xFFU)) {
		cap = (uint8_t)pci_pdev_read_cfg(pdev->bdf, ptr + PCICAP_ID, 1U);

		/* Ignore all other Capability IDs for now */
		if ((cap == PCIY_MSI) || (cap == PCIY_MSIX)) {
			offset = ptr;
			if (cap == PCIY_MSI) {
				pdev->msi_capoff = offset;
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

				ASSERT(pdev->msix.table_count <= CONFIG_MAX_MSIX_TABLE_NUM);

				/* Copy MSIX capability struct into buffer */
				for (idx = 0U; idx < len; idx++) {
					pdev->msix.cap[idx] = (uint8_t)pci_pdev_read_cfg(pdev->bdf, offset + idx, 1U);
				}
			}
		}

		ptr = (uint8_t)pci_pdev_read_cfg(pdev->bdf, ptr + PCICAP_NEXTPTR, 1U);
	}
}

static void init_pdev(uint16_t pbdf)
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
