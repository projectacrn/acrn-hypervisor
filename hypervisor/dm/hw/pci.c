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

uint32_t pci_pdev_read_cfg(struct pci_pdev *pdev, uint32_t offset, uint32_t bytes)
{
	uint32_t addr;
	uint32_t val;

	spinlock_obtain(&pci_device_lock);

	addr = pci_pdev_calc_address(pdev->bdf, offset);

	/* Write address to ADDRESS register */
	pio_write32(addr, PCI_CONFIG_ADDR);

	/* Read result from DATA register */
	switch (bytes) {
	case 1U:
		val = (uint32_t)pio_read8(PCI_CONFIG_DATA + ((uint16_t)offset & 3U));
		break;
	case 2U:
		val = (uint32_t)pio_read16(PCI_CONFIG_DATA + ((uint16_t)offset & 2U));
		break;
	default:
		val = pio_read32(PCI_CONFIG_DATA);
		break;
	}
	spinlock_release(&pci_device_lock);

	return val;
}

void pci_pdev_write_cfg(struct pci_pdev *pdev, uint32_t offset, uint32_t bytes,
	uint32_t val)
{
	uint32_t addr;

	spinlock_obtain(&pci_device_lock);

	addr = pci_pdev_calc_address(pdev->bdf, offset);

	/* Write address to ADDRESS register */
	pio_write32(addr, PCI_CONFIG_ADDR);

	/* Write value to DATA register */
	switch (bytes) {
	case 1U:
		pio_write8((uint8_t)val, PCI_CONFIG_DATA + ((uint16_t)offset & 3U));
		break;
	case 2U:
		pio_write16((uint16_t)val, PCI_CONFIG_DATA + ((uint16_t)offset & 2U));
		break;
	default:
		pio_write32(val, PCI_CONFIG_DATA);
		break;
	}
	spinlock_release(&pci_device_lock);
}
