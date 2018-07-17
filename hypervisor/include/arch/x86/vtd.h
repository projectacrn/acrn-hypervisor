/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VTD_H
#define VTD_H
/*
 * Intel IOMMU register specification per version 1.0 public spec.
 */

#define DMAR_VER_REG    0x0U /* Arch version supported by this IOMMU */
#define DMAR_CAP_REG    0x8U /* Hardware supported capabilities */
#define DMAR_ECAP_REG   0x10U    /* Extended capabilities supported */
#define DMAR_GCMD_REG   0x18U    /* Global command register */
#define DMAR_GSTS_REG   0x1cU    /* Global status register */
#define DMAR_RTADDR_REG 0x20U    /* Root entry table */
#define DMAR_CCMD_REG   0x28U    /* Context command reg */
#define DMAR_FSTS_REG   0x34U    /* Fault Status register */
#define DMAR_FECTL_REG  0x38U    /* Fault control register */
#define DMAR_FEDATA_REG 0x3cU    /* Fault event interrupt data register */
#define DMAR_FEADDR_REG 0x40U    /* Fault event interrupt addr register */
#define DMAR_FEUADDR_REG 0x44U   /* Upper address register */
#define DMAR_AFLOG_REG  0x58U    /* Advanced Fault control */
#define DMAR_PMEN_REG   0x64U    /* Enable Protected Memory Region */
#define DMAR_PLMBASE_REG 0x68U   /* PMRR Low addr */
#define DMAR_PLMLIMIT_REG 0x6cU  /* PMRR low limit */
#define DMAR_PHMBASE_REG 0x70U   /* pmrr high base addr */
#define DMAR_PHMLIMIT_REG 0x78U  /* pmrr high limit */
#define DMAR_IQH_REG    0x80U    /* Invalidation queue head register */
#define DMAR_IQT_REG    0x88U    /* Invalidation queue tail register */
#define DMAR_IQ_SHIFT   4   /* Invalidation queue head/tail shift */
#define DMAR_IQA_REG    0x90U    /* Invalidation queue addr register */
#define DMAR_ICS_REG    0x9cU    /* Invalidation complete status register */
#define DMAR_IRTA_REG   0xb8U    /* Interrupt remapping table addr register */

static inline uint8_t dmar_ver_major(uint64_t version)
{
	return ((version & 0xf0UL) >> 4U);
}

static inline uint8_t dmar_ver_minor(uint64_t version)
{
	return (version & 0x0fUL);
}

/*
 * Decoding Capability Register
 */
static inline uint8_t iommu_cap_pi(uint64_t cap)
{
	return ((cap >> 59U) & 1UL);
}

static inline uint8_t iommu_cap_read_drain(uint64_t cap)
{
	return ((cap >> 55U) & 1UL);
}

static inline uint8_t iommu_cap_write_drain(uint64_t cap)
{
	return ((cap >> 54U) & 1UL);
}

static inline uint8_t iommu_cap_max_amask_val(uint64_t cap)
{
	return ((cap >> 48U) & 0x3fUL);
}

static inline uint16_t iommu_cap_num_fault_regs(uint64_t cap)
{
	return (((cap >> 40U) & 0xffUL) + 1UL);
}

static inline uint8_t iommu_cap_pgsel_inv(uint64_t cap)
{
	return ((cap >> 39U) & 1UL);
}

static inline uint8_t iommu_cap_super_page_val(uint64_t cap)
{
	return ((cap >> 34U) & 0xfUL);
}

static inline uint16_t iommu_cap_fault_reg_offset(uint64_t cap)
{
	return (((cap >> 24U) & 0x3ffUL) * 16U);
}

static inline uint16_t iommu_cap_max_fault_reg_offset(uint64_t cap)
{
	return (iommu_cap_fault_reg_offset(cap) +
		iommu_cap_num_fault_regs(cap) * 16U);
}

static inline uint8_t iommu_cap_zlr(uint64_t cap)
{
	return ((cap >> 22U) & 1UL);
}

static inline uint8_t iommu_cap_isoch(uint64_t cap)
{
	return ((cap >> 23U) & 1UL);
}

static inline uint8_t iommu_cap_mgaw(uint64_t cap)
{
	return (((cap >> 16U) & 0x3fUL) + 1UL);
}

static inline uint8_t iommu_cap_sagaw(uint64_t cap)
{
	return ((cap >> 8U) & 0x1fUL);
}

static inline uint8_t iommu_cap_caching_mode(uint64_t cap)
{
	return ((cap >> 7U) & 1UL);
}

static inline uint8_t iommu_cap_phmr(uint64_t cap)
{
	return ((cap >> 6U) & 1UL);
}

static inline uint8_t iommu_cap_plmr(uint64_t cap)
{
	return ((cap >> 5U) & 1UL);
}

static inline uint8_t iommu_cap_rwbf(uint64_t cap)
{
	return ((cap >> 4U) & 1UL);
}

static inline uint8_t iommu_cap_afl(uint64_t cap)
{
	return ((cap >> 3U) & 1UL);
}

static inline uint32_t iommu_cap_ndoms(uint64_t cap)
{
	return ((1U) << (4UL + 2UL * (cap & 0x7UL)));
}

/*
 * Decoding Extended Capability Register
 */
static inline uint8_t iommu_ecap_c(uint64_t ecap)
{
	return ((ecap >> 0U) & 1UL);
}

static inline uint8_t iommu_ecap_qi(uint64_t ecap)
{
	return ((ecap >> 1U) & 1UL);
}

static inline uint8_t iommu_ecap_dt(uint64_t ecap)
{
	return ((ecap >> 2U) & 1UL);
}

static inline uint8_t iommu_ecap_ir(uint64_t ecap)
{
	return ((ecap >> 3U) & 1UL);
}

static inline uint8_t iommu_ecap_eim(uint64_t ecap)
{
	return ((ecap >> 4U) & 1UL);
}

static inline uint8_t iommu_ecap_pt(uint64_t ecap)
{
	return ((ecap >> 6U) & 1UL);
}

static inline uint8_t iommu_ecap_sc(uint64_t ecap)
{
	return ((ecap >> 7U) & 1UL);
}

static inline uint16_t iommu_ecap_iro(uint64_t ecap)
{
	return ((ecap >> 8U) & 0x3ffUL);
}

static inline uint8_t iommu_ecap_mhmv(uint64_t ecap)
{
	return ((ecap >> 20U) & 0xfUL);
}

static inline uint8_t iommu_ecap_ecs(uint64_t ecap)
{
	return ((ecap >> 24U) & 1UL);
}

static inline uint8_t iommu_ecap_mts(uint64_t ecap)
{
	return ((ecap >> 25U) & 1UL);
}

static inline uint8_t iommu_ecap_nest(uint64_t ecap)
{
	return ((ecap >> 26U) & 1UL);
}

static inline uint8_t iommu_ecap_dis(uint64_t ecap)
{
	return ((ecap >> 27U) & 1UL);
}

static inline uint8_t iommu_ecap_prs(uint64_t ecap)
{
	return ((ecap >> 29U) & 1UL);
}

static inline uint8_t iommu_ecap_ers(uint64_t ecap)
{
	return ((ecap >> 30U) & 1UL);
}

static inline uint8_t iommu_ecap_srs(uint64_t ecap)
{
	return ((ecap >> 31U) & 1UL);
}

static inline uint8_t iommu_ecap_nwfs(uint64_t ecap)
{
	return ((ecap >> 33U) & 1UL);
}

static inline uint8_t iommu_ecap_eafs(uint64_t ecap)
{
	return ((ecap >> 34U) & 1UL);
}

static inline uint8_t iommu_ecap_pss(uint64_t ecap)
{
	return ((ecap >> 35U) & 0x1fUL);
}

static inline uint8_t iommu_ecap_pasid(uint64_t ecap)
{
	return ((ecap >> 40U) & 1UL);
}

static inline uint8_t iommu_ecap_dit(uint64_t ecap)
{
	return ((ecap >> 41U) & 1UL);
}

static inline uint8_t iommu_ecap_pds(uint64_t ecap)
{
	return ((ecap >> 42U) & 1UL);
}

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
#define DMA_CCMD_ICC (((uint64_t)1UL) << 63)
#define DMA_CCMD_ICC_32 (((uint32_t)1UL) << 31)
#define DMA_CCMD_GLOBAL_INVL (((uint64_t)1UL) << 61)
#define DMA_CCMD_DOMAIN_INVL (((uint64_t)2UL) << 61)
#define DMA_CCMD_DEVICE_INVL (((uint64_t)3UL) << 61)
static inline uint64_t dma_ccmd_fm(uint8_t fm)
{
	return (((uint64_t)(fm & 0x3U)) << 32U);
}

#define DMA_CCMD_MASK_NOBIT 0UL
#define DMA_CCMD_MASK_1BIT 1UL
#define DMA_CCMD_MASK_2BIT 2UL
#define DMA_CCMD_MASK_3BIT 3UL
static inline uint64_t dma_ccmd_sid(uint16_t sid)
{
	return (((uint64_t)(sid & 0xffffU)) << 16U);
}

static inline uint16_t dma_ccmd_did(uint16_t did)
{
	return (did & 0xffffU);
}

static inline uint8_t dma_ccmd_get_caig_32(uint32_t gaig)
{
	return ((gaig >> 27U) & 0x3U);
}


/* IOTLB_REG */
#define DMA_IOTLB_IVT				(((uint64_t)1UL) << 63)
#define DMA_IOTLB_IVT_32			(((uint32_t)1U) << 31)
#define DMA_IOTLB_GLOBAL_INVL		(((uint64_t)1UL) << 60)
#define DMA_IOTLB_DOMAIN_INVL		(((uint64_t)2UL) << 60)
#define DMA_IOTLB_PAGE_INVL			(((uint64_t)3UL) << 60)
#define DMA_IOTLB_DR				(((uint64_t)1UL) << 49)
#define DMA_IOTLB_DW				(((uint64_t)1UL) << 48)
static inline uint64_t dma_iotlb_did(uint16_t did)
{
	return (((uint64_t)(did & 0xffffU)) << 32U);
}

static inline uint8_t dma_iotlb_get_iaig_32(uint32_t iai)
{
	return ((iai >> 25U) & 0x3U);
}

/* INVALIDATE_ADDRESS_REG */
static inline uint8_t dma_iotlb_invl_addr_am(uint8_t am)
{
	return (am & 0x3fU);
}

#define DMA_IOTLB_INVL_ADDR_IH_UNMODIFIED	(((uint64_t)1UL) << 6)

/* FECTL_REG */
#define DMA_FECTL_IM				(((uint32_t)1U) << 31)

/* FSTS_REG */
static inline bool dma_fsts_pfo(uint32_t PFO)
{
	return ((PFO >> 0U) & 1U) == 1U;
}

static inline bool dma_fsts_ppf(uint32_t PPF)
{
	return ((PPF >> 1U) & 1U) == 1U;
}

static inline bool dma_fsts_afo(uint32_t AFO)
{
	return ((AFO >> 2U) & 1U) == 1U;
}

static inline bool dma_fsts_apf(uint32_t APF)
{
	return ((APF >> 3U) & 1U) == 1U;
}

static inline bool dma_fsts_iqe(uint32_t IQE)
{
	return ((IQE >> 4U) & 1U) == 1U;
}

static inline bool dma_fsts_ice(uint32_t ICE)
{
	return ((ICE >> 5U) & 1U) == 1U;
}

static inline bool dma_fsts_ite(uint32_t ITE)
{
	return ((ITE >> 6U) & 1U) == 1U;
}

static inline bool dma_fsts_pro(uint32_t PRO)
{
	return ((PRO >> 7U) & 1U) == 1U;
}

static inline uint8_t dma_fsts_fri(uint32_t FRI)
{
	return ((FRI >> 8U) & 0xFFU);
}

/* FRCD_REGs: upper 64 bits*/
static inline bool dma_frcd_up_f(uint64_t UP_F)
{
	return ((UP_F >> 63U) & 1UL) == 1UL;
}

static inline uint8_t dma_frcd_up_t(uint64_t UP_T)
{
	return ((UP_T >> 62U) & 1UL);
}

static inline uint8_t dma_frcd_up_at(uint64_t UP_AT)
{
	return ((UP_AT >> 60U) & 3UL);
}

static inline uint32_t dma_frcd_up_pasid(uint64_t UP_PASID)
{
	return ((UP_PASID >> 40U) & 0xfffffUL);
}

static inline uint8_t dma_frcd_up_fr(uint64_t UP_FR)
{
	return ((UP_FR >> 32U) & 0xffUL);
}

static inline bool dma_frcd_up_pp(uint64_t UP_PP)
{
	return ((UP_PP >> 31U) & 1UL) == 1UL;
}

static inline bool dma_frcd_up_exe(uint64_t UP_EXE)
{
	return ((UP_EXE >> 30U) & 1UL) == 1UL;
}

static inline bool dma_frcd_up_priv(uint64_t UP_PRIV)
{
	return ((UP_PRIV >> 29U) & 1UL) == 1UL;
}

static inline uint32_t dma_frcd_up_sid(uint64_t UP_SID)
{
	return ((UP_SID >> 0U) & 0xffffUL);
}

#define DMAR_CONTEXT_TRANSLATION_TYPE_TRANSLATED 0x00U
#define DMAR_CONTEXT_TRANSLATION_TYPE_RESERVED 0x01U
#define DMAR_CONTEXT_TRANSLATION_TYPE_PASSED_THROUGH 0x02U

#define DRHD_FLAG_INCLUDE_PCI_ALL_MASK      (1U)

#define DEVFUN(dev, fun)            (((dev & 0x1FU) << 3) | ((fun & 0x7U)))

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
struct iommu_domain *create_iommu_domain(uint16_t vm_id,
	uint64_t translation_table, uint32_t addr_width);

/* Destroy the iommu domain */
int destroy_iommu_domain(struct iommu_domain *domain);

/* Enable translation of iommu*/
void enable_iommu(void);

/* Disable translation of iommu*/
void disable_iommu(void);

/* suspend iomu */
void suspend_iommu(void);

/* resume iomu */
void resume_iommu(void);

/* iommu initialization */
int init_iommu(void);
#endif
