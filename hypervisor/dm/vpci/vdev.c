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

#include <x86/guest/vm.h>
#include "vpci_priv.h"
#include <x86/guest/ept.h>
#include <logmsg.h>
#include <hash.h>

/**
 * @pre vdev != NULL
 */
uint32_t pci_vdev_read_vcfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes)
{
	uint32_t val;

	switch (bytes) {
	case 1U:
		val = vdev->cfgdata.data_8[offset];
		break;
	case 2U:
		val = vdev->cfgdata.data_16[offset >> 1U];
		break;
	default:
		val = vdev->cfgdata.data_32[offset >> 2U];
		break;
	}

	return val;
}

/**
 * @pre vdev != NULL
 */
void pci_vdev_write_vcfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	switch (bytes) {
	case 1U:
		vdev->cfgdata.data_8[offset] = (uint8_t)val;
		break;
	case 2U:
		vdev->cfgdata.data_16[offset >> 1U] = (uint16_t)val;
		break;
	default:
		vdev->cfgdata.data_32[offset >> 2U] = val;
		break;
	}
}

/**
 * @pre vpci != NULL
 * @pre vpci->pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 */
struct pci_vdev *pci_find_vdev(struct acrn_vpci *vpci, union pci_bdf vbdf)
{
	struct pci_vdev *vdev = NULL, *tmp;
	struct hlist_node *n;

	hlist_for_each(n, &vpci->vdevs_hlist_heads[hash64(vbdf.value, VDEV_LIST_HASHBITS)]) {
		tmp = hlist_entry(n, struct pci_vdev, link);
		if (bdf_is_equal(vbdf, tmp->bdf)) {
			vdev = tmp;
			break;
		}
	}

	return vdev;
}

uint32_t pci_vdev_read_vbar(const struct pci_vdev *vdev, uint32_t idx)
{
	uint32_t bar, offset;

	offset = pci_bar_offset(idx);
	bar = pci_vdev_read_vcfg(vdev, offset, 4U);
	/* Sizing BAR */
	if (bar == ~0U) {
		bar = vdev->vbars[idx].mask | vdev->vbars[idx].bar_type.bits;
	}
	return bar;
}

static void pci_vdev_update_vbar_base(struct pci_vdev *vdev, uint32_t idx)
{
	struct pci_vbar *vbar;
	uint64_t base = 0UL;
	uint32_t lo, hi, offset;

	vbar = &vdev->vbars[idx];
	offset = pci_bar_offset(idx);
	lo = pci_vdev_read_vcfg(vdev, offset, 4U);
	if ((!is_pci_reserved_bar(vbar)) && (lo != ~0U)) {
		base = lo & vbar->mask;

		if (is_pci_mem64lo_bar(vbar)) {
			vbar = &vdev->vbars[idx + 1U];
			hi = pci_vdev_read_vcfg(vdev, (offset + 4U), 4U);
			if (hi != ~0U) {
				hi &= vbar->mask;
				base |= ((uint64_t)hi << 32U);
			} else {
				base = 0UL;
			}
		}
	} else if (is_pci_io_bar(vbar)) {
		/* Because guest driver may write to upper 16-bits of PIO BAR and expect that should have no effect,
		 * SO PIO BAR base may bigger than 0xffff after calculation, should mask the upper 16-bits.
		 */
		base &= 0xffffUL;
	}

	if (is_pci_mem_bar(vbar) && (base != 0UL) && !ept_is_mr_valid(vpci2vm(vdev->vpci), base, vdev->vbars[idx].size)) {
		pr_warn("%s, %x:%x.%x set invalid bar[%d] base: 0x%lx, size: 0x%lx\n", __func__,
			vdev->bdf.bits.b, vdev->bdf.bits.d, vdev->bdf.bits.f, idx, base, vdev->vbars[idx].size);
		base = 0UL;	/* 0UL means invalid GPA, so that EPT won't map */
	}

	vdev->vbars[idx].base_gpa = base;
}

void pci_vdev_write_vbar(struct pci_vdev *vdev, uint32_t idx, uint32_t val)
{
	struct pci_vbar *vbar;
	uint32_t bar, offset;
	uint32_t update_idx = idx;

	vbar = &vdev->vbars[idx];
	bar = val & vbar->mask;
	if (vbar->is_mem64hi) {
		update_idx -= 1U;
	} else {
		if (is_pci_io_bar(vbar)) {
			bar |= (vbar->bar_type.bits & (~PCI_BASE_ADDRESS_IO_MASK));
		} else {
			bar |= (vbar->bar_type.bits & (~PCI_BASE_ADDRESS_MEM_MASK));
		}
	}
	offset = pci_bar_offset(idx);
	pci_vdev_write_vcfg(vdev, offset, 4U, bar);

	pci_vdev_update_vbar_base(vdev, update_idx);
}
