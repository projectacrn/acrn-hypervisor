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

#define pr_fmt(fmt)	"iommu: " fmt

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>

#define DBG_IOMMU 0

#if DBG_IOMMU
#define ACRN_DBG_IOMMU LOG_INFO
#define DMAR_FAULT_LOOP_MAX 10
#else
#define ACRN_DBG_IOMMU 6
#endif

/* set an appropriate bus limitation when iommu init,
 * to reduce memory & time cost
 */
#define IOMMU_INIT_BUS_LIMIT        (0xf)

#define PAGE_MASK                   (0xFFFUL)
#define LEVEL_WIDTH 9

#define ROOT_ENTRY_LOWER_PRESENT_POS        (0)
#define ROOT_ENTRY_LOWER_PRESENT_MASK       ((uint64_t)1)
#define ROOT_ENTRY_LOWER_CTP_POS            (12)
#define ROOT_ENTRY_LOWER_CTP_MASK           ((uint64_t)0xFFFFFFFFFFFFF)

#define CTX_ENTRY_UPPER_AW_POS          (0)
#define CTX_ENTRY_UPPER_AW_MASK         \
		((uint64_t)0x7 << CTX_ENTRY_UPPER_AW_POS)
#define CTX_ENTRY_UPPER_DID_POS         (8)
#define CTX_ENTRY_UPPER_DID_MASK        \
		((uint64_t)0x3F << CTX_ENTRY_UPPER_DID_POS)
#define CTX_ENTRY_LOWER_P_POS           (0)
#define CTX_ENTRY_LOWER_P_MASK          \
		((uint64_t)0x1 << CTX_ENTRY_LOWER_P_POS)
#define CTX_ENTRY_LOWER_FPD_POS         (1)
#define CTX_ENTRY_LOWER_FPD_MASK        \
		((uint64_t)0x1 << CTX_ENTRY_LOWER_FPD_POS)
#define CTX_ENTRY_LOWER_TT_POS          (2)
#define CTX_ENTRY_LOWER_TT_MASK         \
		((uint64_t)0x3 << CTX_ENTRY_LOWER_TT_POS)
#define CTX_ENTRY_LOWER_SLPTPTR_POS     (12)
#define CTX_ENTRY_LOWER_SLPTPTR_MASK    \
		((uint64_t)0xFFFFFFFFFFFFF <<  CTX_ENTRY_LOWER_SLPTPTR_POS)

#define DMAR_GET_BITSLICE(var, bitname) \
	((var & bitname ## _MASK) >> bitname ## _POS)

#define DMAR_SET_BITSLICE(var, bitname, val) \
	((var &	\
	  ~bitname ## _MASK) | ((val << bitname ## _POS) & bitname ## _MASK))

/* translation type */
#define DMAR_CTX_TT_UNTRANSLATED    0x0
#define DMAR_CTX_TT_ALL             0x1
#define DMAR_CTX_TT_PASSTHROUGH     0x2

/* Fault event MSI data register */
#define DMAR_MSI_DELIVERY_MODE_SHIFT     (8)
#define DMAR_MSI_DELIVERY_FIXED          (0 << DMAR_MSI_DELIVERY_MODE_SHIFT)
#define DMAR_MSI_DELIVERY_LOWPRI         (1 << DMAR_MSI_DELIVERY_MODE_SHIFT)

/* Fault event MSI address register */
#define DMAR_MSI_DEST_MODE_SHIFT         (2)
#define DMAR_MSI_DEST_MODE_PHYS          (0 << DMAR_MSI_DEST_MODE_SHIFT)
#define DMAR_MSI_DEST_MODE_LOGIC         (1 << DMAR_MSI_DEST_MODE_SHIFT)
#define DMAR_MSI_REDIRECTION_SHIFT       (3)
#define DMAR_MSI_REDIRECTION_CPU         (0 << DMAR_MSI_REDIRECTION_SHIFT)
#define DMAR_MSI_REDIRECTION_LOWPRI      (1 << DMAR_MSI_REDIRECTION_SHIFT)

#define IOMMU_LOCK(u) spinlock_obtain(&((u)->lock))
#define IOMMU_UNLOCK(u) spinlock_release(&((u)->lock))

#define DMAR_OP_TIMEOUT CYCLES_PER_MS

#define DMAR_WAIT_COMPLETION(offset, condition, status) \
	do {                                                \
		uint64_t start = rdtsc();                       \
		while (1) {                                     \
			status = iommu_read32(dmar_uint, offset);   \
			if (condition)                              \
				break;                                  \
			ASSERT((rdtsc() - start < CYCLES_PER_MS),        \
				"DMAR OP Timeout!");   \
			asm volatile ("pause" ::: "memory");        \
		}                                               \
	} while (0)

enum dmar_cirg_type {
	DMAR_CIRG_RESERVED = 0,
	DMAR_CIRG_GLOBAL,
	DMAR_CIRG_DOMAIN,
	DMAR_CIRG_DEVICE
};

enum dmar_iirg_type {
	DMAR_IIRG_RESERVED = 0,
	DMAR_IIRG_GLOBAL,
	DMAR_IIRG_DOMAIN,
	DMAR_IIRG_PAGE
};

/* dmar unit runtime data */
struct dmar_drhd_rt {
	struct list_head list;
	spinlock_t lock;

	struct dmar_drhd *drhd;

	uint64_t root_table_addr;

	uint64_t cap;
	uint64_t ecap;
	uint64_t gcmd;  /* sw cache value of global cmd register */

	uint32_t irq;
	struct dev_handler_node *dmar_irq_node;

	uint32_t max_domain_id;

	bool cap_pw_coherency;  /* page-walk coherency */
	uint8_t cap_msagaw;
	uint16_t cap_num_fault_regs;
	uint16_t cap_fault_reg_offset;
	uint16_t ecap_iotlb_offset;
};

struct dmar_root_entry {
	uint64_t lower;
	uint64_t upper;
};

struct dmar_context_entry {
	uint64_t lower;
	uint64_t upper;
};

struct iommu_domain {
	struct list_head list;
	bool is_host;
	bool is_tt_ept;     /* if reuse EPT of the domain */
	uint16_t dom_id;
	int vm_id;
	uint32_t addr_width;   /* address width of the domain */
	void *trans_table_ptr;
};

static struct list_head dmar_drhd_units;
static uint32_t dmar_hdrh_unit_count;

/* Use to record the domain ids that are used,
 * support 64 domains (should be enough?)
 * domain id 0 is reserved,
 * bit0 --> domain id 0, ..., bit63 --> domain id 63
 */
static uint32_t max_domain_id = 63;
static uint64_t domain_bitmap;
static spinlock_t domain_lock;
static struct iommu_domain *host_domain;
static struct list_head iommu_domains;

static void dmar_register_hrhd(struct dmar_drhd_rt *drhd_rt);
static struct dmar_drhd_rt *device_to_dmaru(uint16_t segment, uint8_t bus,
					   uint8_t devfun);
static int register_hrhd_units(void)
{
	struct dmar_info *info = get_dmar_info();
	struct dmar_drhd_rt *drhd_rt;
	uint32_t i;

	if (!info) {
		pr_warn("vtd: no dmar units found");
		return -1;
	}

	for (i = 0; i < info->drhd_count; i++) {
		drhd_rt = malloc(sizeof(struct dmar_drhd_rt));
		ASSERT(drhd_rt != NULL, "");
		memset(drhd_rt, 0, sizeof(struct dmar_drhd_rt));
		drhd_rt->drhd = &info->drhd_units[i];
		dmar_register_hrhd(drhd_rt);
	}

	return 0;
}

static uint32_t iommu_read32(struct dmar_drhd_rt *dmar_uint, uint32_t offset)
{
	return mmio_read_long(dmar_uint->drhd->reg_base_addr + offset);
}

static uint64_t iommu_read64(struct dmar_drhd_rt *dmar_uint, uint32_t offset)
{
	uint64_t value;

	value = (mmio_read_long(dmar_uint->drhd->reg_base_addr + offset + 4));
	value = value << 32;
	value = value | (mmio_read_long(dmar_uint->drhd->reg_base_addr +
					offset));

	return value;
}

static void iommu_write32(struct dmar_drhd_rt *dmar_uint, uint32_t offset,
			  uint32_t value)
{
	mmio_write_long(value, dmar_uint->drhd->reg_base_addr + offset);
}

static void iommu_write64(struct dmar_drhd_rt *dmar_uint, uint32_t offset,
			  uint64_t value)
{
	uint32_t temp;

	temp = value;
	mmio_write_long(temp, dmar_uint->drhd->reg_base_addr + offset);

	temp = value >> 32;
	mmio_write_long(temp, dmar_uint->drhd->reg_base_addr + offset + 4);
}

#if DBG_IOMMU
static void dmar_uint_show_capability(struct dmar_drhd_rt *dmar_uint)
{
	pr_info("dmar unit[0x%x]", dmar_uint->drhd->reg_base_addr);
	pr_info("\tNumDomain:%d",
		iommu_cap_ndoms(dmar_uint->cap));
	pr_info("\tAdvancedFaultLogging:%d",
		iommu_cap_afl(dmar_uint->cap));
	pr_info("\tRequiredWBFlush:%d",
		iommu_cap_rwbf(dmar_uint->cap));
	pr_info("\tProtectedLowMemRegion:%d",
		iommu_cap_plmr(dmar_uint->cap));
	pr_info("\tProtectedHighMemRegion:%d",
		iommu_cap_phmr(dmar_uint->cap));
	pr_info("\tCachingMode:%d",
		iommu_cap_caching_mode(dmar_uint->cap));
	pr_info("\tSAGAW:0x%x",
		iommu_cap_sagaw(dmar_uint->cap));
	pr_info("\tMGAW:%d",
		iommu_cap_mgaw(dmar_uint->cap));
	pr_info("\tZeroLenRead:%d",
		iommu_cap_zlr(dmar_uint->cap));
	pr_info("\tLargePageSupport:0x%x",
		iommu_cap_super_page_val(dmar_uint->cap));
	pr_info("\tPageSelectiveInvalidation:%d",
		iommu_cap_pgsel_inv(dmar_uint->cap));
	pr_info("\tPageSelectInvalidation:%d",
		iommu_cap_pgsel_inv(dmar_uint->cap));
	pr_info("\tNumOfFaultRecordingReg:%d",
		iommu_cap_num_fault_regs(dmar_uint->cap));
	pr_info("\tMAMV:0x%x",
		iommu_cap_max_amask_val(dmar_uint->cap));
	pr_info("\tWriteDraining:%d",
		iommu_cap_write_drain(dmar_uint->cap));
	pr_info("\tReadDraining:%d",
		iommu_cap_read_drain(dmar_uint->cap));
	pr_info("\tPostInterrupts:%d\n",
		iommu_cap_pi(dmar_uint->cap));
	pr_info("\tPage-walk Coherency:%d",
		iommu_ecap_c(dmar_uint->ecap));
	pr_info("\tQueuedInvalidation:%d",
		iommu_ecap_qi(dmar_uint->ecap));
	pr_info("\tDeviceTLB:%d",
		iommu_ecap_dt(dmar_uint->ecap));
	pr_info("\tInterruptRemapping:%d",
		iommu_ecap_ir(dmar_uint->ecap));
	pr_info("\tExtendedInterruptMode:%d",
		iommu_ecap_eim(dmar_uint->ecap));
	pr_info("\tPassThrough:%d",
		iommu_ecap_pt(dmar_uint->ecap));
	pr_info("\tSnoopControl:%d",
		iommu_ecap_sc(dmar_uint->ecap));
	pr_info("\tIOTLB RegOffset:0x%x",
		iommu_ecap_iro(dmar_uint->ecap));
	pr_info("\tMHMV:0x%x", iommu_ecap_mhmv(dmar_uint->ecap));
	pr_info("\tECS:%d", iommu_ecap_ecs(dmar_uint->ecap));
	pr_info("\tMTS:%d", iommu_ecap_mts(dmar_uint->ecap));
	pr_info("\tNEST:%d", iommu_ecap_nest(dmar_uint->ecap));
	pr_info("\tDIS:%d", iommu_ecap_dis(dmar_uint->ecap));
	pr_info("\tPRS:%d", iommu_ecap_prs(dmar_uint->ecap));
	pr_info("\tERS:%d", iommu_ecap_ers(dmar_uint->ecap));
	pr_info("\tSRS:%d", iommu_ecap_srs(dmar_uint->ecap));
	pr_info("\tNWFS:%d", iommu_ecap_nwfs(dmar_uint->ecap));
	pr_info("\tEAFS:%d", iommu_ecap_eafs(dmar_uint->ecap));
	pr_info("\tPSS:0x%x", iommu_ecap_pss(dmar_uint->ecap));
	pr_info("\tPASID:%d", iommu_ecap_pasid(dmar_uint->ecap));
	pr_info("\tDIT:%d", iommu_ecap_dit(dmar_uint->ecap));
	pr_info("\tPDS:%d\n", iommu_ecap_pds(dmar_uint->ecap));
}
#endif

static inline uint8_t width_to_level(int width)
{
	return ((width - 12) + (LEVEL_WIDTH)-1) / (LEVEL_WIDTH);
}

static inline uint8_t width_to_agaw(int width)
{
	return width_to_level(width) - 2;
}

static uint8_t dmar_uint_get_msagw(struct dmar_drhd_rt *dmar_uint)
{
	int i;
	uint8_t sgaw = iommu_cap_sagaw(dmar_uint->cap);

	for (i = 4; i >= 0; i--) {
		if ((1 << i) & sgaw)
			break;
	}
	return (uint8_t)i;
}

static bool
dmar_unit_support_aw(struct dmar_drhd_rt *dmar_uint, uint32_t addr_width)
{
	uint8_t aw;

	aw = (uint8_t)width_to_agaw(addr_width);

	return ((1 << aw) & iommu_cap_sagaw(dmar_uint->cap)) != 0;
}

static void dmar_enable_translation(struct dmar_drhd_rt *dmar_uint)
{
	uint32_t status;

	IOMMU_LOCK(dmar_uint);
	dmar_uint->gcmd |= DMA_GCMD_TE;
	iommu_write32(dmar_uint, DMAR_GCMD_REG, dmar_uint->gcmd);

	/* 32-bit register */
	DMAR_WAIT_COMPLETION(DMAR_GSTS_REG, status & DMA_GSTS_TES, status);


	status = iommu_read32(dmar_uint, DMAR_GSTS_REG);

	IOMMU_UNLOCK(dmar_uint);

	dev_dbg(ACRN_DBG_IOMMU, "%s: gsr:0x%x", __func__, status);
}

static void dmar_disable_translation(struct dmar_drhd_rt *dmar_uint)
{
	uint32_t status;

	IOMMU_LOCK(dmar_uint);
	dmar_uint->gcmd &= ~DMA_GCMD_TE;
	iommu_write32(dmar_uint, DMAR_GCMD_REG, dmar_uint->gcmd);

	/* 32-bit register */
	DMAR_WAIT_COMPLETION(DMAR_GSTS_REG, !(status & DMA_GSTS_TES), status);
	IOMMU_UNLOCK(dmar_uint);
}

static void dmar_register_hrhd(struct dmar_drhd_rt *dmar_uint)
{
	dev_dbg(ACRN_DBG_IOMMU, "Register dmar uint [%d] @0x%llx",
			dmar_hdrh_unit_count,
			dmar_uint->drhd->reg_base_addr);

	spinlock_init(&dmar_uint->lock);

	dmar_uint->cap = iommu_read64(dmar_uint, DMAR_CAP_REG);
	dmar_uint->ecap = iommu_read64(dmar_uint, DMAR_ECAP_REG);
	dmar_uint->gcmd = iommu_read64(dmar_uint, DMAR_GCMD_REG);

	dmar_uint->cap_msagaw = dmar_uint_get_msagw(dmar_uint);
	dmar_uint->cap_num_fault_regs =
		iommu_cap_num_fault_regs(dmar_uint->cap);
	dmar_uint->cap_fault_reg_offset =
		iommu_cap_fault_reg_offset(dmar_uint->cap);
	dmar_uint->ecap_iotlb_offset = iommu_ecap_iro(dmar_uint->ecap) * 16;

#if DBG_IOMMU
	pr_info("version:0x%x, cap:0x%llx, ecap:0x%llx",
		iommu_read32(dmar_uint, DMAR_VER_REG),
		dmar_uint->cap,
		dmar_uint->ecap);
	pr_info("sagaw:0x%x, msagaw:0x%x, iotlb offset 0x%x",
		iommu_cap_sagaw(dmar_uint->cap),
		dmar_uint->cap_msagaw,
		dmar_uint->ecap_iotlb_offset);

	dmar_uint_show_capability(dmar_uint);
#endif

	/* check capability */
	if ((iommu_cap_super_page_val(dmar_uint->cap) & 0x1) == 0)
		dev_dbg(ACRN_DBG_IOMMU, "dmar uint doesn't support 2MB page!");

	if ((iommu_cap_super_page_val(dmar_uint->cap) & 0x2) == 0)
		dev_dbg(ACRN_DBG_IOMMU, "dmar uint doesn't support 1GB page!");

	/* when the hardware support snoop control,
	 * to make sure snoop control is always enabled,
	 * the SNP filed in the leaf PTE should be set.
	 * How to guarantee it when EPT is used as second-level
	 * translation paging structures?
	 */
	if (!iommu_ecap_sc(dmar_uint->ecap))
		dev_dbg(ACRN_DBG_IOMMU,
			"dmar uint doesn't support snoop control!");

	dmar_uint->max_domain_id = iommu_cap_ndoms(dmar_uint->cap) - 1;

	if (dmar_uint->max_domain_id > 63)
		dmar_uint->max_domain_id = 63;

	if (max_domain_id > dmar_uint->max_domain_id)
		max_domain_id = dmar_uint->max_domain_id;

	/* register operation is considered serial, no lock here */
	if (dmar_uint->drhd->flags & DRHD_FLAG_INCLUDE_PCI_ALL_MASK)
		list_add_tail(&dmar_uint->list, &dmar_drhd_units);
	else
		list_add(&dmar_uint->list, &dmar_drhd_units);

	dmar_hdrh_unit_count++;

	if (dmar_uint->gcmd & DMA_GCMD_TE)
		dmar_disable_translation(dmar_uint);
}

static struct dmar_drhd_rt *device_to_dmaru(uint16_t segment, uint8_t bus,
					   uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_uint;
	struct list_head *pos;
	uint32_t i;

	list_for_each(pos, &dmar_drhd_units) {
		dmar_uint = list_entry(pos, struct dmar_drhd_rt, list);

		if (dmar_uint->drhd->segment != segment)
			continue;

		for (i = 0; i < dmar_uint->drhd->dev_cnt; i++) {
			if ((dmar_uint->drhd->devices[i].bus == bus) &&
				(dmar_uint->drhd->devices[i].devfun == devfun))
				return dmar_uint;
		}

		/* has the same segment number and
		 * the dmar unit has INCLUDE_PCI_ALL set
		 */
		if (dmar_uint->drhd->flags & DRHD_FLAG_INCLUDE_PCI_ALL_MASK)
			return dmar_uint;
	}

	return NULL;
}

static int alloc_domain_id(void)
{
	int i;
	uint64_t mask;

	spinlock_obtain(&domain_lock);
	/* domain id 0 is reserved, when CM = 1.
	 * so domain id allocation start from 1
	 */
	for (i = 1; i < 64; i++) {
		mask = (1 << i);
		if ((domain_bitmap & mask) == 0) {
			domain_bitmap |= mask;
			break;
		}
	}
	spinlock_release(&domain_lock);
	return i;
}

static void free_domain_id(int dom_id)
{
	uint64_t mask = (1 << dom_id);

	spinlock_obtain(&domain_lock);
	domain_bitmap &= ~mask;
	spinlock_release(&domain_lock);
}

static struct iommu_domain *create_host_domain(void)
{
	struct iommu_domain *domain = calloc(1, sizeof(struct iommu_domain));

	ASSERT(domain != NULL, "");
	domain->is_host = true;
	domain->dom_id = alloc_domain_id();
	/* dmar uint need to support translation passthrough */
	domain->trans_table_ptr = NULL;
	domain->addr_width = 48;

	return domain;
}

static void dmar_write_buffer_flush(struct dmar_drhd_rt *dmar_uint)
{
	uint32_t status;

	if (!iommu_cap_rwbf(dmar_uint->cap))
		return;

	IOMMU_LOCK(dmar_uint);
	iommu_write64(dmar_uint, DMAR_GCMD_REG,
			  dmar_uint->gcmd | DMA_GCMD_WBF);

	/* read lower 32 bits to check */
	DMAR_WAIT_COMPLETION(DMAR_GSTS_REG, !(status & DMA_GSTS_WBFS), status);
	IOMMU_UNLOCK(dmar_uint);
}

/*
 * did: domain id
 * sid: source id
 * fm: function mask
 * cirg: cache-invalidation request granularity
 */
static void dmar_invalid_context_cache(struct dmar_drhd_rt *dmar_uint,
	uint16_t did, uint16_t sid, uint8_t fm, enum dmar_cirg_type cirg)
{
	uint64_t cmd = DMA_CCMD_ICC;
	uint32_t status;

	switch (cirg) {
	case DMAR_CIRG_GLOBAL:
		cmd |= DMA_CCMD_GLOBAL_INVL;
		break;
	case DMAR_CIRG_DOMAIN:
		cmd |= DMA_CCMD_DOMAIN_INVL | DMA_CCMD_DID(did);
		break;
	case DMAR_CIRG_DEVICE:
		cmd |= DMA_CCMD_DEVICE_INVL | DMA_CCMD_DID(did) |
			   DMA_CCMD_SID(sid) | DMA_CCMD_FM(fm);
		break;
	default:
		pr_err("unknown CIRG type");
		return;
	}

	IOMMU_LOCK(dmar_uint);
	iommu_write64(dmar_uint, DMAR_CCMD_REG, cmd);
	/* read upper 32bits to check */
	DMAR_WAIT_COMPLETION(DMAR_CCMD_REG + 4, !(status & DMA_CCMD_ICC_32),
				 status);

	IOMMU_UNLOCK(dmar_uint);

	dev_dbg(ACRN_DBG_IOMMU, "cc invalidation granularity %d",
		DMA_CCMD_GET_CAIG_32(status));
}

static void dmar_invalid_context_cache_global(struct dmar_drhd_rt *dmar_uint)
{
	dmar_invalid_context_cache(dmar_uint, 0, 0, 0, DMAR_CIRG_GLOBAL);
}

static void dmar_invalid_iotlb(struct dmar_drhd_rt *dmar_uint,
				   uint16_t did, uint64_t address, uint8_t am,
				   bool hint, enum dmar_iirg_type iirg)
{
	/* set Drain Reads & Drain Writes,
	 * if hardware doesn't support it, will be ignored by hardware
	 */
	uint64_t cmd = DMA_IOTLB_IVT | DMA_IOTLB_DR | DMA_IOTLB_DW;
	uint64_t addr = 0;
	uint32_t status;

	switch (iirg) {
	case DMAR_IIRG_GLOBAL:
		cmd |= DMA_IOTLB_GLOBAL_INVL;
		break;
	case DMAR_IIRG_DOMAIN:
		cmd |= DMA_IOTLB_DOMAIN_INVL | DMA_IOTLB_DID(did);
		break;
	case DMAR_IIRG_PAGE:
		cmd |= DMA_IOTLB_PAGE_INVL | DMA_IOTLB_DID(did);
		addr = address | DMA_IOTLB_INVL_ADDR_AM(am);
		if (hint)
			addr |= DMA_IOTLB_INVL_ADDR_IH_UNMODIFIED;
		break;
	default:
		pr_err("unknown IIRG type");
		return;
	}
	IOMMU_LOCK(dmar_uint);
	if (addr)
		iommu_write64(dmar_uint, dmar_uint->ecap_iotlb_offset, addr);

	iommu_write64(dmar_uint, dmar_uint->ecap_iotlb_offset + 8, cmd);
	/* read upper 32bits to check */
	DMAR_WAIT_COMPLETION(dmar_uint->ecap_iotlb_offset + 12,
				 !(status & DMA_IOTLB_IVT_32), status);
	IOMMU_UNLOCK(dmar_uint);

	if (!DMA_IOTLB_GET_IAIG_32(status)) {
		pr_err("fail to invalidate IOTLB!, 0x%x, 0x%x",
			status, iommu_read32(dmar_uint, DMAR_FSTS_REG));
	}
}

/* Invalidate IOTLB globally,
 * all iotlb entries are invalidated,
 * all PASID-cache entries are invalidated,
 * all paging-structure-cache entries are invalidated.
 */
static void dmar_invalid_iotlb_global(struct dmar_drhd_rt *dmar_uint)
{
	dmar_invalid_iotlb(dmar_uint, 0, 0, 0, 0, DMAR_IIRG_GLOBAL);
}

static void dmar_set_root_table(struct dmar_drhd_rt *dmar_uint)
{
	uint64_t address;
	uint32_t status;

	IOMMU_LOCK(dmar_uint);

	/* Currently don't support extended root table */
	address = dmar_uint->root_table_addr;

	iommu_write64(dmar_uint, DMAR_RTADDR_REG, address);

	iommu_write32(dmar_uint, DMAR_GCMD_REG,
			dmar_uint->gcmd | DMA_GCMD_SRTP);

	/* 32-bit register */
	DMAR_WAIT_COMPLETION(DMAR_GSTS_REG, status & DMA_GSTS_RTPS, status);
	IOMMU_UNLOCK(dmar_uint);
}

static int dmar_fault_event_mask(struct dmar_drhd_rt *dmar_uint)
{
	IOMMU_LOCK(dmar_uint);
	iommu_write32(dmar_uint, DMAR_FECTL_REG, DMA_FECTL_IM);
	IOMMU_UNLOCK(dmar_uint);
	return 0;
}

static int dmar_fault_event_unmask(struct dmar_drhd_rt *dmar_uint)
{
	IOMMU_LOCK(dmar_uint);
	iommu_write32(dmar_uint, DMAR_FECTL_REG, 0);
	IOMMU_UNLOCK(dmar_uint);
	return 0;
}

static void dmar_fault_msi_write(struct dmar_drhd_rt *dmar_uint,
			uint8_t vector)
{
	uint32_t data;
	uint32_t addr_low;
	uint32_t lapic_id = get_cur_lapic_id();

	data = DMAR_MSI_DELIVERY_LOWPRI | vector;
	/* redirection hint: 0
	 * destination mode: 0
	 */
	addr_low = 0xFEE00000 | ((lapic_id & 0xFF) << 12);

	IOMMU_LOCK(dmar_uint);
	iommu_write32(dmar_uint, DMAR_FEDATA_REG, data);
	iommu_write32(dmar_uint, DMAR_FEADDR_REG, addr_low);
	IOMMU_UNLOCK(dmar_uint);
}

#if DBG_IOMMU
static void fault_status_analysis(uint32_t status)
{
	if (DMA_FSTS_PFO(status))
		pr_info("Primary Fault Overflow");

	if (DMA_FSTS_PPF(status))
		pr_info("Primary Pending Fault");

	if (DMA_FSTS_AFO(status))
		pr_info("Advanced Fault Overflow");

	if (DMA_FSTS_APF(status))
		pr_info("Advanced Pending Fault");

	if (DMA_FSTS_IQE(status))
		pr_info("Invalidation Queue Error");

	if (DMA_FSTS_ICE(status))
		pr_info("Invalidation Completion Error");

	if (DMA_FSTS_ITE(status))
		pr_info("Invalidation Time-out Error");

	if (DMA_FSTS_PRO(status))
		pr_info("Page Request Overflow");
}
#endif

static void fault_record_analysis(__unused uint64_t low, uint64_t high)
{
	if (!DMA_FRCD_UP_F(high))
		return;

	/* currently skip PASID related parsing */
	pr_info("%s, Reason: 0x%x, SID: %x.%x.%x @0x%llx",
		DMA_FRCD_UP_T(high) ? "Read/Atomic" : "Write",
		DMA_FRCD_UP_FR(high),
		DMA_FRCD_UP_SID(high) >> 8,
		(DMA_FRCD_UP_SID(high) >> 3) & 0x1f,
		DMA_FRCD_UP_SID(high) & 0x7,
		low);
#if DBG_IOMMU
	if (iommu_ecap_dt(dmar_uint->ecap))
		pr_info("Address Type: 0x%x",
				DMA_FRCD_UP_AT(high));
#endif
}

static int dmar_fault_handler(__unused int irq, void *data)
{
	struct dmar_drhd_rt *dmar_uint = (struct dmar_drhd_rt *)data;
	uint32_t fsr;
	uint32_t index;
	uint32_t record_reg_offset;
	uint64_t record[2];
	int loop = 0;

	dev_dbg(ACRN_DBG_IOMMU, "%s: irq = %d", __func__, irq);

	fsr = iommu_read32(dmar_uint, DMAR_FSTS_REG);

#if DBG_IOMMU
	fault_status_analysis(fsr);
#endif

	while (DMA_FSTS_PPF(fsr)) {
		loop++;
		index = DMA_FSTS_FRI(fsr);
		record_reg_offset = dmar_uint->cap_fault_reg_offset
				+ index * 16;
		if (index >= dmar_uint->cap_num_fault_regs) {
			dev_dbg(ACRN_DBG_IOMMU, "%s: invalid FR Index",
					__func__);
			break;
		}

		/* read 128-bit fault recording register */
		record[0] = iommu_read64(dmar_uint, record_reg_offset);
		record[1] = iommu_read64(dmar_uint, record_reg_offset + 8);

		dev_dbg(ACRN_DBG_IOMMU, "%s: record[%d] @0x%x:  0x%llx, 0x%llx",
			__func__, index, record_reg_offset,
			record[0], record[1]);

		fault_record_analysis(record[0], record[1]);

		/* write to clear */
		iommu_write64(dmar_uint, record_reg_offset, record[0]);
		iommu_write64(dmar_uint, record_reg_offset + 8, record[1]);

#ifdef DMAR_FAULT_LOOP_MAX
		if (loop > DMAR_FAULT_LOOP_MAX) {
			dev_dbg(ACRN_DBG_IOMMU, "%s: loop more than %d times",
				__func__, DMAR_FAULT_LOOP_MAX);
			break;
		}
#endif

		fsr = iommu_read32(dmar_uint, DMAR_FSTS_REG);
	}

	return 0;
}

static int dmar_setup_interrupt(struct dmar_drhd_rt *dmar_uint)
{
	int vector;

	if (dmar_uint->dmar_irq_node) {
		dev_dbg(ACRN_DBG_IOMMU, "%s: irq already setup", __func__);
		return 0;
	}

	dmar_uint->dmar_irq_node = normal_register_handler(-1,
					dmar_fault_handler,
					dmar_uint, true, false,
					"dmar_fault_event");

	if (!dmar_uint->dmar_irq_node) {
		pr_err("%s: fail to setup interrupt", __func__);
		return 1;
	}

	vector = dev_to_vector(dmar_uint->dmar_irq_node);

	dev_dbg(ACRN_DBG_IOMMU, "alloc irq#%d vector#%d for dmar_uint",
			dev_to_irq(dmar_uint->dmar_irq_node), vector);

	dmar_fault_msi_write(dmar_uint, vector);
	dmar_fault_event_unmask(dmar_uint);

	return 0;
}

static void dmar_enable(struct dmar_drhd_rt *dmar_uint)
{
	dev_dbg(ACRN_DBG_IOMMU, "enable dmar uint [0x%x]",
		dmar_uint->drhd->reg_base_addr);
	dmar_setup_interrupt(dmar_uint);
	dmar_write_buffer_flush(dmar_uint);
	dmar_set_root_table(dmar_uint);
	dmar_invalid_context_cache_global(dmar_uint);
	dmar_invalid_iotlb_global(dmar_uint);
	dmar_enable_translation(dmar_uint);
}

static void dmar_disable(struct dmar_drhd_rt *dmar_uint)
{
	if (dmar_uint->gcmd & DMA_GCMD_TE)
		dmar_disable_translation(dmar_uint);

	dmar_fault_event_mask(dmar_uint);
}

struct iommu_domain *create_iommu_domain(int vm_id, void *translation_table,
		int addr_width)
{
	struct iommu_domain *domain;
	uint16_t domain_id;

	/* TODO: check if a domain with the vm_id exists */

	if (!translation_table) {
		pr_err("translation table is NULL");
		return NULL;
	}

	domain_id = alloc_domain_id();
	if (domain_id > max_domain_id) {
		pr_err("domain id is exhausted");
		return NULL;
	}

	domain = calloc(1, sizeof(struct iommu_domain));

	ASSERT(domain != NULL, "");
	domain->is_host = false;
	domain->dom_id = domain_id;
	domain->vm_id = vm_id;
	domain->trans_table_ptr = translation_table;
	domain->addr_width = addr_width;
	domain->is_tt_ept = true;


	spinlock_obtain(&domain_lock);
	list_add(&domain->list, &iommu_domains);
	spinlock_release(&domain_lock);

	dev_dbg(ACRN_DBG_IOMMU, "create domain [%d]: vm_id = %d, ept@0x%x",
		domain->dom_id,
		domain->vm_id,
		domain->trans_table_ptr);

	return domain;
}

int destroy_iommu_domain(struct iommu_domain *domain)
{
	if (!domain)
		return 1;

	/* currently only support ept */
	if (!domain->is_tt_ept)
		ASSERT(false, "translation_table is not EPT!");

	/* TODO: check if any device assigned to this domain */

	spinlock_obtain(&domain_lock);
	list_del(&domain->list);
	spinlock_release(&domain_lock);

	free_domain_id(domain->dom_id);
	free(domain);

	return 0;
}

static int add_iommu_device(struct iommu_domain *domain, uint16_t segment,
		uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_uint;
	uint64_t *root_table;
	uint64_t context_table_addr;
	uint64_t *context_table;
	struct dmar_root_entry *root_entry;
	struct dmar_context_entry *context_entry;
	uint64_t upper = 0;
	uint64_t lower = 0;

	if (!domain)
		return 1;

	dmar_uint = device_to_dmaru(segment, bus, devfun);
	if (!dmar_uint) {
		pr_err("no dmar unit found for device:0x%x:%x.%x",
			bus, devfun >> 3, devfun & 0x7);
		return 1;
	}

	if (dmar_uint->drhd->ignore) {
		dev_dbg(ACRN_DBG_IOMMU, "device is ignored :0x%x:%x.%x",
			bus, devfun >> 3, devfun & 0x7);
		return 0;
	}

	if (!dmar_unit_support_aw(dmar_uint, domain->addr_width)) {
		pr_err("dmar doesn't support addr width %d",
			domain->addr_width);
		return 1;
	}

	if (dmar_uint->root_table_addr == 0) {
		/* 1:1 mapping for hypervisor HEAP,
		 * physical address equals virtual address
		 */
		dmar_uint->root_table_addr =
			(uint64_t)alloc_paging_struct();
	}

	root_table = (uint64_t *)dmar_uint->root_table_addr;

	root_entry = (struct dmar_root_entry *)&root_table[bus * 2];

	if (!DMAR_GET_BITSLICE(root_entry->lower, ROOT_ENTRY_LOWER_PRESENT)) {
		/* create context table for the bus if not present */
		context_table_addr =
			(uint64_t)alloc_paging_struct();

		context_table_addr = context_table_addr >> 12;

		lower = DMAR_SET_BITSLICE(lower, ROOT_ENTRY_LOWER_CTP,
					  context_table_addr);
		lower = DMAR_SET_BITSLICE(lower, ROOT_ENTRY_LOWER_PRESENT, 1);

		root_entry->upper = 0;
		root_entry->lower = lower;
	} else {
		context_table_addr = DMAR_GET_BITSLICE(root_entry->lower,
				ROOT_ENTRY_LOWER_CTP);
	}

	context_table_addr = context_table_addr << 12;

	context_table = (uint64_t *)context_table_addr;
	context_entry = (struct dmar_context_entry *)&context_table[devfun * 2];

	/* the context entry should not be present */
	if (DMAR_GET_BITSLICE(context_entry->lower, CTX_ENTRY_LOWER_P)) {
		pr_err("%s: context entry@0x%llx (Lower:%x) ",
				__func__, context_entry, context_entry->lower);
		pr_err("already present for %x:%x.%x",
				bus, devfun >> 3, devfun & 0x7);
		return 1;
	}

	/* setup context entry for the devfun */
	upper = 0;
	lower = 0;
	if (domain->is_host) {
		if (iommu_ecap_pt(dmar_uint->ecap)) {
			/* When the Translation-type (T) field indicates
			 * pass-through processing (10b), AW field must be
			 * programmed to indicate the largest AGAW value
			 * supported by hardware.
			 */
			upper = DMAR_SET_BITSLICE(upper, CTX_ENTRY_UPPER_AW,
						  dmar_uint->cap_msagaw);
			lower = DMAR_SET_BITSLICE(lower, CTX_ENTRY_LOWER_TT,
						  DMAR_CTX_TT_PASSTHROUGH);
		} else
			ASSERT(false,
				  "dmaru doesn't support trans passthrough");
	} else {
		/* TODO: add Device TLB support */
		upper =
			DMAR_SET_BITSLICE(upper, CTX_ENTRY_UPPER_AW,
					  width_to_agaw(
						  domain->addr_width));
		lower = DMAR_SET_BITSLICE(lower, CTX_ENTRY_LOWER_TT,
					  DMAR_CTX_TT_UNTRANSLATED);
	}

	upper = DMAR_SET_BITSLICE(upper, CTX_ENTRY_UPPER_DID, domain->dom_id);
	lower = DMAR_SET_BITSLICE(lower, CTX_ENTRY_LOWER_SLPTPTR,
				  (uint64_t)domain->trans_table_ptr >> 12);
	lower = DMAR_SET_BITSLICE(lower, CTX_ENTRY_LOWER_P, 1);

	context_entry->upper = upper;
	context_entry->lower = lower;

	return 0;
}

static int
remove_iommu_device(struct iommu_domain *domain, uint16_t segment,
		uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_uint;
	uint64_t *root_table;
	uint64_t context_table_addr;
	uint64_t *context_table;
	struct dmar_root_entry *root_entry;
	struct dmar_context_entry *context_entry;

	if (!domain)
		return 1;

	dmar_uint = device_to_dmaru(segment, bus, devfun);
	if (!dmar_uint) {
		pr_err("no dmar unit found for device:0x%x:%x",
				bus, devfun);
		return 1;
	}

	root_table = (uint64_t *)dmar_uint->root_table_addr;
	root_entry = (struct dmar_root_entry *)&root_table[bus * 2];

	context_table_addr = DMAR_GET_BITSLICE(root_entry->lower,
						   ROOT_ENTRY_LOWER_CTP);
	context_table_addr = context_table_addr << 12;
	context_table = (uint64_t *)context_table_addr;

	context_entry = (struct dmar_context_entry *)&context_table[devfun * 2];

	if (DMAR_GET_BITSLICE(context_entry->upper,
				  CTX_ENTRY_UPPER_DID) != domain->dom_id) {
		pr_err("%s: domain id mismatch", __func__);
		return 1;
	}

	/* clear the present bit first */
	context_entry->lower = 0;
	context_entry->upper = 0;

	/* if caching mode is present, need to invalidate translation cache */
	/* if(cap_caching_mode(dmar_uint->cap)) { */
	dmar_invalid_context_cache_global(dmar_uint);
	dmar_invalid_iotlb_global(dmar_uint);
	/* } */
	return 0;
}

int assign_iommu_device(struct iommu_domain *domain, uint8_t bus,
				uint8_t devfun)
{
	if (!domain)
		return 1;

	/* TODO: check if the device assigned */

	remove_iommu_device(host_domain, 0, bus, devfun);
	add_iommu_device(domain, 0, bus, devfun);
	return 0;
}

int unassign_iommu_device(struct iommu_domain *domain, uint8_t bus,
				uint8_t devfun)
{
	if (!domain)
		return 1;

	/* TODO: check if the device assigned */

	remove_iommu_device(domain, 0, bus, devfun);
	add_iommu_device(host_domain, 0, bus, devfun);
	return 0;
}

void enable_iommu(void)
{
	struct dmar_drhd_rt *dmar_uint;
	struct list_head *pos;

	list_for_each(pos, &dmar_drhd_units) {
		dmar_uint = list_entry(pos, struct dmar_drhd_rt, list);
		if (!dmar_uint->drhd->ignore)
			dmar_enable(dmar_uint);
		else
			dev_dbg(ACRN_DBG_IOMMU, "ignore dmar_uint @0x%x",
				dmar_uint->drhd->reg_base_addr);
	}
}

void disable_iommu(void)
{
	struct dmar_drhd_rt *dmar_uint;
	struct list_head *pos;

	list_for_each(pos, &dmar_drhd_units) {
		dmar_uint = list_entry(pos, struct dmar_drhd_rt, list);
		dmar_disable(dmar_uint);
	}
}

int init_iommu(void)
{
	uint16_t bus;
	uint16_t devfun;

	INIT_LIST_HEAD(&dmar_drhd_units);
	INIT_LIST_HEAD(&iommu_domains);

	spinlock_init(&domain_lock);

	if (register_hrhd_units())
		return -1;

	host_domain = create_host_domain();

	for (bus = 0; bus <= IOMMU_INIT_BUS_LIMIT; bus++) {
		for (devfun = 0; devfun <= 255; devfun++) {
			add_iommu_device(host_domain, 0,
					 (uint8_t)bus, (uint8_t)devfun);
		}
	}

	enable_iommu();

	return 0;
}
