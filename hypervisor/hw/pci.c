/*
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

static spinlock_t pci_device_lock = {
	.head = 0U,
	.tail = 0U
};

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

	spinlock_obtain(&pci_device_lock);

	addr = pci_pdev_calc_address(bdf, offset);

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

void pci_pdev_write_cfg(union pci_bdf bdf, uint32_t offset, uint32_t bytes,	uint32_t val)
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
#define BUS_SCAN_PENDING  	1U
#define BUS_SCAN_COMPLETE	2U
void pci_scan_bus(pci_enumeration_cb cb_func, void *cb_data)
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

				if ((val == 0xFFFFFFFFU) || (val == 0x0U)) {
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

				if (cb_func != NULL) {
					cb_func(pbdf.value, cb_data);
				}

				hdr_type = (uint8_t)pci_pdev_read_cfg(pbdf, PCIR_HDRTYPE, 1U);
				if ((hdr_type & PCIM_HDRTYPE) == PCIM_HDRTYPE_BRIDGE) {

					/* Secondary bus to be scanned */
					secondary_bus = (uint8_t)pci_pdev_read_cfg(pbdf, PCIR_SECBUS_1, 1U);
					if (bus_to_scan[secondary_bus] != BUS_SCAN_SKIP) {
						pr_err("%s, bus %d may be downstream of different PCI bridges", secondary_bus);
					} else {
						bus_to_scan[secondary_bus] = BUS_SCAN_PENDING;
					}
				}
			}
		}
	}
}
