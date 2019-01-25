/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define pr_prefix		"iommu: "

#include <hypervisor.h>
#include <vtd.h>

#define DBG_IOMMU 0

#if DBG_IOMMU
#define ACRN_DBG_IOMMU LOG_INFO
#define DMAR_FAULT_LOOP_MAX 10
#else
#define ACRN_DBG_IOMMU 6U
#endif

#define LEVEL_WIDTH 9U

#define ROOT_ENTRY_LOWER_PRESENT_POS        (0U)
#define ROOT_ENTRY_LOWER_PRESENT_MASK       (1UL)
#define ROOT_ENTRY_LOWER_CTP_POS            (12U)
#define ROOT_ENTRY_LOWER_CTP_MASK           (0xFFFFFFFFFFFFFUL)

/* 4 iommu fault register state */
#define	IOMMU_FAULT_REGISTER_STATE_NUM	4U
#define	IOMMU_FAULT_REGISTER_SIZE	4U

#define CTX_ENTRY_UPPER_AW_POS          (0U)
#define CTX_ENTRY_UPPER_AW_MASK         (0x7UL << CTX_ENTRY_UPPER_AW_POS)
#define CTX_ENTRY_UPPER_DID_POS         (8U)
#define CTX_ENTRY_UPPER_DID_MASK        (0x3FUL << CTX_ENTRY_UPPER_DID_POS)
#define CTX_ENTRY_LOWER_P_POS           (0U)
#define CTX_ENTRY_LOWER_P_MASK          (0x1UL << CTX_ENTRY_LOWER_P_POS)
#define CTX_ENTRY_LOWER_FPD_POS         (1U)
#define CTX_ENTRY_LOWER_FPD_MASK        (0x1UL << CTX_ENTRY_LOWER_FPD_POS)
#define CTX_ENTRY_LOWER_TT_POS          (2U)
#define CTX_ENTRY_LOWER_TT_MASK         (0x3UL << CTX_ENTRY_LOWER_TT_POS)
#define CTX_ENTRY_LOWER_SLPTPTR_POS     (12U)
#define CTX_ENTRY_LOWER_SLPTPTR_MASK    (0xFFFFFFFFFFFFFUL <<  CTX_ENTRY_LOWER_SLPTPTR_POS)

static inline uint64_t dmar_get_bitslice(uint64_t var, uint64_t mask, uint32_t pos)
{
	return ((var & mask) >> pos);
}

static inline uint64_t dmar_set_bitslice(uint64_t var, uint64_t mask, uint32_t pos, uint64_t val)
{
	return ((var & ~mask) | ((val << pos) & mask));
}

/* translation type */
#define DMAR_CTX_TT_UNTRANSLATED    0x0UL
#define DMAR_CTX_TT_ALL             0x1UL
#define DMAR_CTX_TT_PASSTHROUGH     0x2UL

/* Fault event MSI data register */
#define DMAR_MSI_DELIVERY_MODE_SHIFT     (8U)
#define DMAR_MSI_DELIVERY_FIXED          (0U << DMAR_MSI_DELIVERY_MODE_SHIFT)
#define DMAR_MSI_DELIVERY_LOWPRI         (1U << DMAR_MSI_DELIVERY_MODE_SHIFT)

/* Fault event MSI address register */
#define DMAR_MSI_DEST_MODE_SHIFT         (2U)
#define DMAR_MSI_DEST_MODE_PHYS          (0U << DMAR_MSI_DEST_MODE_SHIFT)
#define DMAR_MSI_DEST_MODE_LOGIC         (1U << DMAR_MSI_DEST_MODE_SHIFT)
#define DMAR_MSI_REDIRECTION_SHIFT       (3U)
#define DMAR_MSI_REDIRECTION_CPU         (0U << DMAR_MSI_REDIRECTION_SHIFT)
#define DMAR_MSI_REDIRECTION_LOWPRI      (1U << DMAR_MSI_REDIRECTION_SHIFT)

#define DMAR_INVALIDATION_QUEUE_SIZE	4096U
#define DMAR_QI_INV_ENTRY_SIZE		16U
#define DMAR_NUM_IR_ENTRIES_PER_PAGE	256U

#define DMAR_INV_STATUS_WRITE_SHIFT	5U
#define DMAR_INV_CONTEXT_CACHE_DESC	0x01UL
#define DMAR_INV_IOTLB_DESC		0x02UL
#define DMAR_INV_IEC_DESC		0x04UL
#define DMAR_INV_WAIT_DESC		0x05UL
#define DMAR_INV_STATUS_WRITE		(1UL << DMAR_INV_STATUS_WRITE_SHIFT)
#define DMAR_INV_STATUS_INCOMPLETE	0UL
#define DMAR_INV_STATUS_COMPLETED	1UL
#define DMAR_INV_STATUS_DATA_SHIFT	32U
#define DMAR_INV_STATUS_DATA		(DMAR_INV_STATUS_COMPLETED << DMAR_INV_STATUS_DATA_SHIFT)
#define DMAR_INV_WAIT_DESC_LOWER	(DMAR_INV_STATUS_WRITE | DMAR_INV_WAIT_DESC | DMAR_INV_STATUS_DATA)

#define DMAR_IR_ENABLE_EIM_SHIFT	11UL
#define DMAR_IR_ENABLE_EIM		(1UL << DMAR_IR_ENABLE_EIM_SHIFT)

#define DMAR_IECI_INDEXED		1U
#define DMAR_IEC_GLOBAL_INVL		0U

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
	uint32_t index;
	spinlock_t lock;

	struct dmar_drhd *drhd;

	uint64_t root_table_addr;
	uint64_t ir_table_addr;
	uint64_t qi_queue;
	uint16_t qi_tail;

	uint64_t cap;
	uint64_t ecap;
	uint32_t gcmd;  /* sw cache value of global cmd register */

	uint32_t dmar_irq;

	bool cap_pw_coherency;  /* page-walk coherency */
	uint8_t cap_msagaw;
	uint16_t cap_num_fault_regs;
	uint16_t cap_fault_reg_offset;
	uint16_t ecap_iotlb_offset;
	uint32_t fault_state[IOMMU_FAULT_REGISTER_STATE_NUM]; /* 32bit registers */
};

struct dmar_root_entry {
	uint64_t lower;
	uint64_t upper;
};

struct dmar_context_entry {
	uint64_t lower;
	uint64_t upper;
};

struct dmar_qi_desc {
	uint64_t lower;
	uint64_t upper;
};

struct context_table {
	struct page buses[CONFIG_IOMMU_BUS_NUM];
};

struct intr_remap_table {
	struct page tables[CONFIG_MAX_IR_ENTRIES/DMAR_NUM_IR_ENTRIES_PER_PAGE];
};

static inline uint8_t* get_root_table(uint32_t dmar_index)
{
	static struct page root_tables[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return root_tables[dmar_index].contents;
}

static inline uint8_t* get_ctx_table(uint32_t dmar_index, uint8_t bus_no)
{
	static struct context_table ctx_tables[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return ctx_tables[dmar_index].buses[bus_no].contents;
}

/*
 * @pre dmar_index < CONFIG_MAX_IOMMU_NUM
 */
static inline uint8_t *get_qi_queue(uint32_t dmar_index)
{
	static struct page qi_queues[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return qi_queues[dmar_index].contents;
}

static inline uint8_t *get_ir_table(uint32_t dmar_index)
{
	static struct intr_remap_table ir_tables[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return ir_tables[dmar_index].tables[0].contents;
}

bool iommu_snoop_supported(const struct iommu_domain *iommu)
{
	bool ret;

	if ((iommu == NULL) || (iommu->iommu_snoop)) {
		ret =  true;
	} else {
		ret = false;
	}

	return ret;
}

static struct dmar_drhd_rt dmar_drhd_units[CONFIG_MAX_IOMMU_NUM];
static bool iommu_page_walk_coherent = true;
static struct iommu_domain *fallback_iommu_domain;
static uint32_t qi_status = 0U;

/* Domain id 0 is reserved in some cases per VT-d */
#define MAX_DOMAIN_NUM (CONFIG_MAX_VM_NUM + 1)

static inline uint16_t vmid_to_domainid(uint16_t vm_id)
{
	return vm_id + 1U;
}

static int32_t dmar_register_hrhd(struct dmar_drhd_rt *dmar_unit);
static struct dmar_drhd_rt *device_to_dmaru(uint16_t segment, uint8_t bus, uint8_t devfun);
static int32_t register_hrhd_units(void)
{
	struct dmar_info *info = get_dmar_info();
	struct dmar_drhd_rt *drhd_rt;
	uint32_t i;
	int32_t ret = 0;

	if ((info == NULL) || (info->drhd_count == 0U)) {
		pr_fatal("%s: can't find dmar info\n", __func__);
		ret = -ENODEV;
	} else if (info->drhd_count > CONFIG_MAX_IOMMU_NUM) {
		pr_fatal("%s: dmar count(%d) beyond the limitation(%d)\n",
				__func__, info->drhd_count, CONFIG_MAX_IOMMU_NUM);
		ret = -EINVAL;
	} else {
		for (i = 0U; i < info->drhd_count; i++) {
			drhd_rt = &dmar_drhd_units[i];
			drhd_rt->index = i;
			drhd_rt->drhd = &info->drhd_units[i];
			drhd_rt->dmar_irq = IRQ_INVALID;
			ret = dmar_register_hrhd(drhd_rt);
			if (ret != 0) {
				break;
			}
		}
	}

	return ret;
}

static uint32_t iommu_read32(const struct dmar_drhd_rt *dmar_unit, uint32_t offset)
{
	return mmio_read32(hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static uint64_t iommu_read64(const struct dmar_drhd_rt *dmar_unit, uint32_t offset)
{
	uint64_t value;

	value = mmio_read32(hpa2hva(dmar_unit->drhd->reg_base_addr + offset + 4U));
	value = value << 32U;
	value = value | mmio_read32(hpa2hva(dmar_unit->drhd->reg_base_addr + offset));

	return value;
}

static void iommu_write32(const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint32_t value)
{
	mmio_write32(value, hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static void iommu_write64(const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint64_t value)
{
	uint32_t temp;

	temp = (uint32_t)value;
	mmio_write32(temp, hpa2hva(dmar_unit->drhd->reg_base_addr + offset));

	temp = (uint32_t)(value >> 32U);
	mmio_write32(temp, hpa2hva(dmar_unit->drhd->reg_base_addr + offset + 4U));
}

static inline void dmar_wait_completion(const struct dmar_drhd_rt *dmar_unit, uint32_t offset,
	uint32_t mask, bool pre_condition, uint32_t *status)
{
	/* variable start isn't used when built as release version */
	__unused uint64_t start = rdtsc();
	bool condition, temp_condition;

	while (1) {
		*status = iommu_read32(dmar_unit, offset);
		temp_condition = ((*status & mask) == 0U) ? true : false;

		/*
		 * pre_condition    temp_condition    | condition
		 * -----------------------------------|----------
		 * true             true              | true
		 * true             false             | false
		 * false            true              | false
		 * false            false             | true
		 */
		condition = (temp_condition == pre_condition) ? true : false;

		if (condition) {
			break;
		}
		ASSERT(((rdtsc() - start) < CYCLES_PER_MS),
			"DMAR OP Timeout!");
		asm_pause();
	}
}

/* flush cache when root table, context table updated */
static void iommu_flush_cache(const struct dmar_drhd_rt *dmar_unit,
			      void *p, uint32_t size)
{
	uint32_t i;

	/* if vtd support page-walk coherency, no need to flush cacheline */
	if (iommu_ecap_c(dmar_unit->ecap) == 0U) {
		for (i = 0U; i < size; i += CACHE_LINE_SIZE) {
			clflush((char *)p + i);
		}
	}
}

#if DBG_IOMMU
static inline uint8_t iommu_cap_rwbf(uint64_t cap)
{
	return ((uint8_t)(cap >> 4U) & 1U);
}

static void dmar_unit_show_capability(struct dmar_drhd_rt *dmar_unit)
{
	pr_info("dmar unit[0x%x]", dmar_unit->drhd->reg_base_addr);
	pr_info("\tNumDomain:%d", iommu_cap_ndoms(dmar_unit->cap));
	pr_info("\tAdvancedFaultLogging:%d", iommu_cap_afl(dmar_unit->cap));
	pr_info("\tRequiredWBFlush:%d", iommu_cap_rwbf(dmar_unit->cap));
	pr_info("\tProtectedLowMemRegion:%d", iommu_cap_plmr(dmar_unit->cap));
	pr_info("\tProtectedHighMemRegion:%d", iommu_cap_phmr(dmar_unit->cap));
	pr_info("\tCachingMode:%d", iommu_cap_caching_mode(dmar_unit->cap));
	pr_info("\tSAGAW:0x%x", iommu_cap_sagaw(dmar_unit->cap));
	pr_info("\tMGAW:%d", iommu_cap_mgaw(dmar_unit->cap));
	pr_info("\tZeroLenRead:%d", iommu_cap_zlr(dmar_unit->cap));
	pr_info("\tLargePageSupport:0x%x", iommu_cap_super_page_val(dmar_unit->cap));
	pr_info("\tPageSelectiveInvalidation:%d", iommu_cap_pgsel_inv(dmar_unit->cap));
	pr_info("\tPageSelectInvalidation:%d", iommu_cap_pgsel_inv(dmar_unit->cap));
	pr_info("\tNumOfFaultRecordingReg:%d", iommu_cap_num_fault_regs(dmar_unit->cap));
	pr_info("\tMAMV:0x%x", iommu_cap_max_amask_val(dmar_unit->cap));
	pr_info("\tWriteDraining:%d", iommu_cap_write_drain(dmar_unit->cap));
	pr_info("\tReadDraining:%d", iommu_cap_read_drain(dmar_unit->cap));
	pr_info("\tPostInterrupts:%d\n", iommu_cap_pi(dmar_unit->cap));
	pr_info("\tPage-walk Coherency:%d", iommu_ecap_c(dmar_unit->ecap));
	pr_info("\tQueuedInvalidation:%d", iommu_ecap_qi(dmar_unit->ecap));
	pr_info("\tDeviceTLB:%d", iommu_ecap_dt(dmar_unit->ecap));
	pr_info("\tInterruptRemapping:%d", iommu_ecap_ir(dmar_unit->ecap));
	pr_info("\tExtendedInterruptMode:%d", iommu_ecap_eim(dmar_unit->ecap));
	pr_info("\tPassThrough:%d", iommu_ecap_pt(dmar_unit->ecap));
	pr_info("\tSnoopControl:%d", iommu_ecap_sc(dmar_unit->ecap));
	pr_info("\tIOTLB RegOffset:0x%x", iommu_ecap_iro(dmar_unit->ecap));
	pr_info("\tMHMV:0x%x", iommu_ecap_mhmv(dmar_unit->ecap));
	pr_info("\tECS:%d", iommu_ecap_ecs(dmar_unit->ecap));
	pr_info("\tMTS:%d", iommu_ecap_mts(dmar_unit->ecap));
	pr_info("\tNEST:%d", iommu_ecap_nest(dmar_unit->ecap));
	pr_info("\tDIS:%d", iommu_ecap_dis(dmar_unit->ecap));
	pr_info("\tPRS:%d", iommu_ecap_prs(dmar_unit->ecap));
	pr_info("\tERS:%d", iommu_ecap_ers(dmar_unit->ecap));
	pr_info("\tSRS:%d", iommu_ecap_srs(dmar_unit->ecap));
	pr_info("\tNWFS:%d", iommu_ecap_nwfs(dmar_unit->ecap));
	pr_info("\tEAFS:%d", iommu_ecap_eafs(dmar_unit->ecap));
	pr_info("\tPSS:0x%x", iommu_ecap_pss(dmar_unit->ecap));
	pr_info("\tPASID:%d", iommu_ecap_pasid(dmar_unit->ecap));
	pr_info("\tDIT:%d", iommu_ecap_dit(dmar_unit->ecap));
	pr_info("\tPDS:%d\n", iommu_ecap_pds(dmar_unit->ecap));
}
#endif

static inline uint8_t width_to_level(uint32_t width)
{
	return (uint8_t)(((width - 12U) + (LEVEL_WIDTH)-1U) / (LEVEL_WIDTH));
}

static inline uint8_t width_to_agaw(uint32_t width)
{
	return width_to_level(width) - 2U;
}

static uint8_t dmar_unit_get_msagw(const struct dmar_drhd_rt *dmar_unit)
{
	uint8_t i;
	uint8_t sgaw = iommu_cap_sagaw(dmar_unit->cap);

	for (i = 5U; i > 0U; ) {
		i--;
		if (((1U << i) & sgaw) != 0U) {
			break;
		}
	}
	return i;
}

static bool dmar_unit_support_aw(const struct dmar_drhd_rt *dmar_unit, uint32_t addr_width)
{
	uint8_t aw;

	aw = width_to_agaw(addr_width);

	return (((1U << aw) & iommu_cap_sagaw(dmar_unit->cap)) != 0U);
}

static void dmar_enable_intr_remapping(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status = 0;

	spinlock_obtain(&(dmar_unit->lock));
	if ((dmar_unit->gcmd & DMA_GCMD_IRE) == 0U) {
		dmar_unit->gcmd |= DMA_GCMD_IRE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_IRES, false, &status);
#if DBG_IOMMU
		status = iommu_read32(dmar_unit, DMAR_GSTS_REG);
#endif
	}

	spinlock_release(&(dmar_unit->lock));
	dev_dbg(ACRN_DBG_IOMMU, "%s: gsr:0x%x", __func__, status);
}

static void dmar_enable_translation(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status = 0;

	spinlock_obtain(&(dmar_unit->lock));
	if ((dmar_unit->gcmd & DMA_GCMD_TE) == 0U) {
		dmar_unit->gcmd |= DMA_GCMD_TE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_TES, false, &status);
#if DBG_IOMMU
		status = iommu_read32(dmar_unit, DMAR_GSTS_REG);
#endif
	}

	spinlock_release(&(dmar_unit->lock));

	dev_dbg(ACRN_DBG_IOMMU, "%s: gsr:0x%x", __func__, status);
}

static void dmar_disable_intr_remapping(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status;

	spinlock_obtain(&(dmar_unit->lock));
	if ((dmar_unit->gcmd & DMA_GCMD_IRE) != 0U) {
		dmar_unit->gcmd &= ~DMA_GCMD_IRE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_IRES, true, &status);
	}

	spinlock_release(&(dmar_unit->lock));
}

static void dmar_disable_translation(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status;

	spinlock_obtain(&(dmar_unit->lock));
	if ((dmar_unit->gcmd & DMA_GCMD_TE) != 0U) {
		dmar_unit->gcmd &= ~DMA_GCMD_TE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_TES, true, &status);
	}

	spinlock_release(&(dmar_unit->lock));
}

static int32_t dmar_register_hrhd(struct dmar_drhd_rt *dmar_unit)
{
	int32_t ret = 0;

	dev_dbg(ACRN_DBG_IOMMU, "Register dmar uint [%d] @0x%llx", dmar_unit->index, dmar_unit->drhd->reg_base_addr);

	spinlock_init(&dmar_unit->lock);

	dmar_unit->cap = iommu_read64(dmar_unit, DMAR_CAP_REG);
	dmar_unit->ecap = iommu_read64(dmar_unit, DMAR_ECAP_REG);
	dmar_unit->gcmd = iommu_read32(dmar_unit, DMAR_GCMD_REG);

	dmar_unit->cap_msagaw = dmar_unit_get_msagw(dmar_unit);

	dmar_unit->cap_num_fault_regs = iommu_cap_num_fault_regs(dmar_unit->cap);
	dmar_unit->cap_fault_reg_offset = iommu_cap_fault_reg_offset(dmar_unit->cap);
	dmar_unit->ecap_iotlb_offset = iommu_ecap_iro(dmar_unit->ecap) * 16U;

#if DBG_IOMMU
	pr_info("version:0x%x, cap:0x%llx, ecap:0x%llx",
		iommu_read32(dmar_unit, DMAR_VER_REG), dmar_unit->cap, dmar_unit->ecap);
	pr_info("sagaw:0x%x, msagaw:0x%x, iotlb offset 0x%x",
		iommu_cap_sagaw(dmar_unit->cap), dmar_unit->cap_msagaw, dmar_unit->ecap_iotlb_offset);

	dmar_unit_show_capability(dmar_unit);
#endif

	/* check capability */
	if ((iommu_cap_super_page_val(dmar_unit->cap) & 0x1U) == 0U) {
		pr_fatal("%s: dmar uint doesn't support 2MB page!\n", __func__);
		ret = -ENODEV;
	} else if ((iommu_cap_super_page_val(dmar_unit->cap) & 0x2U) == 0U) {
		pr_fatal("%s: dmar uint doesn't support 1GB page!\n", __func__);
		ret = -ENODEV;
	} else if (iommu_ecap_qi(dmar_unit->ecap) == 0U) {
		pr_fatal("%s: dmar unit doesn't support Queued Invalidation!", __func__);
		ret = -ENODEV;
	} else if (iommu_ecap_ir(dmar_unit->ecap) == 0U) {
		pr_fatal("%s: dmar unit doesn't support Interrupt Remapping!", __func__);
		ret = -ENODEV;
	} else if (iommu_ecap_eim(dmar_unit->ecap) == 0U) {
		pr_fatal("%s: dmar unit doesn't support Extended Interrupt Mode!", __func__);
		ret = -ENODEV;
	} else {
		if ((iommu_ecap_c(dmar_unit->ecap) == 0U) && (dmar_unit->drhd->ignore != 0U)) {
			iommu_page_walk_coherent = false;
		}

		/* when the hardware support snoop control,
		 * to make sure snoop control is always enabled,
		 * the SNP filed in the leaf PTE should be set.
		 * How to guarantee it when EPT is used as second-level
		 * translation paging structures?
		 */
		if (iommu_ecap_sc(dmar_unit->ecap) == 0U) {
			dev_dbg(ACRN_DBG_IOMMU, "dmar uint doesn't support snoop control!");
		}

		dmar_disable_translation(dmar_unit);
	}

	return ret;
}

static struct dmar_drhd_rt *ioapic_to_dmaru(uint16_t ioapic_id, union pci_bdf *sid)
{
	struct dmar_info *info = get_dmar_info();
	struct dmar_drhd_rt *dmar_unit = NULL;
	uint32_t i, j;
	bool found = false;

	if (info == NULL) {
		pr_fatal("%s: can't find dmar info\n", __func__);
	} else {
		for (j = 0U; j < info->drhd_count; j++) {
			dmar_unit = &dmar_drhd_units[j];
			for (i = 0U; i < dmar_unit->drhd->dev_cnt; i++) {
				if ((dmar_unit->drhd->devices[i].type == ACPI_DMAR_SCOPE_TYPE_IOAPIC) &&
						(dmar_unit->drhd->devices[i].id == ioapic_id)) {
					sid->bits.f = pci_func((uint8_t)dmar_unit->drhd->devices[i].devfun);
					sid->bits.d = pci_slot((uint8_t)dmar_unit->drhd->devices[i].devfun);
					sid->bits.b = dmar_unit->drhd->devices[i].bus;
					found = true;
					break;
				}
			}
			if (found) {
				break;
			}
		}

		if (j == info->drhd_count) {
			dmar_unit = NULL;
		}
	}

	return dmar_unit;
}

static struct dmar_drhd_rt *device_to_dmaru(uint16_t segment, uint8_t bus, uint8_t devfun)
{
	struct dmar_info *info = get_dmar_info();
	struct dmar_drhd_rt *dmar_unit = NULL;
	uint32_t i, j;

	if (info == NULL) {
		pr_fatal("%s: can't find dmar info\n", __func__);
	} else {
		for (j = 0U; j < info->drhd_count; j++) {
			dmar_unit = &dmar_drhd_units[j];

			if (dmar_unit->drhd->segment != segment) {
				continue;
			}

			for (i = 0U; i < dmar_unit->drhd->dev_cnt; i++) {
				if ((dmar_unit->drhd->devices[i].bus == bus) &&
						(dmar_unit->drhd->devices[i].devfun == devfun)) {
					break;
				}
			}

			/* found exact one or the one which has the same segment number with INCLUDE_PCI_ALL set */
			if ((i != dmar_unit->drhd->dev_cnt) || ((dmar_unit->drhd->flags & DRHD_FLAG_INCLUDE_PCI_ALL_MASK) != 0U)) {
				break;
			}
		}

		/* not found */
		if (j == info->drhd_count) {
			dmar_unit = NULL;
		}
	}

	return dmar_unit;
}

static void dmar_issue_qi_request(struct dmar_drhd_rt *dmar_unit, struct dmar_qi_desc invalidate_desc)
{
	struct dmar_qi_desc *invalidate_desc_ptr;
	__unused uint64_t start;

	invalidate_desc_ptr = (struct dmar_qi_desc *)(dmar_unit->qi_queue + dmar_unit->qi_tail);

	invalidate_desc_ptr->upper = invalidate_desc.upper;
	invalidate_desc_ptr->lower = invalidate_desc.lower;
	dmar_unit->qi_tail = (dmar_unit->qi_tail + DMAR_QI_INV_ENTRY_SIZE) % DMAR_INVALIDATION_QUEUE_SIZE;

	invalidate_desc_ptr++;

	invalidate_desc_ptr->upper = hva2hpa(&qi_status);
	invalidate_desc_ptr->lower = DMAR_INV_WAIT_DESC_LOWER;
	dmar_unit->qi_tail = (dmar_unit->qi_tail + DMAR_QI_INV_ENTRY_SIZE) % DMAR_INVALIDATION_QUEUE_SIZE;

	qi_status = DMAR_INV_STATUS_INCOMPLETE;
	iommu_write32(dmar_unit, DMAR_IQT_REG, dmar_unit->qi_tail);

	start = rdtsc();
	while (qi_status == DMAR_INV_STATUS_INCOMPLETE) {
		if (qi_status == DMAR_INV_STATUS_COMPLETED) {
			break;
		}
		if ((rdtsc() - start) > CYCLES_PER_MS) {
			pr_err("DMAR OP Timeout! @ %s", __func__);
		}
		asm_pause();
	}
}

/*
 * did: domain id
 * sid: source id
 * fm: function mask
 * cirg: cache-invalidation request granularity
 */
static void dmar_invalid_context_cache(struct dmar_drhd_rt *dmar_unit,
	uint16_t did, uint16_t sid, uint8_t fm, enum dmar_cirg_type cirg)
{
	struct dmar_qi_desc invalidate_desc;

	invalidate_desc.upper = 0UL;
	invalidate_desc.lower = DMAR_INV_CONTEXT_CACHE_DESC;
	switch (cirg) {
	case DMAR_CIRG_GLOBAL:
		invalidate_desc.lower |= DMA_CONTEXT_GLOBAL_INVL;
		break;
	case DMAR_CIRG_DOMAIN:
		invalidate_desc.lower |= DMA_CONTEXT_DOMAIN_INVL | dma_ccmd_did(did);
		break;
	case DMAR_CIRG_DEVICE:
		invalidate_desc.lower |= DMA_CCMD_DEVICE_INVL | dma_ccmd_did(did) | dma_ccmd_sid(sid) | dma_ccmd_fm(fm);
		break;
	default:
		invalidate_desc.lower = 0UL;
		pr_err("unknown CIRG type");
		break;
	}

	if (invalidate_desc.lower != 0UL) {
		spinlock_obtain(&(dmar_unit->lock));

		dmar_issue_qi_request(dmar_unit, invalidate_desc);

		spinlock_release(&(dmar_unit->lock));
	}
}

static void dmar_invalid_context_cache_global(struct dmar_drhd_rt *dmar_unit)
{
	dmar_invalid_context_cache(dmar_unit, 0U, 0U, 0U, DMAR_CIRG_GLOBAL);
}

static void dmar_invalid_iotlb(struct dmar_drhd_rt *dmar_unit, uint16_t did, uint64_t address, uint8_t am,
			       bool hint, enum dmar_iirg_type iirg)
{
	/* set Drain Reads & Drain Writes,
	 * if hardware doesn't support it, will be ignored by hardware
	 */
	struct dmar_qi_desc invalidate_desc;
	uint64_t addr = 0UL;

	invalidate_desc.upper = 0UL;

	invalidate_desc.lower = DMA_IOTLB_DR | DMA_IOTLB_DW | DMAR_INV_IOTLB_DESC;

	switch (iirg) {
	case DMAR_IIRG_GLOBAL:
		invalidate_desc.lower |= DMA_IOTLB_GLOBAL_INVL;
		break;
	case DMAR_IIRG_DOMAIN:
		invalidate_desc.lower |= DMA_IOTLB_DOMAIN_INVL | dma_iotlb_did(did);
		break;
	case DMAR_IIRG_PAGE:
		invalidate_desc.lower |= DMA_IOTLB_PAGE_INVL | dma_iotlb_did(did);
		addr = address | dma_iotlb_invl_addr_am(am);
		if (hint) {
			addr |= DMA_IOTLB_INVL_ADDR_IH_UNMODIFIED;
		}
		invalidate_desc.upper |= addr;
		break;
	default:
		invalidate_desc.lower = 0UL;
		pr_err("unknown IIRG type");
	}

	if (invalidate_desc.lower != 0UL) {
		spinlock_obtain(&(dmar_unit->lock));

		dmar_issue_qi_request(dmar_unit, invalidate_desc);

		spinlock_release(&(dmar_unit->lock));
	}
}

/* Invalidate IOTLB globally,
 * all iotlb entries are invalidated,
 * all PASID-cache entries are invalidated,
 * all paging-structure-cache entries are invalidated.
 */
static void dmar_invalid_iotlb_global(struct dmar_drhd_rt *dmar_unit)
{
	dmar_invalid_iotlb(dmar_unit, 0U, 0UL, 0U, false, DMAR_IIRG_GLOBAL);
}

static void dmar_set_intr_remap_table(struct dmar_drhd_rt *dmar_unit)
{
	uint64_t address;
	uint32_t status;
	uint8_t size;

	spinlock_obtain(&(dmar_unit->lock));

	if (dmar_unit->ir_table_addr == 0UL) {
		dmar_unit->ir_table_addr = hva2hpa(get_ir_table(dmar_unit->index));
	}

	address = dmar_unit->ir_table_addr | DMAR_IR_ENABLE_EIM;

	/* Set number of bits needed to represent the entries minus 1 */
	size = (uint8_t) fls32(CONFIG_MAX_IR_ENTRIES) - 1U;
	address = address | size;

	iommu_write64(dmar_unit, DMAR_IRTA_REG, address);

	iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd | DMA_GCMD_SIRTP);

	dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_IRTPS, false, &status);

	spinlock_release(&(dmar_unit->lock));
}

static void dmar_invalid_iec(struct dmar_drhd_rt *dmar_unit, uint16_t intr_index,
				uint8_t index_mask, bool is_global)
{
	struct dmar_qi_desc invalidate_desc;

	invalidate_desc.upper = 0UL;
	invalidate_desc.lower = DMAR_INV_IEC_DESC;

	if (is_global) {
		invalidate_desc.lower |= DMAR_IEC_GLOBAL_INVL;
	} else {
		invalidate_desc.lower |= DMAR_IECI_INDEXED | dma_iec_index(intr_index, index_mask);
	}

	if (invalidate_desc.lower != 0UL) {
		spinlock_obtain(&(dmar_unit->lock));

		dmar_issue_qi_request(dmar_unit, invalidate_desc);

		spinlock_release(&(dmar_unit->lock));
	}
}

static void dmar_invalid_iec_global(struct dmar_drhd_rt *dmar_unit)
{
	dmar_invalid_iec(dmar_unit, 0U, 0U, true);
}

static void dmar_set_root_table(struct dmar_drhd_rt *dmar_unit)
{
	uint64_t address;
	uint32_t status;

	spinlock_obtain(&(dmar_unit->lock));

	/*
	 * dmar_set_root_table is called from init_iommu and
	 * resume_iommu. So NULL check on this pointer is needed
	 * so that we do not change the root table pointer in the
	 * resume flow.
	 */

	if (dmar_unit->root_table_addr == 0UL) {
		dmar_unit->root_table_addr = hva2hpa(get_root_table(dmar_unit->index));
	}

	/* Currently don't support extended root table */
	address = dmar_unit->root_table_addr;

	iommu_write64(dmar_unit, DMAR_RTADDR_REG, address);

	iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd | DMA_GCMD_SRTP);

	/* 32-bit register */
	dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_RTPS, false, &status);
	spinlock_release(&(dmar_unit->lock));
}

static void dmar_fault_event_mask(struct dmar_drhd_rt *dmar_unit)
{
	spinlock_obtain(&(dmar_unit->lock));
	iommu_write32(dmar_unit, DMAR_FECTL_REG, DMA_FECTL_IM);
	spinlock_release(&(dmar_unit->lock));
}

static void dmar_fault_event_unmask(struct dmar_drhd_rt *dmar_unit)
{
	spinlock_obtain(&(dmar_unit->lock));
	iommu_write32(dmar_unit, DMAR_FECTL_REG, 0U);
	spinlock_release(&(dmar_unit->lock));
}

static void dmar_fault_msi_write(struct dmar_drhd_rt *dmar_unit,
			uint32_t vector)
{
	uint32_t data;
	uint32_t addr_low;
	uint32_t lapic_id = get_cur_lapic_id();

	data = DMAR_MSI_DELIVERY_LOWPRI | vector;
	/* redirection hint: 0
	 * destination mode: 0
	 */
	addr_low = 0xFEE00000U | ((uint32_t)(lapic_id) << 12U);

	spinlock_obtain(&(dmar_unit->lock));
	iommu_write32(dmar_unit, DMAR_FEDATA_REG, data);
	iommu_write32(dmar_unit, DMAR_FEADDR_REG, addr_low);
	spinlock_release(&(dmar_unit->lock));
}

#if DBG_IOMMU
static void fault_status_analysis(uint32_t status)
{
	if (dma_fsts_pfo(status)) {
		pr_info("Primary Fault Overflow");
	}

	if (dma_fsts_ppf(status)) {
		pr_info("Primary Pending Fault");
	}

	if (dma_fsts_afo(status)) {
		pr_info("Advanced Fault Overflow");
	}

	if (dma_fsts_apf(status)) {
		pr_info("Advanced Pending Fault");
	}

	if (dma_fsts_iqe(status)) {
		pr_info("Invalidation Queue Error");
	}

	if (dma_fsts_ice(status)) {
		pr_info("Invalidation Completion Error");
	}

	if (dma_fsts_ite(status)) {
		pr_info("Invalidation Time-out Error");
	}

	if (dma_fsts_pro(status)) {
		pr_info("Page Request Overflow");
	}
}
#endif

static void fault_record_analysis(__unused uint64_t low, uint64_t high)
{
	if (!dma_frcd_up_f(high)) {
		/* currently skip PASID related parsing */
		pr_info("%s, Reason: 0x%x, SID: %x.%x.%x @0x%llx",
			(dma_frcd_up_t(high) != 0U) ? "Read/Atomic" : "Write", dma_frcd_up_fr(high),
			pci_bus(dma_frcd_up_sid(high)), pci_slot(dma_frcd_up_sid(high)), pci_func(dma_frcd_up_sid(high)), low);
#if DBG_IOMMU
		if (iommu_ecap_dt(dmar_unit->ecap) != 0U) {
			pr_info("Address Type: 0x%x", dma_frcd_up_at(high));
		}
#endif
	}
}

static void dmar_fault_handler(uint32_t irq, void *data)
{
	struct dmar_drhd_rt *dmar_unit = (struct dmar_drhd_rt *)data;
	uint32_t fsr;
	uint32_t index;
	uint32_t record_reg_offset;
	uint64_t record[2];
	int32_t loop = 0;

	dev_dbg(ACRN_DBG_IOMMU, "%s: irq = %d", __func__, irq);

	fsr = iommu_read32(dmar_unit, DMAR_FSTS_REG);

#if DBG_IOMMU
	fault_status_analysis(fsr);
#endif

	while (dma_fsts_ppf(fsr)) {
		loop++;
		index = dma_fsts_fri(fsr);
		record_reg_offset = (uint32_t)dmar_unit->cap_fault_reg_offset + (index * 16U);
		if (index >= dmar_unit->cap_num_fault_regs) {
			dev_dbg(ACRN_DBG_IOMMU, "%s: invalid FR Index", __func__);
			break;
		}

		/* read 128-bit fault recording register */
		record[0] = iommu_read64(dmar_unit, record_reg_offset);
		record[1] = iommu_read64(dmar_unit, record_reg_offset + 8U);

		dev_dbg(ACRN_DBG_IOMMU, "%s: record[%d] @0x%x:  0x%llx, 0x%llx",
			__func__, index, record_reg_offset, record[0], record[1]);

		fault_record_analysis(record[0], record[1]);

		/* write to clear */
		iommu_write64(dmar_unit, record_reg_offset, record[0]);
		iommu_write64(dmar_unit, record_reg_offset + 8U, record[1]);

#ifdef DMAR_FAULT_LOOP_MAX
		if (loop > DMAR_FAULT_LOOP_MAX) {
			dev_dbg(ACRN_DBG_IOMMU, "%s: loop more than %d times", __func__, DMAR_FAULT_LOOP_MAX);
			break;
		}
#endif

		fsr = iommu_read32(dmar_unit, DMAR_FSTS_REG);
	}
}

static void dmar_setup_interrupt(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t vector;
	int32_t retval = 0;

	spinlock_obtain(&(dmar_unit->lock));
	if (dmar_unit->dmar_irq == IRQ_INVALID) {
		retval = request_irq(IRQ_INVALID, dmar_fault_handler, dmar_unit, IRQF_NONE);
		dmar_unit->dmar_irq = (uint32_t)retval;
	}
	spinlock_release(&(dmar_unit->lock));
	/* the panic will only happen before any VM starts running */
	if (retval < 0 ) {
		panic("dmar[%d] fail to setup interrupt", dmar_unit->index);
	}

	vector = irq_to_vector(dmar_unit->dmar_irq);
	dev_dbg(ACRN_DBG_IOMMU, "irq#%d vector#%d for dmar_unit", dmar_unit->dmar_irq, vector);

	dmar_fault_msi_write(dmar_unit, vector);
	dmar_fault_event_unmask(dmar_unit);
}

static void dmar_enable_qi(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status = 0;

	dmar_unit->qi_queue = hva2hpa(get_qi_queue(dmar_unit->index));
	iommu_write64(dmar_unit, DMAR_IQA_REG, dmar_unit->qi_queue);

	iommu_write32(dmar_unit, DMAR_IQT_REG, 0U);

	if ((dmar_unit->gcmd & DMA_GCMD_QIE) == 0U) {
		dmar_unit->gcmd |= DMA_GCMD_QIE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG,	dmar_unit->gcmd);
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_QIES, false, &status);
	}
}

static void dmar_disable_qi(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status = 0;

	if ((dmar_unit->gcmd & DMA_GCMD_QIE) == DMA_GCMD_QIE) {
		dmar_unit->gcmd &= ~DMA_GCMD_QIE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG,	dmar_unit->gcmd);
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_QIES, true, &status);
	}
}

static void dmar_prepare(struct dmar_drhd_rt *dmar_unit)
{
	dev_dbg(ACRN_DBG_IOMMU, "enable dmar uint [0x%x]", dmar_unit->drhd->reg_base_addr);
	dmar_setup_interrupt(dmar_unit);
	dmar_set_root_table(dmar_unit);
	dmar_enable_qi(dmar_unit);
	dmar_set_intr_remap_table(dmar_unit);
}

static void dmar_enable(struct dmar_drhd_rt *dmar_unit)
{
	dev_dbg(ACRN_DBG_IOMMU, "enable dmar uint [0x%x]", dmar_unit->drhd->reg_base_addr);
	dmar_invalid_context_cache_global(dmar_unit);
	dmar_invalid_iotlb_global(dmar_unit);
	dmar_enable_translation(dmar_unit);
}

static void dmar_disable(struct dmar_drhd_rt *dmar_unit)
{
	dmar_disable_qi(dmar_unit);
	dmar_disable_translation(dmar_unit);
	dmar_fault_event_mask(dmar_unit);
	dmar_disable_intr_remapping(dmar_unit);
}

static void dmar_suspend(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t i;

	dmar_invalid_context_cache_global(dmar_unit);
	dmar_invalid_iotlb_global(dmar_unit);

	dmar_disable(dmar_unit);

	/* save IOMMU fault register state */
	for (i = 0U; i < IOMMU_FAULT_REGISTER_STATE_NUM; i++) {
		dmar_unit->fault_state[i] =  iommu_read32(dmar_unit, DMAR_FECTL_REG + (i * IOMMU_FAULT_REGISTER_SIZE));
	}
}

static void dmar_resume(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t i;

	/* restore IOMMU fault register state */
	for (i = 0U; i < IOMMU_FAULT_REGISTER_STATE_NUM; i++) {
		iommu_write32(dmar_unit, DMAR_FECTL_REG + (i * IOMMU_FAULT_REGISTER_SIZE), dmar_unit->fault_state[i]);
	}
	dmar_prepare(dmar_unit);
	dmar_enable(dmar_unit);
	dmar_enable_intr_remapping(dmar_unit);
}

static int32_t add_iommu_device(struct iommu_domain *domain, uint16_t segment, uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_unit;
	struct dmar_root_entry *root_table;
	uint64_t context_table_addr;
	struct dmar_context_entry *context;
	struct dmar_root_entry *root_entry;
	struct dmar_context_entry *context_entry;
	uint64_t upper;
	uint64_t lower = 0UL;
	struct acrn_vm *vm;
	int32_t ret = 0;

	dmar_unit = device_to_dmaru(segment, bus, devfun);
	if (dmar_unit == NULL) {
		pr_err("no dmar unit found for device: %x:%x.%x", bus, pci_slot(devfun), pci_func(devfun));
		ret = -EINVAL;
	} else if (dmar_unit->drhd->ignore) {
		dev_dbg(ACRN_DBG_IOMMU, "device is ignored :0x%x:%x.%x", bus, pci_slot(devfun), pci_func(devfun));
	} else if ((!dmar_unit_support_aw(dmar_unit, domain->addr_width)) || (dmar_unit->root_table_addr == 0UL)) {
		pr_err("invalid dmar unit");
		ret = -EINVAL;
	} else {
		if (iommu_ecap_sc(dmar_unit->ecap) == 0U) {
			vm = get_vm_from_vmid(domain->vm_id);
			if (vm != NULL) {
				vm->snoopy_mem = false;
			}
			/* TODO: remove iommu_snoop from iommu_domain */
			domain->iommu_snoop = false;
			dev_dbg(ACRN_DBG_IOMMU, "vm=%d add %x:%x no snoop control!", domain->vm_id, bus, devfun);
		}

		root_table = (struct dmar_root_entry *)hpa2hva(dmar_unit->root_table_addr);

		root_entry = root_table + bus;

		if (dmar_get_bitslice(root_entry->lower,
					ROOT_ENTRY_LOWER_PRESENT_MASK,
					ROOT_ENTRY_LOWER_PRESENT_POS) == 0UL) {
			/* create context table for the bus if not present */
			context_table_addr = hva2hpa(get_ctx_table(dmar_unit->index, bus));

			context_table_addr = context_table_addr >> PAGE_SHIFT;

			lower = dmar_set_bitslice(lower,
					ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS, context_table_addr);
			lower = dmar_set_bitslice(lower,
					ROOT_ENTRY_LOWER_PRESENT_MASK, ROOT_ENTRY_LOWER_PRESENT_POS, 1UL);

			root_entry->upper = 0UL;
			root_entry->lower = lower;
			iommu_flush_cache(dmar_unit, root_entry, sizeof(struct dmar_root_entry));
		} else {
			context_table_addr = dmar_get_bitslice(root_entry->lower,
					ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS);
		}

		context_table_addr = context_table_addr << PAGE_SHIFT;

		context = (struct dmar_context_entry *)hpa2hva(context_table_addr);
		context_entry = context + devfun;

		if (context_entry == NULL) {
			pr_err("dmar context entry is invalid");
			ret = -EINVAL;
		} else if (dmar_get_bitslice(context_entry->lower, CTX_ENTRY_LOWER_P_MASK, CTX_ENTRY_LOWER_P_POS) != 0UL) {
			/* the context entry should not be present */
			pr_err("%s: context entry@0x%llx (Lower:%x) ", __func__, context_entry, context_entry->lower);
			pr_err("already present for %x:%x.%x", bus, pci_slot(devfun), pci_func(devfun));
			ret = -EBUSY;
		} else {
			/* setup context entry for the devfun */
			upper = 0UL;
			lower = 0UL;
			if (domain->is_host) {
				if (iommu_ecap_pt(dmar_unit->ecap) != 0U) {
					/* When the Translation-type (T) field indicates
					 * pass-through processing (10b), AW field must be
					 * programmed to indicate the largest AGAW value
					 * supported by hardware.
					 */
					upper = dmar_set_bitslice(upper,
							CTX_ENTRY_UPPER_AW_MASK, CTX_ENTRY_UPPER_AW_POS, dmar_unit->cap_msagaw);
					lower = dmar_set_bitslice(lower,
							CTX_ENTRY_LOWER_TT_MASK, CTX_ENTRY_LOWER_TT_POS, DMAR_CTX_TT_PASSTHROUGH);
				} else {
					pr_err("dmaru[%d] doesn't support trans passthrough", dmar_unit->index);
					ret = -ENODEV;
				}
			} else {
				/* TODO: add Device TLB support */
				upper = dmar_set_bitslice(upper,
						CTX_ENTRY_UPPER_AW_MASK, CTX_ENTRY_UPPER_AW_POS, (uint64_t)width_to_agaw(domain->addr_width));
				lower = dmar_set_bitslice(lower,
						CTX_ENTRY_LOWER_TT_MASK, CTX_ENTRY_LOWER_TT_POS, DMAR_CTX_TT_UNTRANSLATED);
			}

			if (ret == 0) {
				upper = dmar_set_bitslice(upper,
						CTX_ENTRY_UPPER_DID_MASK, CTX_ENTRY_UPPER_DID_POS, (uint64_t)vmid_to_domainid(domain->vm_id));
				lower = dmar_set_bitslice(lower,
						CTX_ENTRY_LOWER_SLPTPTR_MASK, CTX_ENTRY_LOWER_SLPTPTR_POS, domain->trans_table_ptr >> PAGE_SHIFT);
				lower = dmar_set_bitslice(lower, CTX_ENTRY_LOWER_P_MASK, CTX_ENTRY_LOWER_P_POS, 1UL);

				context_entry->upper = upper;
				context_entry->lower = lower;
				iommu_flush_cache(dmar_unit, context_entry, sizeof(struct dmar_context_entry));
			}
		}
	}

	return ret;
}

static int32_t remove_iommu_device(const struct iommu_domain *domain, uint16_t segment, uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_unit;
	struct dmar_root_entry *root_table;
	uint64_t context_table_addr;
	struct dmar_context_entry *context;
	struct dmar_root_entry *root_entry;
	struct dmar_context_entry *context_entry;
	int32_t ret = 0;

	dmar_unit = device_to_dmaru(segment, bus, devfun);
	if (dmar_unit == NULL) {
		pr_err("no dmar unit found for device: %x:%x.%x", bus, pci_slot(devfun), pci_func(devfun));
		ret = -EINVAL;
	} else {
		root_table = (struct dmar_root_entry *)hpa2hva(dmar_unit->root_table_addr);
		root_entry = root_table + bus;

		if (root_entry == NULL) {
			pr_err("dmar root table entry is invalid\n");
			ret = -EINVAL;
		} else {
			context_table_addr = dmar_get_bitslice(root_entry->lower,  ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS);
			context_table_addr = context_table_addr << PAGE_SHIFT;
			context = (struct dmar_context_entry *)hpa2hva(context_table_addr);

			context_entry = context + devfun;

			if (context_entry == NULL) {
				pr_err("dmar context entry is invalid");
				ret = -EINVAL;
			} else if ((uint16_t)dmar_get_bitslice(context_entry->upper, CTX_ENTRY_UPPER_DID_MASK, CTX_ENTRY_UPPER_DID_POS) != vmid_to_domainid(domain->vm_id)) {
				pr_err("%s: domain id mismatch", __func__);
				ret = -EPERM;
			} else {
				/* clear the present bit first */
				context_entry->lower = 0UL;
				context_entry->upper = 0UL;
				iommu_flush_cache(dmar_unit, context_entry, sizeof(struct dmar_context_entry));

				dmar_invalid_context_cache_global(dmar_unit);
				dmar_invalid_iotlb_global(dmar_unit);
			}
		}
	}
	return ret;
}

/*
 * @pre action != NULL
 * As an internal API, VT-d code can guarantee action is not NULL.
 */
static void do_action_for_iommus(void (*action)(struct dmar_drhd_rt *))
{
	struct dmar_info *info = get_dmar_info();
	struct dmar_drhd_rt *dmar_unit;
	uint32_t i;

	if (info != NULL) {
		for (i = 0U; i < info->drhd_count; i++) {
			dmar_unit = &dmar_drhd_units[i];
			if (!dmar_unit->drhd->ignore) {
				action(dmar_unit);
			} else {
				dev_dbg(ACRN_DBG_IOMMU, "ignore dmar_unit @0x%x", dmar_unit->drhd->reg_base_addr);
			}
		}
	} else {
		pr_fatal("%s: can't find dmar info\n", __func__);
	}
}

struct iommu_domain *create_iommu_domain(uint16_t vm_id, uint64_t translation_table, uint32_t addr_width)
{
	static struct iommu_domain iommu_domains[MAX_DOMAIN_NUM];
	struct iommu_domain *domain;

	/* TODO: check if a domain with the vm_id exists */

	if (translation_table == 0UL) {
		pr_err("translation table is NULL");
	        domain =  NULL;
	} else {
		/*
		 * A hypercall is called to create an iommu domain for a valid VM,
		 * and hv code limit the VM number to CONFIG_MAX_VM_NUM.
		 * So the array iommu_domains will not be accessed out of range.
		 */
		domain = &iommu_domains[vmid_to_domainid(vm_id)];

		domain->is_host = false;
		domain->vm_id = vm_id;
		domain->trans_table_ptr = translation_table;
		domain->addr_width = addr_width;
		domain->is_tt_ept = true;

		dev_dbg(ACRN_DBG_IOMMU, "create domain [%d]: vm_id = %hu, ept@0x%x",
			vmid_to_domainid(domain->vm_id), domain->vm_id, domain->trans_table_ptr);
	}

	return domain;
}

/**
 * @pre domain != NULL
 */
void destroy_iommu_domain(struct iommu_domain *domain)
{
	/* currently only support ept */
	if (!domain->is_tt_ept) {
		ASSERT(false, "translation_table is not EPT!");
	}

	/* TODO: check if any device assigned to this domain */
	(void)memset(domain, 0U, sizeof(*domain));
}

int32_t assign_iommu_device(struct iommu_domain *domain, uint8_t bus, uint8_t devfun)
{
	int32_t status = 0;

	/* TODO: check if the device assigned */

	if (fallback_iommu_domain != NULL) {
		status = remove_iommu_device(fallback_iommu_domain, 0U, bus, devfun);
	}

	if (status == 0) {
		status = add_iommu_device(domain, 0U, bus, devfun);
	}

	return status;
}

int32_t unassign_iommu_device(const struct iommu_domain *domain, uint8_t bus, uint8_t devfun)
{
	int32_t status = 0;

	/* TODO: check if the device assigned */
	status = remove_iommu_device(domain, 0U, bus, devfun);

	if ((status == 0) && (fallback_iommu_domain != NULL)) {
		status = add_iommu_device(fallback_iommu_domain, 0U, bus, devfun);
	}

	return status;
}

void enable_iommu(void)
{
	if (!iommu_page_walk_coherent) {
		cache_flush_invalidate_all();
	}
	do_action_for_iommus(dmar_enable);
}

void disable_iommu(void)
{
	do_action_for_iommus(dmar_disable);
}

void suspend_iommu(void)
{
	do_action_for_iommus(dmar_suspend);
}

void resume_iommu(void)
{
	do_action_for_iommus(dmar_resume);
}

int32_t init_iommu(void)
{
	int32_t ret = 0;

	ret = register_hrhd_units();
	if (ret == 0) {
		do_action_for_iommus(dmar_prepare);
	}

	return ret;
}

void init_fallback_iommu_domain(struct iommu_domain *iommu_dmn, uint16_t vm_id, void *eptp)
{
	uint16_t bus;
	uint16_t devfun;

	iommu_dmn = create_iommu_domain(vm_id, hva2hpa(eptp), 48U);

	fallback_iommu_domain = (struct iommu_domain *) iommu_dmn;
	if (fallback_iommu_domain == NULL) {
		pr_err("fallback_iommu_domain is NULL\n");
	} else {
		for (bus = 0U; bus < CONFIG_IOMMU_BUS_NUM; bus++) {
			for (devfun = 0U; devfun <= 255U; devfun++) {
				if (add_iommu_device(fallback_iommu_domain, 0U, (uint8_t)bus, (uint8_t)devfun) != 0) {
					/* the panic only occurs before fallback_iommu_domain starts running in sharing mode */
					panic("Failed to add %x:%x.%x to fallback_iommu_domain domain", bus, pci_slot(devfun), pci_func(devfun));
				}
			}
		}
	}
}

int32_t dmar_assign_irte(struct intr_source intr_src, union dmar_ir_entry irte, uint16_t index)
{
	struct dmar_drhd_rt *dmar_unit;
	union dmar_ir_entry *ir_table, *ir_entry;
	union pci_bdf sid;
	uint64_t trigger_mode;
	int32_t ret = 0;

	if (intr_src.is_msi) {
		dmar_unit = device_to_dmaru(0U, (uint8_t)intr_src.src.msi.bits.b, pci_devfn(intr_src.src.msi.value));
		sid.value = intr_src.src.msi.value;
		trigger_mode = 0x0UL;
	} else {
		dmar_unit = ioapic_to_dmaru(intr_src.src.ioapic_id, &sid);
		trigger_mode = irte.bits.trigger_mode;
	}

	if (dmar_unit == NULL) {
		pr_err("no dmar unit found for device: %x:%x.%x", sid.bits.b, sid.bits.d, sid.bits.f);
		ret = -EINVAL;
	} else if (dmar_unit->drhd->ignore) {
		dev_dbg(ACRN_DBG_IOMMU, "device is ignored :0x%x:%x.%x", sid.bits.b, sid.bits.d, sid.bits.f);
		ret = -EINVAL;
	} else if (dmar_unit->ir_table_addr == 0UL) {
		pr_err("IR table is not set for dmar unit");
		ret = -EINVAL;
	} else {
		dmar_enable_intr_remapping(dmar_unit);
		irte.bits.svt = 0x1UL;
		irte.bits.sq = 0x0UL;
		irte.bits.sid = sid.value;
		irte.bits.present = 0x1UL;
		irte.bits.mode = 0x0UL;
		irte.bits.trigger_mode = trigger_mode;
		irte.bits.fpd = 0x0UL;
		ir_table = (union dmar_ir_entry *)hpa2hva(dmar_unit->ir_table_addr);
		ir_entry = ir_table + index;
		ir_entry->entry.upper = irte.entry.upper;
		ir_entry->entry.lower = irte.entry.lower;

		iommu_flush_cache(dmar_unit, ir_entry, sizeof(union dmar_ir_entry));
		dmar_invalid_iec_global(dmar_unit);
	}
	return ret;
}

void dmar_free_irte(struct intr_source intr_src, uint16_t index)
{
	struct dmar_drhd_rt *dmar_unit;
	union dmar_ir_entry *ir_table, *ir_entry;
	union pci_bdf sid;

	if (intr_src.is_msi) {
		dmar_unit = device_to_dmaru(0U, (uint8_t)intr_src.src.msi.bits.b, pci_devfn(intr_src.src.msi.value));
	} else {
		dmar_unit = ioapic_to_dmaru(intr_src.src.ioapic_id, &sid);
	}

	if (dmar_unit == NULL) {
		pr_err("no dmar unit found for device: %x:%x.%x", intr_src.src.msi.bits.b,
			intr_src.src.msi.bits.d, intr_src.src.msi.bits.f);
	} else if (dmar_unit->drhd->ignore) {
		dev_dbg(ACRN_DBG_IOMMU, "device is ignored :0x%x:%x.%x", intr_src.src.msi.bits.b,
			intr_src.src.msi.bits.d, intr_src.src.msi.bits.f);
	} else if (dmar_unit->ir_table_addr == 0UL) {
		pr_err("IR table is not set for dmar unit");
	} else {
		ir_table = (union dmar_ir_entry *)hpa2hva(dmar_unit->ir_table_addr);
		ir_entry = ir_table + index;
		ir_entry->bits.present = 0x0UL;

		iommu_flush_cache(dmar_unit, ir_entry, sizeof(union dmar_ir_entry));
		dmar_invalid_iec_global(dmar_unit);
	}
}
