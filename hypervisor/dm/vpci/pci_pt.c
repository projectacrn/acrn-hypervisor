/*-
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
#include <asm/guest/vm.h>
#include <errno.h>
#include <ptdev.h>
#include <asm/guest/assign.h>
#include <asm/vtd.h>
#include <asm/guest/ept.h>
#include <asm/mmu.h>
#include <io.h>
#include <logmsg.h>
#include <config.h>
#include "vpci_priv.h"

/**
 * @pre vdev != NULL
 */
static inline struct msix_table_entry *get_msix_table_entry(const struct pci_vdev *vdev, uint32_t index)
{
	void *hva = hpa2hva(vdev->msix.mmio_hpa + vdev->msix.table_offset);

	return ((struct msix_table_entry *)hva + index);
}

/**
 * @brief Reading MSI-X Capability Structure
 *
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void read_pt_vmsix_cap_reg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	if (vdev->msix.is_vmsix_on_msi) {
		*val = pci_vdev_read_vcfg(vdev, offset, bytes);
	} else {
		read_vmsix_cap_reg(vdev, offset, bytes, val);
	}
}

/**
 * @brief Writing MSI-X Capability Structure
 *
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void write_pt_vmsix_cap_reg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	uint32_t msgctrl;

	if (write_vmsix_cap_reg(vdev, offset, bytes, val)) {
		msgctrl = pci_vdev_read_vcfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U);
		/* If MSI Enable is being set, make sure INTxDIS bit is set */
		if ((msgctrl & PCIM_MSIXCTRL_MSIX_ENABLE) != 0U) {
			enable_disable_pci_intx(vdev->pdev->bdf, false);
		}
		pci_pdev_write_cfg(vdev->pdev->bdf, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U, msgctrl);
	}
}

/**
 * @pre vdev != NULL
 */
static void mask_one_msix_vector(const struct pci_vdev *vdev, uint32_t index)
{
	uint32_t vector_control;
	struct msix_table_entry *pentry = get_msix_table_entry(vdev, index);

	stac();
	vector_control = pentry->vector_control | PCIM_MSIX_VCTRL_MASK;
	mmio_write32(vector_control, (void *)&(pentry->vector_control));
	clac();
}


/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->pdev != NULL
 */
static void remap_one_vmsix_entry(const struct pci_vdev *vdev, uint32_t index)
{
	const struct msix_table_entry *ventry;
	struct msix_table_entry *pentry;
	struct msi_info info = {};
	int32_t ret;

	mask_one_msix_vector(vdev, index);
	ventry = &vdev->msix.table_entries[index];
	if ((ventry->vector_control & PCIM_MSIX_VCTRL_MASK) == 0U) {
		info.addr.full = vdev->msix.table_entries[index].addr;
		info.data.full = vdev->msix.table_entries[index].data;

		ret = ptirq_prepare_msix_remap(vpci2vm(vdev->vpci), vdev->bdf.value, vdev->pdev->bdf.value,
					       (uint16_t)index, &info, INVALID_IRTE_ID);
		if (ret == 0) {
			/* Write the table entry to the physical structure */
			pentry = get_msix_table_entry(vdev, index);

			/*
			 * PCI 3.0 Spec allows writing to Message Address and Message Upper Address
			 * fields with a single QWORD write, but some hardware can accept 32 bits
			 * write only
			 */
			stac();
			mmio_write32((uint32_t)(info.addr.full), (void *)&(pentry->addr));
			mmio_write32((uint32_t)(info.addr.full >> 32U), (void *)((char *)&(pentry->addr) + 4U));

			mmio_write32(info.data.full, (void *)&(pentry->data));
			mmio_write32(vdev->msix.table_entries[index].vector_control, (void *)&(pentry->vector_control));
			clac();
		}
	}

}

/**
 * @pre io_req != NULL
 * @pre priv_data != NULL
 */
static int32_t pt_vmsix_handle_table_mmio_access(struct io_request *io_req, void *priv_data)
{
	struct acrn_mmio_request *mmio = &io_req->reqs.mmio_request;
	struct pci_vdev *vdev;
	uint32_t index;
	int32_t ret = 0;

	vdev = (struct pci_vdev *)priv_data;
	if (vdev->user == vdev) {
		index = rw_vmsix_table(vdev, io_req);

		if ((mmio->direction == ACRN_IOREQ_DIR_WRITE) && (index < vdev->msix.table_count)) {
			if (vdev->msix.is_vmsix_on_msi) {
				remap_one_vmsix_entry_on_msi(vdev, index);
			} else {
				remap_one_vmsix_entry(vdev, index);
			}
		}
	} else {
		ret = -EFAULT;
	}

	return ret;
}

/*
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
static void vdev_pt_unmap_msix(struct pci_vdev *vdev)
{
	uint32_t i;
	uint64_t addr_hi, addr_lo;
	struct pci_msix *msix = &vdev->msix;

	/* Mask all table entries */
	for (i = 0U; i < msix->table_count; i++) {
		msix->table_entries[i].vector_control = PCIM_MSIX_VCTRL_MASK;
		msix->table_entries[i].addr = 0U;
		msix->table_entries[i].data = 0U;
	}

	if (msix->mmio_gpa != 0UL) {
		addr_lo = msix->mmio_gpa + msix->table_offset;
		addr_hi = addr_lo + (msix->table_count * MSIX_TABLE_ENTRY_SIZE);
		addr_lo = round_page_down(addr_lo);
		addr_hi = round_page_up(addr_hi);
		unregister_mmio_emulation_handler(vpci2vm(vdev->vpci), addr_lo, addr_hi);
		msix->mmio_gpa = 0UL;
	}
}

/*
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
void vdev_pt_map_msix(struct pci_vdev *vdev, bool hold_lock)
{
	struct pci_vbar *vbar;
	uint64_t addr_hi, addr_lo;
	struct pci_msix *msix = &vdev->msix;

	vbar = &vdev->vbars[msix->table_bar];
	if (vbar->base_gpa != 0UL) {
		struct acrn_vm *vm = vpci2vm(vdev->vpci);

		addr_lo = vbar->base_gpa + msix->table_offset;
		addr_hi = addr_lo + (msix->table_count * MSIX_TABLE_ENTRY_SIZE);
		addr_lo = round_page_down(addr_lo);
		addr_hi = round_page_up(addr_hi);
		register_mmio_emulation_handler(vm, pt_vmsix_handle_table_mmio_access,
				addr_lo, addr_hi, vdev, hold_lock);
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, addr_lo, addr_hi - addr_lo);
		msix->mmio_gpa = vbar->base_gpa;
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
static void vdev_pt_unmap_mem_vbar(struct pci_vdev *vdev, uint32_t idx)
{
	struct pci_vbar *vbar = &vdev->vbars[idx];

	if (vbar->base_gpa != 0UL) {
		struct acrn_vm *vm = vpci2vm(vdev->vpci);

		ept_del_mr(vm, (uint64_t *)(vm->arch_vm.nworld_eptp),
			vbar->base_gpa, /* GPA (old vbar) */
			vbar->size);
	}

	if ((has_msix_cap(vdev) && (idx == vdev->msix.table_bar))) {
		vdev_pt_unmap_msix(vdev);
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
static void vdev_pt_map_mem_vbar(struct pci_vdev *vdev, uint32_t idx)
{
	struct pci_vbar *vbar = &vdev->vbars[idx];

	if (vbar->base_gpa != 0UL) {
		struct acrn_vm *vm = vpci2vm(vdev->vpci);

		ept_add_mr(vm, (uint64_t *)(vm->arch_vm.nworld_eptp),
			vbar->base_hpa, /* HPA (pbar) */
			vbar->base_gpa, /* GPA (new vbar) */
			vbar->size,
			EPT_WR | EPT_RD | EPT_UNCACHED);
	}

	if (has_msix_cap(vdev) && (idx == vdev->msix.table_bar)) {
		vdev_pt_map_msix(vdev, true);
	}
}

/**
 * @brief Allow IO bar access
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
static void vdev_pt_allow_io_vbar(struct pci_vdev *vdev, uint32_t idx)
{
	struct acrn_vm *vm = vpci2vm(vdev->vpci);

	/* For Service VM, all port IO access is allowed by default, so skip Service VM here */
	if (!is_service_vm(vm)) {
		struct pci_vbar *vbar = &vdev->vbars[idx];
		if (vbar->base_gpa != 0UL) {
			allow_guest_pio_access(vm, (uint16_t)vbar->base_gpa, (uint32_t)(vbar->size));
		}
	}
}

/**
 * @brief Deny IO bar access
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
static void vdev_pt_deny_io_vbar(struct pci_vdev *vdev, uint32_t idx)
{
	struct acrn_vm *vm = vpci2vm(vdev->vpci);

	/* For Service VM, all port IO access is allowed by default, so skip Service VM here */
	if (!is_service_vm(vm)) {
		struct pci_vbar *vbar = &vdev->vbars[idx];
		if (vbar->base_gpa != 0UL) {
			deny_guest_pio_access(vm, (uint16_t)(vbar->base_gpa), (uint32_t)(vbar->size));
		}

	}
}

/**
 * @pre vdev != NULL
 */
void vdev_pt_write_vbar(struct pci_vdev *vdev, uint32_t idx, uint32_t val)
{
	struct pci_vbar *vbar = &vdev->vbars[idx];

	if (is_pci_io_bar(vbar)) {
		vpci_update_one_vbar(vdev, idx, val, vdev_pt_allow_io_vbar, vdev_pt_deny_io_vbar);
	} else {
		/* pci mem bar */
		vpci_update_one_vbar(vdev, idx, val, vdev_pt_map_mem_vbar, vdev_pt_unmap_mem_vbar);
	}
}

/*
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void vdev_bridge_pt_restore_space(struct pci_vdev *vdev)
{
	struct pci_pdev *pdev = vdev->pdev;
	uint32_t pre_val;
	uint32_t offset;

	/* I/O Base (0x1c) and I/O Limit (0x1d) */
	pre_val = pci_vdev_read_vcfg(vdev, PCIR_IO_BASE, 2U);
	if (pre_val != pci_pdev_read_cfg(vdev->pdev->bdf, PCIR_IO_BASE, 2U)) {
		pci_pdev_write_cfg(pdev->bdf, PCIR_IO_BASE, 2U, pre_val);
	}

	/* From Memory Base (0x20) to I/O Base Limit 16 Bits (0x32) */
	for (offset = PCIR_MEM_BASE; offset < PCIR_IO_BASE_UPPER_16; offset += 4) {
		pre_val = pci_vdev_read_vcfg(vdev, offset, 4U);
		if (pre_val != pci_pdev_read_cfg(vdev->pdev->bdf, offset, 4U)) {
			pci_pdev_write_cfg(pdev->bdf, offset, 4U, pre_val);
		}
	}
}

/*
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void vdev_bridge_pt_restore_bus(struct pci_vdev *vdev)
{
	struct pci_pdev *pdev = vdev->pdev;
	uint32_t pre_val;

	/* Primary Bus Number (0x18) and Secondary Bus Number (0x19) */
	pre_val = pci_vdev_read_vcfg(vdev, PCIR_PRIBUS_1, 2U);
	if (pre_val != pci_pdev_read_cfg(vdev->pdev->bdf, PCIR_PRIBUS_1, 2U)) {
		pci_pdev_write_cfg(pdev->bdf, PCIR_PRIBUS_1, 2U, pre_val);
	}

	/* Subordinate Bus Number (0x1a) */
	pre_val = pci_vdev_read_vcfg(vdev, PCIR_SUBBUS_1, 1U);
	if (pre_val != pci_pdev_read_cfg(vdev->pdev->bdf, PCIR_SUBBUS_1, 1U)) {
		pci_pdev_write_cfg(pdev->bdf, PCIR_SUBBUS_1, 1U, pre_val);
	}
}

/**
 * PCI base address register (bar) virtualization:
 *
 * Virtualize the PCI bars (up to 6 bars at byte offset 0x10~0x24 for type 0 PCI device,
 * 2 bars at byte offset 0x10-0x14 for type 1 PCI device) of the PCI configuration space
 * header.
 *
 * pbar: bar for the physical PCI device (pci_pdev), the value of pbar (hpa) is assigned
 * by platform firmware during boot. It is assumed a valid hpa is always assigned to a
 * mmio pbar, hypervisor shall not change the value of a pbar.
 *
 * vbar: for each pci_pdev, it has a virtual PCI device (pci_vdev) counterpart. pci_vdev
 * virtualizes all the bars (called vbars). a vbar can be initialized by hypervisor by
 * assigning a gpa to it; if vbar has a value of 0 (unassigned), guest may assign
 * and program a gpa to it. The guest only sees the vbars, it will not see and can
 * never change the pbars.
 *
 * Hypervisor traps guest changes to the mmio vbar (gpa) to establish ept mapping
 * between vbar(gpa) and pbar(hpa). pbar should always align on 4K boundary.
 *
 * @param vdev         Pointer to a vdev structure
 * @param is_sriov_bar When the first parameter vdev is a SRIOV PF vdev, the function
 *                     init_bars is used to initialize normal PCIe BARs of PF vdev if the
 *                     parameter is_sriov_bar is false, the function init_bars is used to
 *                     initialize SRIOV VF BARs of PF vdev if parameter is_sriov_bar is true
 *                     Otherwise, the parameter is_sriov_bar should be false if the first
 *                     parameter vdev is not SRIOV PF vdev
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->pdev != NULL
 */
static void init_bars(struct pci_vdev *vdev, bool is_sriov_bar)
{
	uint32_t idx, bar_cnt;
	struct pci_vbar *vbar;
	uint32_t size32, offset, lo, hi = 0U;
	union pci_bdf pbdf;
	uint64_t mask;

	if (is_sriov_bar) {
		bar_cnt = PCI_BAR_COUNT;
	} else {
		vdev->nr_bars = vdev->pdev->nr_bars;
		bar_cnt = vdev->nr_bars;
	}
	pbdf.value = vdev->pdev->bdf.value;

	for (idx = 0U; idx < bar_cnt; idx++) {
		if (is_sriov_bar) {
			vbar = &vdev->sriov.vbars[idx];
			offset = sriov_bar_offset(vdev, idx);
		} else {
			vbar = &vdev->vbars[idx];
			offset = pci_bar_offset(idx);
		}
		lo = pci_pdev_read_cfg(pbdf, offset, 4U);
		vbar->bar_type.bits = lo;

		if (is_pci_reserved_bar(vbar)) {
			continue;
		}

		if (is_pci_io_bar(vbar)) {
			if (lo & ~IO_SPACE_BITMASK) {
				/* 
				 * Some buggy x86 BIOS may program an invalid I/O BAR whose upper 16 bits are not zero.
				 * Such I/O BAR is not addressable on x86 platforms. Skip it when initializing the
				 * virtual PCI function as I/O BAR reprogramming in VM is currently unsupported.
				 */
				pr_warn("%s: %02x:%02x.%x: IO BAR%d value 0x%08x has invalid bits, IO_SPACE_BITMASK "
				        "is 0x%08x, Ignore this BAR in vdev",
					__func__, vdev->bdf.bits.b, vdev->bdf.bits.d, vdev->bdf.bits.f, idx, lo,
					IO_SPACE_BITMASK);
				continue;
			}
			mask = PCI_BASE_ADDRESS_IO_MASK;
		} else {
			mask = PCI_BASE_ADDRESS_MEM_MASK;
		}
		vbar->base_hpa = (uint64_t)lo & mask;

		if (is_pci_mem64lo_bar(vbar)) {
			hi = pci_pdev_read_cfg(pbdf, offset + 4U, 4U);
			vbar->base_hpa |= ((uint64_t)hi << 32U);
		}

		if (vbar->base_hpa != 0UL) {
			pci_pdev_write_cfg(pbdf, offset, 4U, ~0U);
			size32 = pci_pdev_read_cfg(pbdf, offset, 4U);
			pci_pdev_write_cfg(pbdf, offset, 4U, lo);

			vbar->mask = size32 & mask;
			vbar->bar_type.bits &= (uint32_t)(~mask);
			vbar->size = (uint64_t)size32 & mask;

			if (is_prelaunched_vm(vpci2vm(vdev->vpci))) {
				lo = (uint32_t)vdev->pci_dev_config->vbar_base[idx];
			}

			if (is_pci_mem64lo_bar(vbar)) {
				idx++;
				if (is_sriov_bar) {
					offset = sriov_bar_offset(vdev, idx);
				} else {
					offset = pci_bar_offset(idx);
				}
				pci_pdev_write_cfg(pbdf, offset, 4U, ~0U);
				size32 = pci_pdev_read_cfg(pbdf, offset, 4U);
				pci_pdev_write_cfg(pbdf, offset, 4U, hi);

				vbar->size |= ((uint64_t)size32 << 32U);
				vbar->size = vbar->size & ~(vbar->size - 1UL);
				vbar->size = round_page_up(vbar->size);

				if (is_sriov_bar) {
					vbar = &vdev->sriov.vbars[idx];
				} else {
					vbar = &vdev->vbars[idx];
				}

				vbar->mask = size32;
				vbar->is_mem64hi = true;

				if (is_prelaunched_vm(vpci2vm(vdev->vpci))) {
					hi = (uint32_t)(vdev->pci_dev_config->vbar_base[idx - 1U] >> 32U);
				}
				/* if it is parsing SRIOV VF BARs, no need to write vdev bars */
				if (!is_sriov_bar) {
					pci_vdev_write_vbar(vdev, idx - 1U, lo);
					pci_vdev_write_vbar(vdev, idx, hi);
				}
			} else {
				vbar->size = vbar->size & ~(vbar->size - 1UL);
				if (is_pci_mem32_bar(vbar)) {
					vbar->size = round_page_up(vbar->size);
				}

				/* if it is parsing SRIOV VF BARs, no need to write vdev bar */
				if (!is_sriov_bar) {
					pci_vdev_write_vbar(vdev, idx, lo);
				}
			}
		}
	}

	/* Initialize MSIx mmio hpa and size after BARs initialization */
	if (has_msix_cap(vdev) && (!is_sriov_bar)) {
		vdev->msix.mmio_hpa = vdev->vbars[vdev->msix.table_bar].base_hpa;
		vdev->msix.mmio_size = vdev->vbars[vdev->msix.table_bar].size;
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->pdev != NULL
 */
void init_vmsix_pt(struct pci_vdev *vdev)
{
	struct pci_pdev *pdev = vdev->pdev;

	vdev->msix.capoff = pdev->msix.capoff;
	vdev->msix.caplen = pdev->msix.caplen;
	vdev->msix.table_bar = pdev->msix.table_bar;
	vdev->msix.table_offset = pdev->msix.table_offset;
	vdev->msix.table_count = pdev->msix.table_count;

	if (has_msix_cap(vdev)) {
		(void)memcpy_s((void *)&vdev->cfgdata.data_8[pdev->msix.capoff], pdev->msix.caplen,
			(void *)&pdev->msix.cap[0U], pdev->msix.caplen);
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
void deinit_vmsix_pt(struct pci_vdev *vdev)
{
	if (has_msix_cap(vdev)) {
		if (vdev->msix.table_count != 0U) {
			ptirq_remove_msix_remapping(vpci2vm(vdev->vpci), vdev->pdev->bdf.value, vdev->msix.table_count);
			(void)memset((void *)&vdev->msix.table_entries, 0U, sizeof(vdev->msix.table_entries));
			vdev->msix.is_vmsix_on_msi_programmed = false;
		}
	}
}

void vdev_pt_hide_sriov_cap(struct pci_vdev *vdev)
{
	uint32_t pre_pos = vdev->pdev->sriov.pre_pos;
	uint32_t pre_hdr, hdr, vhdr;

	pre_hdr = pci_pdev_read_cfg(vdev->pdev->bdf, pre_pos, 4U);
	hdr = pci_pdev_read_cfg(vdev->pdev->bdf, vdev->pdev->sriov.capoff, 4U);

	vhdr = pre_hdr & 0xfffffU;
	vhdr |= hdr & 0xfff00000U;
	pci_vdev_write_vcfg(vdev, pre_pos, 4U, vhdr);
	vdev->pdev->sriov.hide_sriov = true;

	pr_acrnlog("Hide sriov cap for %02x:%02x.%x", vdev->pdev->bdf.bits.b, vdev->pdev->bdf.bits.d, vdev->pdev->bdf.bits.f);
}

/* TODO:
 * The OpRegion is not 4KB aligned, while under some platforms,
 * it will take up to 16KB. In this case, OpRegion overlay 5 pages.
 * So set GPU_OPREGION_SIZE to 0x5000U(20KB) here.
 *
 * The solution that pass-thru OpRegion has potential security issue.
 * Will take the copy + emulation solution to expose host OpRegion to guest later.
 */
void passthru_gpu_opregion(struct pci_vdev *vdev)
{
	uint32_t gpu_opregion_hpa, gpu_opregion_gpa, gpu_asls_phys;

	gpu_opregion_gpa = GPU_OPREGION_GPA;
	gpu_asls_phys = pci_pdev_read_cfg(vdev->pdev->bdf, PCIR_ASLS_CTL, 4U);
	gpu_opregion_hpa = gpu_asls_phys & PCIM_ASLS_OPREGION_MASK;
	ept_add_mr(vpci2vm(vdev->vpci), vpci2vm(vdev->vpci)->arch_vm.nworld_eptp,
			gpu_opregion_hpa, gpu_opregion_gpa,
			GPU_OPREGION_SIZE, EPT_RD | EPT_UNCACHED);
	pci_vdev_write_vcfg(vdev, PCIR_ASLS_CTL, 4U, gpu_opregion_gpa | (gpu_asls_phys & ~PCIM_ASLS_OPREGION_MASK));
}

/**
 * @brief Initialize a specified passthrough vdev structure.
 *
 * The function init_vdev_pt is used to initialize a vdev structure. If a vdev structure supports
 * SRIOV capability that the vdev represents a SRIOV physical function(PF) virtual device, then
 * function init_vdev_pt can initialize PF vdev SRIOV capability if parameter is_pf_vdev is true.
 *
 * @param vdev        pointer to vdev data structure
 * @param is_pf_vdev  indicate the first parameter vdev is the data structure of a PF, which contains
 *                    the SR-IOV capability
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->pdev != NULL
 */
void init_vdev_pt(struct pci_vdev *vdev, bool is_pf_vdev)
{
	uint16_t pci_command;
	uint32_t offset;

	for (offset = 0U; offset < PCI_CFG_HEADER_LENGTH; offset += 4U) {
		pci_vdev_write_vcfg(vdev, offset, 4U, pci_pdev_read_cfg(vdev->pdev->bdf, offset, 4U));
	}

	/* Initialize the vdev BARs except SRIOV VF, VF BARs are initialized directly from create_vf function */
	if (vdev->phyfun == NULL) {
		init_bars(vdev, is_pf_vdev);
		init_vmsix_on_msi(vdev);
		if (is_service_vm(vpci2vm(vdev->vpci)) && (vdev->pdev->bdf.value == CONFIG_IGD_SBDF)) {
			pci_vdev_write_vcfg(vdev, PCIR_ASLS_CTL, 4U, pci_pdev_read_cfg(vdev->pdev->bdf, PCIR_ASLS_CTL, 4U));
		}
		if (is_prelaunched_vm(vpci2vm(vdev->vpci)) && (!is_pf_vdev)) {
			pci_command = (uint16_t)pci_pdev_read_cfg(vdev->pdev->bdf, PCIR_COMMAND, 2U);

			/* Disable INTX */
			pci_command |= 0x400U;
			pci_pdev_write_cfg(vdev->pdev->bdf, PCIR_COMMAND, 2U, pci_command);

			if (vdev->pdev->bdf.value == CONFIG_IGD_SBDF) {
				passthru_gpu_opregion(vdev);
			}
		}
	} else {
		if (vdev->phyfun->vpci != vdev->vpci) {
			/* VF is assigned to a User VM */
			uint32_t vid, did;

			vdev->nr_bars = PCI_BAR_COUNT;
			/* SRIOV VF Vendor ID and Device ID initialization */
			vid = pci_pdev_read_cfg(vdev->phyfun->bdf, PCIR_VENDOR, 2U);
			did = pci_pdev_read_cfg(vdev->phyfun->bdf,
				(vdev->phyfun->sriov.capoff + PCIR_SRIOV_VF_DEV_ID), 2U);
			pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, vid);
			pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, did);
		} else {
			/* VF is unassinged: when VF was first created, the VF's BARs hasn't been assigned */
			uint32_t bar_idx;

			for (bar_idx = 0U; bar_idx < vdev->nr_bars; bar_idx++) {
				vdev_pt_map_mem_vbar(vdev, bar_idx);
			}
		}
	}

	if (!is_service_vm(vpci2vm(vdev->vpci)) && (has_sriov_cap(vdev))) {
		vdev_pt_hide_sriov_cap(vdev);
	}

}

/**
 * @brief Destruct a specified passthrough vdev structure.
 *
 * The function deinit_vdev_pt is the destructor corresponding to the function init_vdev_pt.
 *
 * @param vdev  pointer to vdev data structure
 *
 * @pre vdev != NULL
 */
void deinit_vdev_pt(struct pci_vdev *vdev) {

	/* Check if the vdev is an unassigned SR-IOV VF device */
	if ((vdev->phyfun != NULL) && (vdev->phyfun->vpci == vdev->vpci)) {
		uint32_t bar_idx;

		/* Delete VF MMIO from EPT table since the VF physical device has gone */
		for (bar_idx = 0U; bar_idx < vdev->nr_bars; bar_idx++) {
			vdev_pt_unmap_mem_vbar(vdev, bar_idx);
		}
	}
}
