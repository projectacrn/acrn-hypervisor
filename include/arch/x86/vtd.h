/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VTD_H
#define VTD_H
/*
 * Intel IOMMU register specification per version 1.0 public spec.
 */

#define DMAR_VER_REG    0x0 /* Arch version supported by this IOMMU */
#define DMAR_CAP_REG    0x8 /* Hardware supported capabilities */
#define DMAR_ECAP_REG   0x10    /* Extended capabilities supported */
#define DMAR_GCMD_REG   0x18    /* Global command register */
#define DMAR_GSTS_REG   0x1c    /* Global status register */
#define DMAR_RTADDR_REG 0x20    /* Root entry table */
#define DMAR_CCMD_REG   0x28    /* Context command reg */
#define DMAR_FSTS_REG   0x34    /* Fault Status register */
#define DMAR_FECTL_REG  0x38    /* Fault control register */
#define DMAR_FEDATA_REG 0x3c    /* Fault event interrupt data register */
#define DMAR_FEADDR_REG 0x40    /* Fault event interrupt addr register */
#define DMAR_FEUADDR_REG 0x44   /* Upper address register */
#define DMAR_AFLOG_REG  0x58    /* Advanced Fault control */
#define DMAR_PMEN_REG   0x64    /* Enable Protected Memory Region */
#define DMAR_PLMBASE_REG 0x68   /* PMRR Low addr */
#define DMAR_PLMLIMIT_REG 0x6c  /* PMRR low limit */
#define DMAR_PHMBASE_REG 0x70   /* pmrr high base addr */
#define DMAR_PHMLIMIT_REG 0x78  /* pmrr high limit */
#define DMAR_IQH_REG    0x80    /* Invalidation queue head register */
#define DMAR_IQT_REG    0x88    /* Invalidation queue tail register */
#define DMAR_IQ_SHIFT   4   /* Invalidation queue head/tail shift */
#define DMAR_IQA_REG    0x90    /* Invalidation queue addr register */
#define DMAR_ICS_REG    0x9c    /* Invalidation complete status register */
#define DMAR_IRTA_REG   0xb8    /* Interrupt remapping table addr register */

#define DMAR_VER_MAJOR(v)       (((v) & 0xf0) >> 4)
#define DMAR_VER_MINOR(v)       ((v) & 0x0f)

/*
 * Decoding Capability Register
 */
#define iommu_cap_pi(c)			(((c) >> 59) & 1)
#define iommu_cap_read_drain(c)   (((c) >> 55) & 1)
#define iommu_cap_write_drain(c)  (((c) >> 54) & 1)
#define iommu_cap_max_amask_val(c)    (((c) >> 48) & 0x3f)
#define iommu_cap_num_fault_regs(c)   ((((c) >> 40) & 0xff) + 1)
#define iommu_cap_pgsel_inv(c)    (((c) >> 39) & 1)

#define iommu_cap_super_page_val(c)   (((c) >> 34) & 0xf)
#define iommu_cap_super_offset(c) \
	(((find_first_bit(&iommu_cap_super_page_val(c), 4)) \
	* OFFSET_STRIDE) + 21)

#define iommu_cap_fault_reg_offset(c) ((((c) >> 24) & 0x3ff) * 16)
#define iommu_cap_max_fault_reg_offset(c) \
	(iommu_cap_fault_reg_offset(c) + iommu_cap_num_fault_regs(c) * 16)

#define iommu_cap_zlr(c)      (((c) >> 22) & 1)
#define iommu_cap_isoch(c)        (((c) >> 23) & 1)
#define iommu_cap_mgaw(c)     ((((c) >> 16) & 0x3f) + 1)
#define iommu_cap_sagaw(c)        (((c) >> 8) & 0x1f)
#define iommu_cap_caching_mode(c) (((c) >> 7) & 1)
#define iommu_cap_phmr(c)     (((c) >> 6) & 1)
#define iommu_cap_plmr(c)     (((c) >> 5) & 1)
#define iommu_cap_rwbf(c)     (((c) >> 4) & 1)
#define iommu_cap_afl(c)      (((c) >> 3) & 1)
#define iommu_cap_ndoms(c)        (((unsigned long)1) << (4 + 2 * ((c) & 0x7)))

/*
 * Decoding Extended Capability Register
 */
#define iommu_ecap_c(c)		(((c) >> 0) & 1)
#define iommu_ecap_qi(c)		(((c) >> 1) & 1)
#define iommu_ecap_dt(c)		(((c) >> 2) & 1)
#define iommu_ecap_ir(c)		(((c) >> 3) & 1)
#define iommu_ecap_eim(c)		(((c) >> 4) & 1)
#define iommu_ecap_pt(c)		(((c) >> 6) & 1)
#define iommu_ecap_sc(c)		(((c) >> 7) & 1)
#define iommu_ecap_iro(c)		(((c) >> 8) & 0x3ff)
#define iommu_ecap_mhmv(c)	(((c) >> 20) & 0xf)
#define iommu_ecap_ecs(c)		(((c) >> 24) & 1)
#define iommu_ecap_mts(c)		(((c) >> 25) & 1)
#define iommu_ecap_nest(c)	(((c) >> 26) & 1)
#define iommu_ecap_dis(c)		(((c) >> 27) & 1)
#define iommu_ecap_prs(c)		(((c) >> 29) & 1)
#define iommu_ecap_ers(c)		(((c) >> 30) & 1)
#define iommu_ecap_srs(c)		(((c) >> 31) & 1)
#define iommu_ecap_nwfs(c)	(((c) >> 33) & 1)
#define iommu_ecap_eafs(c)	(((c) >> 34) & 1)
#define iommu_ecap_pss(c)		(((c) >> 35) & 0x1f)
#define iommu_ecap_pasid(c)	(((c) >> 40) & 1)
#define iommu_ecap_dit(c)		(((c) >> 41) & 1)
#define iommu_ecap_pds(c)		(((c) >> 42) & 1)

/* PMEN_REG */
#define DMA_PMEN_EPM (((uint32_t)1)<<31)
#define DMA_PMEN_PRS (((uint32_t)1)<<0)

/* GCMD_REG */
#define DMA_GCMD_TE (((uint32_t)1) << 31)
#define DMA_GCMD_SRTP (((uint32_t)1) << 30)
#define DMA_GCMD_SFL (((uint32_t)1) << 29)
#define DMA_GCMD_EAFL (((uint32_t)1) << 28)
#define DMA_GCMD_WBF (((uint32_t)1) << 27)
#define DMA_GCMD_QIE (((uint32_t)1) << 26)
#define DMA_GCMD_SIRTP (((uint32_t)1) << 24)
#define DMA_GCMD_IRE (((uint32_t) 1) << 25)
#define DMA_GCMD_CFI (((uint32_t) 1) << 23)

/* GSTS_REG */
#define DMA_GSTS_TES (((uint32_t)1) << 31)
#define DMA_GSTS_RTPS (((uint32_t)1) << 30)
#define DMA_GSTS_FLS (((uint32_t)1) << 29)
#define DMA_GSTS_AFLS (((uint32_t)1) << 28)
#define DMA_GSTS_WBFS (((uint32_t)1) << 27)
#define DMA_GSTS_QIES (((uint32_t)1) << 26)
#define DMA_GSTS_IRTPS (((uint32_t)1) << 24)
#define DMA_GSTS_IRES (((uint32_t)1) << 25)
#define DMA_GSTS_CFIS (((uint32_t)1) << 23)

/* CCMD_REG */
#define DMA_CCMD_ICC (((uint64_t)1) << 63)
#define DMA_CCMD_ICC_32 (((uint32_t)1) << 31)
#define DMA_CCMD_GLOBAL_INVL (((uint64_t)1) << 61)
#define DMA_CCMD_DOMAIN_INVL (((uint64_t)2) << 61)
#define DMA_CCMD_DEVICE_INVL (((uint64_t)3) << 61)
#define DMA_CCMD_FM(m) (((uint64_t)((m) & 0x3)) << 32)
#define DMA_CCMD_MASK_NOBIT 0
#define DMA_CCMD_MASK_1BIT 1
#define DMA_CCMD_MASK_2BIT 2
#define DMA_CCMD_MASK_3BIT 3
#define DMA_CCMD_SID(s) (((uint64_t)((s) & 0xffff)) << 16)
#define DMA_CCMD_DID(d) ((uint64_t)((d) & 0xffff))
#define DMA_CCMD_GET_CAIG_32(v) (((uint32_t)(v) >> 27) & 0x3)

/* IOTLB_REG */
#define DMA_IOTLB_IVT				(((uint64_t)1) << 63)
#define DMA_IOTLB_IVT_32			(((uint32_t)1) << 31)
#define DMA_IOTLB_GLOBAL_INVL		(((uint64_t)1) << 60)
#define DMA_IOTLB_DOMAIN_INVL		(((uint64_t)2) << 60)
#define DMA_IOTLB_PAGE_INVL			(((uint64_t)3) << 60)
#define DMA_IOTLB_DR				(((uint64_t)1) << 49)
#define DMA_IOTLB_DW				(((uint64_t)1) << 48)
#define DMA_IOTLB_DID(d)			 \
	(((uint64_t)((d) & 0xffff)) << 32)
#define DMA_IOTLB_GET_IAIG_32(v)	(((uint32_t)(v) >> 25) & 0x3)

/* INVALIDATE_ADDRESS_REG */
#define DMA_IOTLB_INVL_ADDR_AM(m)			((uint64_t)((m) & 0x3f))
#define DMA_IOTLB_INVL_ADDR_IH_UNMODIFIED	(((uint64_t)1) << 6)

/* FECTL_REG */
#define DMA_FECTL_IM				(((uint32_t)1) << 31)

/* FSTS_REG */
#define DMA_FSTS_PFO(s)				(((s) >> 0) & 1)
#define DMA_FSTS_PPF(s)				(((s) >> 1) & 1)
#define DMA_FSTS_AFO(s)				(((s) >> 2) & 1)
#define DMA_FSTS_APF(s)				(((s) >> 3) & 1)
#define DMA_FSTS_IQE(s)				(((s) >> 4) & 1)
#define DMA_FSTS_ICE(s)				(((s) >> 5) & 1)
#define DMA_FSTS_ITE(s)				(((s) >> 6) & 1)
#define DMA_FSTS_PRO(s)				(((s) >> 7) & 1)
#define DMA_FSTS_FRI(s)				(((s) >> 8) & 0xFF)

/* FRCD_REGs: upper 64 bits*/
#define DMA_FRCD_UP_F(r)				(((r) >> 63) & 1)
#define DMA_FRCD_UP_T(r)				(((r) >> 62) & 1)
#define DMA_FRCD_UP_AT(r)				(((r) >> 60) & 3)
#define DMA_FRCD_UP_PASID(r)			(((r) >> 40) & 0xfffff)
#define DMA_FRCD_UP_FR(r)				(((r) >> 32) & 0xff)
#define DMA_FRCD_UP_PP(r)				(((r) >> 31) & 1)
#define DMA_FRCD_UP_EXE(r)				(((r) >> 30) & 1)
#define DMA_FRCD_UP_PRIV(r)				(((r) >> 29) & 1)
#define DMA_FRCD_UP_SID(r)				(((r) >> 0) & 0xffff)

#define DMAR_CONTEXT_TRANSLATION_TYPE_TRANSLATED 0x00
#define DMAR_CONTEXT_TRANSLATION_TYPE_RESERVED 0x01
#define DMAR_CONTEXT_TRANSLATION_TYPE_PASSED_THROUGH 0x02

#define DRHD_FLAG_INCLUDE_PCI_ALL_MASK      (1)

#define DEVFUN(dev, fun)            (((dev & 0x1F) << 3) | ((fun & 0x7)))

struct dmar_dev_scope {
	uint8_t bus;
	uint8_t devfun;
};

struct dmar_drhd {
	uint32_t dev_cnt;
	uint16_t segment;
	uint8_t flags;
	bool ignore;
	uint64_t reg_base_addr;
	/* assume no pci device hotplug support */
	struct dmar_dev_scope *devices;
};

struct dmar_info {
	uint32_t drhd_count;
	struct dmar_drhd *drhd_units;
};

extern struct dmar_info *get_dmar_info(void);

struct iommu_domain;

/* Assign a device specified by bus & devfun to a iommu domain */
int assign_iommu_device(struct iommu_domain *domain,
	uint8_t bus, uint8_t devfun);

/* Unassign a device specified by bus & devfun to a iommu domain */
int unassign_iommu_device(struct iommu_domain *domain,
	uint8_t bus, uint8_t devfun);

/* Create a iommu domain for a VM specified by vm_id */
struct iommu_domain *create_iommu_domain(int vm_id,
	uint64_t translation_table, int addr_width);

/* Destroy the iommu domain */
int destroy_iommu_domain(struct iommu_domain *domain);

/* Enable translation of iommu*/
void enable_iommu(void);

/* Disable translation of iommu*/
void disable_iommu(void);

/* iommu initialization */
int init_iommu(void);
#endif
