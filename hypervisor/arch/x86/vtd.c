/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define pr_prefix		"iommu: "

#include <types.h>
#include <errno.h>
#include <asm/lib/bits.h>
#include <asm/lib/spinlock.h>
#include <asm/cpu_caps.h>
#include <irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mmu.h>
#include <asm/lapic.h>
#include <asm/vtd.h>
#include <ticks.h>
#include <logmsg.h>
#include <asm/board.h>
#include <asm/vm_config.h>
#include <pci.h>
#include <asm/platform_caps.h>

#define DBG_IOMMU 0

#if DBG_IOMMU
#define DBG_LEVEL_IOMMU LOG_INFO
#define DMAR_FAULT_LOOP_MAX 10
#else
#define DBG_LEVEL_IOMMU 6U
#endif
#define LEVEL_WIDTH 9U

#define ROOT_ENTRY_LOWER_PRESENT_POS        (0U)
#define ROOT_ENTRY_LOWER_PRESENT_MASK       (1UL << ROOT_ENTRY_LOWER_PRESENT_POS)
#define ROOT_ENTRY_LOWER_CTP_POS            (12U)
#define ROOT_ENTRY_LOWER_CTP_MASK           (0xFFFFFFFFFFFFFUL << ROOT_ENTRY_LOWER_CTP_POS)

#define CONFIG_MAX_IOMMU_NUM		DRHD_COUNT

/* 4 iommu fault register state */
#define	IOMMU_FAULT_REGISTER_STATE_NUM	4U
#define	IOMMU_FAULT_REGISTER_SIZE	4U

#define CTX_ENTRY_UPPER_AW_POS          (0U)
#define CTX_ENTRY_UPPER_AW_MASK         (0x7UL << CTX_ENTRY_UPPER_AW_POS)
#define CTX_ENTRY_UPPER_DID_POS         (8U)
#define CTX_ENTRY_UPPER_DID_MASK        (0xFFFFUL << CTX_ENTRY_UPPER_DID_POS)
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
	uint64_t irte_alloc_bitmap[CONFIG_MAX_IR_ENTRIES / 64U];
	uint64_t irte_reserved_bitmap[CONFIG_MAX_IR_ENTRIES / 64U];
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

struct context_table {
	struct page buses[CONFIG_IOMMU_BUS_NUM];
};

struct intr_remap_table {
	struct page tables[CONFIG_MAX_IR_ENTRIES/DMAR_NUM_IR_ENTRIES_PER_PAGE];
};

static inline uint8_t *get_root_table(uint32_t dmar_index)
{
	static struct page root_tables[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return root_tables[dmar_index].contents;
}

static inline uint8_t *get_ctx_table(uint32_t dmar_index, uint8_t bus_no)
{
	static struct context_table ctx_tables[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return ctx_tables[dmar_index].buses[bus_no].contents;
}

/*
 * @pre dmar_index < CONFIG_MAX_IOMMU_NUM
 */
static inline void *get_qi_queue(uint32_t dmar_index)
{
	static struct page qi_queues[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return (void *)qi_queues[dmar_index].contents;
}

static inline void *get_ir_table(uint32_t dmar_index)
{
	static struct intr_remap_table ir_tables[CONFIG_MAX_IOMMU_NUM] __aligned(PAGE_SIZE);
	return (void *)ir_tables[dmar_index].tables[0].contents;
}

static struct dmar_drhd_rt dmar_drhd_units[MAX_DRHDS];
static bool iommu_page_walk_coherent = true;
static struct dmar_info *platform_dmar_info = NULL;

/* Domain id 0 is reserved in some cases per VT-d */
#define MAX_DOMAIN_NUM (CONFIG_MAX_VM_NUM + 1)

static inline uint16_t vmid_to_domainid(uint16_t vm_id)
{
	return vm_id + 1U;
}

static int32_t dmar_register_hrhd(struct dmar_drhd_rt *dmar_unit);
static struct dmar_drhd_rt *device_to_dmaru(uint8_t bus, uint8_t devfun);

static int32_t register_hrhd_units(void)
{
	struct dmar_drhd_rt *drhd_rt;
	uint32_t i;
	int32_t ret = 0;

	for (i = 0U; i < platform_dmar_info->drhd_count; i++) {
		drhd_rt = &dmar_drhd_units[i];
		drhd_rt->index = i;
		drhd_rt->drhd = &platform_dmar_info->drhd_units[i];
		drhd_rt->dmar_irq = IRQ_INVALID;

		set_paging_supervisor(drhd_rt->drhd->reg_base_addr, PAGE_SIZE);

		ret = dmar_register_hrhd(drhd_rt);
		if (ret != 0) {
			break;
		}

		if ((iommu_cap_pi(drhd_rt->cap) == 0U) || (!is_apicv_advanced_feature_supported())) {
			platform_caps.pi = false;
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
	return mmio_read64(hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static void iommu_write32(const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint32_t value)
{
	mmio_write32(value, hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static void iommu_write64(const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint64_t value)
{
	mmio_write64(value, hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static inline void dmar_wait_completion(const struct dmar_drhd_rt *dmar_unit, uint32_t offset,
	uint32_t mask, uint32_t pre_condition, uint32_t *status)
{
	/* variable start isn't used when built as release version */
	__unused uint64_t start = cpu_ticks();

	do {
		*status = iommu_read32(dmar_unit, offset);
		ASSERT(((cpu_ticks() - start) < TICKS_PER_MS),
			"DMAR OP Timeout!");
		asm_pause();
	} while( (*status & mask) == pre_condition);
}

/* Flush CPU cache when root table, context table or second-level translation teable updated
 * In the context of ACRN, GPA to HPA mapping relationship is not changed after VM created,
 * skip flushing iotlb to avoid performance penalty.
 */
void iommu_flush_cache(const void *p, uint32_t size)
{
	/* if vtd support page-walk coherency, no need to flush cacheline */
	if (!iommu_page_walk_coherent) {
		flush_cache_range(p, size);
	}
}

#if DBG_IOMMU
static inline uint8_t iommu_cap_rwbf(uint64_t cap)
{
	return ((uint8_t)(cap >> 4U) & 1U);
}

static inline uint8_t iommu_ecap_sc(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 7U) & 1U);
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
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_IRES, 0U, &status);
#if DBG_IOMMU
		status = iommu_read32(dmar_unit, DMAR_GSTS_REG);
#endif
	}

	spinlock_release(&(dmar_unit->lock));
	dev_dbg(DBG_LEVEL_IOMMU, "%s: gsr:0x%x", __func__, status);
}

static void dmar_enable_translation(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status = 0;

	spinlock_obtain(&(dmar_unit->lock));
	if ((dmar_unit->gcmd & DMA_GCMD_TE) == 0U) {
		dmar_unit->gcmd |= DMA_GCMD_TE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_TES, 0U, &status);
#if DBG_IOMMU
		status = iommu_read32(dmar_unit, DMAR_GSTS_REG);
#endif
	}

	spinlock_release(&(dmar_unit->lock));

	dev_dbg(DBG_LEVEL_IOMMU, "%s: gsr:0x%x", __func__, status);
}

static void dmar_disable_intr_remapping(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status;

	spinlock_obtain(&(dmar_unit->lock));
	if ((dmar_unit->gcmd & DMA_GCMD_IRE) != 0U) {
		dmar_unit->gcmd &= ~DMA_GCMD_IRE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd);
		/* 32-bit register */
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_IRES, DMA_GSTS_IRES, &status);
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
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_TES, DMA_GSTS_TES, &status);
	}

	spinlock_release(&(dmar_unit->lock));
}

static int32_t dmar_register_hrhd(struct dmar_drhd_rt *dmar_unit)
{
	int32_t ret = 0;

	dev_dbg(DBG_LEVEL_IOMMU, "Register dmar uint [%d] @0x%lx", dmar_unit->index, dmar_unit->drhd->reg_base_addr);

	spinlock_init(&dmar_unit->lock);

	dmar_unit->cap = iommu_read64(dmar_unit, DMAR_CAP_REG);
	dmar_unit->ecap = iommu_read64(dmar_unit, DMAR_ECAP_REG);

	/*
	 * The initialization of "dmar_unit->gcmd" shall be done via reading from Global Status Register rather than
	 * Global Command Register.
	 * According to Chapter 10.4.4 Global Command Register in VT-d spec, Global Command Register is a write-only
	 * register to control remapping hardware. Global Status Register is the corresponding read-only register to
	 * report remapping hardware status.
	 */
	dmar_unit->gcmd = iommu_read32(dmar_unit, DMAR_GSTS_REG);

	dmar_unit->cap_msagaw = dmar_unit_get_msagw(dmar_unit);

	dmar_unit->cap_num_fault_regs = iommu_cap_num_fault_regs(dmar_unit->cap);
	dmar_unit->cap_fault_reg_offset = iommu_cap_fault_reg_offset(dmar_unit->cap);
	dmar_unit->ecap_iotlb_offset = iommu_ecap_iro(dmar_unit->ecap) * 16U;
	dmar_unit->root_table_addr = hva2hpa(get_root_table(dmar_unit->index));
	dmar_unit->ir_table_addr = hva2hpa(get_ir_table(dmar_unit->index));

#if DBG_IOMMU
	pr_info("version:0x%x, cap:0x%lx, ecap:0x%lx",
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
		if ((iommu_ecap_c(dmar_unit->ecap) == 0U) && (!dmar_unit->drhd->ignore)) {
			iommu_page_walk_coherent = false;
		}
		dmar_disable_translation(dmar_unit);
	}

	return ret;
}

static struct dmar_drhd_rt *ioapic_to_dmaru(uint16_t ioapic_id, union pci_bdf *sid)
{
	struct dmar_drhd_rt *dmar_unit = NULL;
	uint32_t i, j;
	bool found = false;

	for (j = 0U; j < platform_dmar_info->drhd_count; j++) {
		dmar_unit = &dmar_drhd_units[j];
		for (i = 0U; i < dmar_unit->drhd->dev_cnt; i++) {
			if ((dmar_unit->drhd->devices[i].type == ACPI_DMAR_SCOPE_TYPE_IOAPIC) &&
					(dmar_unit->drhd->devices[i].id == ioapic_id)) {
				sid->fields.devfun = dmar_unit->drhd->devices[i].devfun;
				sid->fields.bus = dmar_unit->drhd->devices[i].bus;
				found = true;
				break;
			}
		}
		if (found) {
			break;
		}
	}

	if (j == platform_dmar_info->drhd_count) {
		dmar_unit = NULL;
	}

	return dmar_unit;
}

static struct dmar_drhd_rt *device_to_dmaru(uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmaru = NULL;
	uint16_t bdf = ((uint16_t)bus << 8U) | devfun;
	uint32_t index = pci_lookup_drhd_for_pbdf(bdf);

	if (index == INVALID_DRHD_INDEX) {
		pr_fatal("BDF %02x:%02x:%x has no IOMMU\n", bus, devfun >> 3U, devfun & 7U);
		/*
		 * pci_lookup_drhd_for_pbdf would return -1U for any of the reasons
		 * 1) PCI device with bus, devfun does not exist on platform
		 * 2) ACRN had issues finding the device with bus, devfun during init
		 * 3) DMAR tables provided by ACPI for this platform are incorrect
		 */
	} else {
		dmaru = &dmar_drhd_units[index];
	}

	return dmaru;
}

static void dmar_issue_qi_request(struct dmar_drhd_rt *dmar_unit, struct dmar_entry invalidate_desc)
{
	struct dmar_entry *invalidate_desc_ptr;
	uint32_t qi_status = 0U;
	uint64_t start;

	spinlock_obtain(&(dmar_unit->lock));

	invalidate_desc_ptr = (struct dmar_entry *)(dmar_unit->qi_queue + dmar_unit->qi_tail);

	invalidate_desc_ptr->hi_64 = invalidate_desc.hi_64;
	invalidate_desc_ptr->lo_64 = invalidate_desc.lo_64;
	dmar_unit->qi_tail = (dmar_unit->qi_tail + DMAR_QI_INV_ENTRY_SIZE) % DMAR_INVALIDATION_QUEUE_SIZE;

	invalidate_desc_ptr++;

	invalidate_desc_ptr->hi_64 = hva2hpa(&qi_status);
	invalidate_desc_ptr->lo_64 = DMAR_INV_WAIT_DESC_LOWER;
	dmar_unit->qi_tail = (dmar_unit->qi_tail + DMAR_QI_INV_ENTRY_SIZE) % DMAR_INVALIDATION_QUEUE_SIZE;

	qi_status = DMAR_INV_STATUS_INCOMPLETE;
	iommu_write32(dmar_unit, DMAR_IQT_REG, dmar_unit->qi_tail);

	start = cpu_ticks();
	while (qi_status != DMAR_INV_STATUS_COMPLETED) {
		if ((cpu_ticks() - start) > TICKS_PER_MS) {
			pr_err("DMAR OP Timeout! @ %s", __func__);
			break;
		}
		asm_pause();
	}

	spinlock_release(&(dmar_unit->lock));
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
	struct dmar_entry invalidate_desc;

	invalidate_desc.hi_64 = 0UL;
	invalidate_desc.lo_64 = DMAR_INV_CONTEXT_CACHE_DESC;
	switch (cirg) {
	case DMAR_CIRG_GLOBAL:
		invalidate_desc.lo_64 |= DMA_CONTEXT_GLOBAL_INVL;
		break;
	case DMAR_CIRG_DOMAIN:
		invalidate_desc.lo_64 |= DMA_CONTEXT_DOMAIN_INVL | dma_ccmd_did(did);
		break;
	case DMAR_CIRG_DEVICE:
		invalidate_desc.lo_64 |= DMA_CONTEXT_DEVICE_INVL | dma_ccmd_did(did) | dma_ccmd_sid(sid) | dma_ccmd_fm(fm);
		break;
	default:
		invalidate_desc.lo_64 = 0UL;
		pr_err("unknown CIRG type");
		break;
	}

	if (invalidate_desc.lo_64 != 0UL) {
		dmar_issue_qi_request(dmar_unit, invalidate_desc);
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
	struct dmar_entry invalidate_desc;
	uint64_t addr = 0UL;

	invalidate_desc.hi_64 = 0UL;

	invalidate_desc.lo_64 = DMA_IOTLB_DR | DMA_IOTLB_DW | DMAR_INV_IOTLB_DESC;

	switch (iirg) {
	case DMAR_IIRG_GLOBAL:
		invalidate_desc.lo_64 |= DMA_IOTLB_GLOBAL_INVL;
		break;
	case DMAR_IIRG_DOMAIN:
		invalidate_desc.lo_64 |= DMA_IOTLB_DOMAIN_INVL | dma_iotlb_did(did);
		break;
	case DMAR_IIRG_PAGE:
		invalidate_desc.lo_64 |= DMA_IOTLB_PAGE_INVL | dma_iotlb_did(did);
		addr = address | dma_iotlb_invl_addr_am(am);
		if (hint) {
			addr |= DMA_IOTLB_INVL_ADDR_IH_UNMODIFIED;
		}
		invalidate_desc.hi_64 |= addr;
		break;
	default:
		invalidate_desc.lo_64 = 0UL;
		pr_err("unknown IIRG type");
	}

	if (invalidate_desc.lo_64 != 0UL) {
		dmar_issue_qi_request(dmar_unit, invalidate_desc);
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

/* @pre dmar_unit->ir_table_addr != NULL */
static void dmar_set_intr_remap_table(struct dmar_drhd_rt *dmar_unit)
{
	uint64_t address;
	uint32_t status;
	uint8_t size;

	spinlock_obtain(&(dmar_unit->lock));

	/* Set number of bits needed to represent the entries minus 1 */
	size = (uint8_t) fls32(CONFIG_MAX_IR_ENTRIES) - 1U;
	address = dmar_unit->ir_table_addr | DMAR_IR_ENABLE_EIM | size;

	iommu_write64(dmar_unit, DMAR_IRTA_REG, address);

	iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd | DMA_GCMD_SIRTP);

	dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_IRTPS, 0U, &status);

	spinlock_release(&(dmar_unit->lock));
}

static void dmar_invalid_iec(struct dmar_drhd_rt *dmar_unit, uint16_t intr_index,
				uint8_t index_mask, bool is_global)
{
	struct dmar_entry invalidate_desc;

	invalidate_desc.hi_64 = 0UL;
	invalidate_desc.lo_64 = DMAR_INV_IEC_DESC;

	if (is_global) {
		invalidate_desc.lo_64 |= DMAR_IEC_GLOBAL_INVL;
	} else {
		invalidate_desc.lo_64 |= DMAR_IECI_INDEXED | dma_iec_index(intr_index, index_mask);
	}

	if (invalidate_desc.lo_64 != 0UL) {
		dmar_issue_qi_request(dmar_unit, invalidate_desc);
	}
}

static void dmar_invalid_iec_global(struct dmar_drhd_rt *dmar_unit)
{
	dmar_invalid_iec(dmar_unit, 0U, 0U, true);
}

/* @pre dmar_unit->root_table_addr != NULL */
static void dmar_set_root_table(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status;

	spinlock_obtain(&(dmar_unit->lock));
	iommu_write64(dmar_unit, DMAR_RTADDR_REG, dmar_unit->root_table_addr);

	iommu_write32(dmar_unit, DMAR_GCMD_REG, dmar_unit->gcmd | DMA_GCMD_SRTP);

	/* 32-bit register */
	dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_RTPS, 0U, &status);
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
	union pci_bdf dmar_bdf;

	if (!dma_frcd_up_f(high)) {
		dmar_bdf.value = dma_frcd_up_sid(high);
		/* currently skip PASID related parsing */
		pr_info("%s, Reason: 0x%x, SID: %x.%x.%x @0x%lx",
			(dma_frcd_up_t(high) != 0U) ? "Read/Atomic" : "Write", dma_frcd_up_fr(high),
			dmar_bdf.bits.b, dmar_bdf.bits.d, dmar_bdf.bits.f, low);
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
	struct dmar_entry fault_record;
	int32_t loop = 0;

	dev_dbg(DBG_LEVEL_IOMMU, "%s: irq = %d", __func__, irq);

	fsr = iommu_read32(dmar_unit, DMAR_FSTS_REG);

#if DBG_IOMMU
	fault_status_analysis(fsr);
#endif

	while (dma_fsts_ppf(fsr)) {
		loop++;
		index = dma_fsts_fri(fsr);
		record_reg_offset = (uint32_t)dmar_unit->cap_fault_reg_offset + (index * 16U);
		if (index >= dmar_unit->cap_num_fault_regs) {
			dev_dbg(DBG_LEVEL_IOMMU, "%s: invalid FR Index", __func__);
			break;
		}

		/* read 128-bit fault recording register */
		fault_record.lo_64 = iommu_read64(dmar_unit, record_reg_offset);
		fault_record.hi_64 = iommu_read64(dmar_unit, record_reg_offset + 8U);

		dev_dbg(DBG_LEVEL_IOMMU, "%s: record[%d] @0x%x:  0x%lx, 0x%lx",
			__func__, index, record_reg_offset, fault_record.lo_64, fault_record.hi_64);

		fault_record_analysis(fault_record.lo_64, fault_record.hi_64);

		/* write to clear */
		iommu_write64(dmar_unit, record_reg_offset, fault_record.lo_64);
		iommu_write64(dmar_unit, record_reg_offset + 8U, fault_record.hi_64);

#ifdef DMAR_FAULT_LOOP_MAX
		if (loop > DMAR_FAULT_LOOP_MAX) {
			dev_dbg(DBG_LEVEL_IOMMU, "%s: loop more than %d times", __func__, DMAR_FAULT_LOOP_MAX);
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
	if (retval < 0) {
		panic("dmar[%d] fail to setup interrupt", dmar_unit->index);
	}

	vector = irq_to_vector(dmar_unit->dmar_irq);
	dev_dbg(DBG_LEVEL_IOMMU, "irq#%d vector#%d for dmar_unit", dmar_unit->dmar_irq, vector);

	dmar_fault_msi_write(dmar_unit, vector);
	dmar_fault_event_unmask(dmar_unit);
}

static void dmar_enable_qi(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status = 0;

	spinlock_obtain(&(dmar_unit->lock));

	dmar_unit->qi_queue = hva2hpa(get_qi_queue(dmar_unit->index));
	iommu_write64(dmar_unit, DMAR_IQA_REG, dmar_unit->qi_queue);

	iommu_write32(dmar_unit, DMAR_IQT_REG, 0U);

	if ((dmar_unit->gcmd & DMA_GCMD_QIE) == 0U) {
		dmar_unit->gcmd |= DMA_GCMD_QIE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG,	dmar_unit->gcmd);
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_QIES, 0U, &status);
	}

	spinlock_release(&(dmar_unit->lock));
}

static void dmar_disable_qi(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t status = 0;

	spinlock_obtain(&(dmar_unit->lock));

	if ((dmar_unit->gcmd & DMA_GCMD_QIE) == DMA_GCMD_QIE) {
		dmar_unit->gcmd &= ~DMA_GCMD_QIE;
		iommu_write32(dmar_unit, DMAR_GCMD_REG,	dmar_unit->gcmd);
		dmar_wait_completion(dmar_unit, DMAR_GSTS_REG, DMA_GSTS_QIES, DMA_GSTS_QIES, &status);
	}

	spinlock_release(&(dmar_unit->lock));
}

static void prepare_dmar(struct dmar_drhd_rt *dmar_unit)
{
	dev_dbg(DBG_LEVEL_IOMMU, "enable dmar uint [0x%x]", dmar_unit->drhd->reg_base_addr);
	dmar_setup_interrupt(dmar_unit);
	dmar_set_root_table(dmar_unit);
	dmar_enable_qi(dmar_unit);
	dmar_set_intr_remap_table(dmar_unit);
}

static void enable_dmar(struct dmar_drhd_rt *dmar_unit)
{
	dev_dbg(DBG_LEVEL_IOMMU, "enable dmar uint [0x%x]", dmar_unit->drhd->reg_base_addr);
	dmar_invalid_context_cache_global(dmar_unit);
	dmar_invalid_iotlb_global(dmar_unit);
	dmar_invalid_iec_global(dmar_unit);
	dmar_enable_translation(dmar_unit);
}

static void disable_dmar(struct dmar_drhd_rt *dmar_unit)
{
	dmar_disable_qi(dmar_unit);
	dmar_disable_translation(dmar_unit);
	dmar_fault_event_mask(dmar_unit);
	dmar_disable_intr_remapping(dmar_unit);
}

static void suspend_dmar(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t i;

	dmar_invalid_context_cache_global(dmar_unit);
	dmar_invalid_iotlb_global(dmar_unit);
	dmar_invalid_iec_global(dmar_unit);

	disable_dmar(dmar_unit);

	/* save IOMMU fault register state */
	for (i = 0U; i < IOMMU_FAULT_REGISTER_STATE_NUM; i++) {
		dmar_unit->fault_state[i] =  iommu_read32(dmar_unit, DMAR_FECTL_REG + (i * IOMMU_FAULT_REGISTER_SIZE));
	}
}

static void resume_dmar(struct dmar_drhd_rt *dmar_unit)
{
	uint32_t i;

	/* restore IOMMU fault register state */
	for (i = 0U; i < IOMMU_FAULT_REGISTER_STATE_NUM; i++) {
		iommu_write32(dmar_unit, DMAR_FECTL_REG + (i * IOMMU_FAULT_REGISTER_SIZE), dmar_unit->fault_state[i]);
	}
	prepare_dmar(dmar_unit);
	enable_dmar(dmar_unit);
	dmar_enable_intr_remapping(dmar_unit);
}

static inline bool is_dmar_unit_ignored(const struct dmar_drhd_rt *dmar_unit)
{
	bool ignored = false;

	if ((dmar_unit != NULL) && (dmar_unit->drhd->ignore)) {
		ignored = true;
	}

	return ignored;
}

static bool is_dmar_unit_valid(const struct dmar_drhd_rt *dmar_unit, union pci_bdf sid)
{
	bool valid = false;

	if (dmar_unit == NULL) {
		pr_err("no dmar unit found for device: %x:%x.%x", sid.bits.b, sid.bits.d, sid.bits.f);
	} else if (dmar_unit->drhd->ignore) {
		dev_dbg(DBG_LEVEL_IOMMU, "device is ignored : %x:%x.%x", sid.bits.b, sid.bits.d, sid.bits.f);
	} else {
		valid = true;
	}

	return valid;
}

/* @pre bus < CONFIG_IOMMU_BUS_NUM */
static int32_t iommu_attach_device(const struct iommu_domain *domain, uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_unit;
	struct dmar_entry *root_table;
	uint64_t context_table_addr;
	struct dmar_entry *context;
	struct dmar_entry *root_entry;
	struct dmar_entry *context_entry;
	uint64_t hi_64 = 0UL;
	uint64_t lo_64 = 0UL;
	int32_t ret = -EINVAL;
	/* source id */
	union pci_bdf sid;

	sid.fields.bus = bus;
	sid.fields.devfun = devfun;

	dmar_unit = device_to_dmaru(bus, devfun);
	if (is_dmar_unit_valid(dmar_unit, sid) && dmar_unit_support_aw(dmar_unit, domain->addr_width)) {
		root_table = (struct dmar_entry *)hpa2hva(dmar_unit->root_table_addr);
		root_entry = root_table + bus;

		if (dmar_get_bitslice(root_entry->lo_64,
					ROOT_ENTRY_LOWER_PRESENT_MASK,
					ROOT_ENTRY_LOWER_PRESENT_POS) == 0UL) {
			/* create context table for the bus if not present */
			context_table_addr = hva2hpa(get_ctx_table(dmar_unit->index, bus));

			context_table_addr = context_table_addr >> PAGE_SHIFT;

			lo_64 = dmar_set_bitslice(lo_64,
					ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS, context_table_addr);
			lo_64 = dmar_set_bitslice(lo_64,
					ROOT_ENTRY_LOWER_PRESENT_MASK, ROOT_ENTRY_LOWER_PRESENT_POS, 1UL);

			root_entry->hi_64 = 0UL;
			root_entry->lo_64 = lo_64;
			iommu_flush_cache(root_entry, sizeof(struct dmar_entry));
		} else {
			context_table_addr = dmar_get_bitslice(root_entry->lo_64,
					ROOT_ENTRY_LOWER_CTP_MASK, ROOT_ENTRY_LOWER_CTP_POS);
		}

		context_table_addr = context_table_addr << PAGE_SHIFT;

		context = (struct dmar_entry *)hpa2hva(context_table_addr);
		context_entry = context + devfun;

		if (dmar_get_bitslice(context_entry->lo_64, CTX_ENTRY_LOWER_P_MASK, CTX_ENTRY_LOWER_P_POS) != 0UL) {
			/* the context entry should not be present */
			pr_err("%s: context entry@0x%lx (Lower:%x) ", __func__, context_entry, context_entry->lo_64);
			pr_err("already present for %x:%x.%x", bus, sid.bits.d, sid.bits.f);
			ret = -EBUSY;
		} else {
			/* setup context entry for the devfun */
			/* TODO: add Device TLB support */
			hi_64 = dmar_set_bitslice(hi_64, CTX_ENTRY_UPPER_AW_MASK, CTX_ENTRY_UPPER_AW_POS,
					(uint64_t)width_to_agaw(domain->addr_width));
			lo_64 = dmar_set_bitslice(lo_64, CTX_ENTRY_LOWER_TT_MASK, CTX_ENTRY_LOWER_TT_POS,
					DMAR_CTX_TT_UNTRANSLATED);
			hi_64 = dmar_set_bitslice(hi_64, CTX_ENTRY_UPPER_DID_MASK, CTX_ENTRY_UPPER_DID_POS,
				(uint64_t)vmid_to_domainid(domain->vm_id));
			lo_64 = dmar_set_bitslice(lo_64, CTX_ENTRY_LOWER_SLPTPTR_MASK, CTX_ENTRY_LOWER_SLPTPTR_POS,
				domain->trans_table_ptr >> PAGE_SHIFT);
			lo_64 = dmar_set_bitslice(lo_64, CTX_ENTRY_LOWER_P_MASK, CTX_ENTRY_LOWER_P_POS, 1UL);

			context_entry->hi_64 = hi_64;
			context_entry->lo_64 = lo_64;
			iommu_flush_cache(context_entry, sizeof(struct dmar_entry));
			ret = 0;
		}
	} else if (is_dmar_unit_ignored(dmar_unit)) {
	       ret = 0;
	}

	return ret;
}

/* @pre bus < CONFIG_IOMMU_BUS_NUM */
static int32_t iommu_detach_device(const struct iommu_domain *domain, uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_unit;
	struct dmar_entry *root_table;
	uint64_t context_table_addr;
	struct dmar_entry *context;
	struct dmar_entry *root_entry;
	struct dmar_entry *context_entry;
	/* source id */
	union pci_bdf sid;
	int32_t ret = -EINVAL;

	dmar_unit = device_to_dmaru(bus, devfun);

	sid.fields.bus = bus;
	sid.fields.devfun = devfun;

	if (is_dmar_unit_valid(dmar_unit, sid)) {
		root_table = (struct dmar_entry *)hpa2hva(dmar_unit->root_table_addr);
		root_entry = root_table + bus;
		ret = 0;

		context_table_addr = dmar_get_bitslice(root_entry->lo_64,  ROOT_ENTRY_LOWER_CTP_MASK,
							ROOT_ENTRY_LOWER_CTP_POS);
		context_table_addr = context_table_addr << PAGE_SHIFT;
		context = (struct dmar_entry *)hpa2hva(context_table_addr);

		context_entry = context + devfun;

		if ((context == NULL) || (context_entry == NULL)) {
			pr_err("dmar context entry is invalid");
			ret = -EINVAL;
		} else if ((uint16_t)dmar_get_bitslice(context_entry->hi_64, CTX_ENTRY_UPPER_DID_MASK,
						CTX_ENTRY_UPPER_DID_POS) != vmid_to_domainid(domain->vm_id)) {
			pr_err("%s: domain id mismatch", __func__);
			ret = -EPERM;
		} else {
			/* clear the present bit first */
			context_entry->lo_64 = 0UL;
			context_entry->hi_64 = 0UL;
			iommu_flush_cache(context_entry, sizeof(struct dmar_entry));

			dmar_invalid_context_cache(dmar_unit, vmid_to_domainid(domain->vm_id), sid.value, 0U,
							DMAR_CIRG_DEVICE);
			dmar_invalid_iotlb(dmar_unit, vmid_to_domainid(domain->vm_id), 0UL, 0U, false,
							DMAR_IIRG_DOMAIN);
		}
	} else if (is_dmar_unit_ignored(dmar_unit)) {
	       ret = 0;
	}

	return ret;
}

/*
 * @pre action != NULL
 * As an internal API, VT-d code can guarantee action is not NULL.
 */
static void do_action_for_iommus(void (*action)(struct dmar_drhd_rt *))
{
	struct dmar_drhd_rt *dmar_unit;
	uint32_t i;

	for (i = 0U; i < platform_dmar_info->drhd_count; i++) {
		dmar_unit = &dmar_drhd_units[i];
		if (!dmar_unit->drhd->ignore) {
			action(dmar_unit);
		} else {
			dev_dbg(DBG_LEVEL_IOMMU, "ignore dmar_unit @0x%x", dmar_unit->drhd->reg_base_addr);
		}
	}
}

struct iommu_domain *create_iommu_domain(uint16_t vm_id, uint64_t translation_table, uint32_t addr_width)
{
	static struct iommu_domain iommu_domains[MAX_DOMAIN_NUM];
	struct iommu_domain *domain;

	/* TODO: check if a domain with the vm_id exists */

	if (translation_table == 0UL) {
		pr_err("translation table is NULL");
		domain = NULL;
	} else {
		/*
		 * A hypercall is called to create an iommu domain for a valid VM,
		 * and hv code limit the VM number to CONFIG_MAX_VM_NUM.
		 * So the array iommu_domains will not be accessed out of range.
		 */
		domain = &iommu_domains[vmid_to_domainid(vm_id)];

		domain->vm_id = vm_id;
		domain->trans_table_ptr = translation_table;
		domain->addr_width = addr_width;

		dev_dbg(DBG_LEVEL_IOMMU, "create domain [%d]: vm_id = %hu, ept@0x%x",
			vmid_to_domainid(domain->vm_id), domain->vm_id, domain->trans_table_ptr);
	}

	return domain;
}

/**
 * @pre domain != NULL
 */
void destroy_iommu_domain(struct iommu_domain *domain)
{
	/* TODO: check if any device assigned to this domain */
	(void)memset(domain, 0U, sizeof(*domain));
}

/*
 * @pre (from_domain != NULL) || (to_domain != NULL)
 */

int32_t move_pt_device(const struct iommu_domain *from_domain, const struct iommu_domain *to_domain, uint8_t bus, uint8_t devfun)
{
	int32_t status = 0;
	uint16_t bus_local = bus;

	/* TODO: check if the device assigned */

	if (bus_local < CONFIG_IOMMU_BUS_NUM) {
		if (from_domain != NULL) {
			status = iommu_detach_device(from_domain, bus, devfun);
		}

		if ((status == 0) && (to_domain != NULL)) {
			status = iommu_attach_device(to_domain, bus, devfun);
		}
	} else {
		status = -EINVAL;
	}

	return status;
}

void enable_iommu(void)
{
	do_action_for_iommus(enable_dmar);
}

void suspend_iommu(void)
{
	do_action_for_iommus(suspend_dmar);
}

void resume_iommu(void)
{
	do_action_for_iommus(resume_dmar);
}

/**
 * @post return != NULL
 * @post return->drhd_count > 0U
 */
static struct dmar_info *get_dmar_info(void)
{
#ifdef CONFIG_ACPI_PARSE_ENABLED
	parse_dmar_table(&plat_dmar_info);
#endif
	return &plat_dmar_info;
}

int32_t init_iommu(void)
{
	int32_t ret = 0;

	platform_dmar_info = get_dmar_info();

	if ((platform_dmar_info == NULL) || (platform_dmar_info->drhd_count == 0U)) {
		pr_fatal("%s: can't find dmar info\n", __func__);
		ret = -ENODEV;
	} else if (platform_dmar_info->drhd_count > CONFIG_MAX_IOMMU_NUM) {
		pr_fatal("%s: dmar count(%d) beyond the limitation(%d)\n",
				__func__, platform_dmar_info->drhd_count, CONFIG_MAX_IOMMU_NUM);
		ret = -EINVAL;
	} else {
		ret = register_hrhd_units();
		if (ret == 0) {
			do_action_for_iommus(prepare_dmar);
		}
	}
	return ret;
}

/* Allocate continuous IRTEs specified by num, num can be 1, 2, 4, 8, 16, 32 */
static uint16_t alloc_irtes(struct dmar_drhd_rt *dmar_unit, const uint16_t num)
{
	uint16_t irte_idx;
	uint64_t mask = (1UL << num) - 1U;
	uint64_t test_mask;

	ASSERT((bitmap_weight(num) == 1U) && (num <= 32U));

	spinlock_obtain(&dmar_unit->lock);
	for (irte_idx = 0U; irte_idx < CONFIG_MAX_IR_ENTRIES; irte_idx += num) {
		test_mask = mask << (irte_idx & 0x3FU);
		if ((dmar_unit->irte_alloc_bitmap[irte_idx >> 6U] & test_mask) == 0UL) {
			dmar_unit->irte_alloc_bitmap[irte_idx >> 6U] |= test_mask;
			break;
		}
	}
	spinlock_release(&dmar_unit->lock);

	return (irte_idx < CONFIG_MAX_IR_ENTRIES) ? irte_idx: INVALID_IRTE_ID;
}

static bool is_irte_reserved(const struct dmar_drhd_rt *dmar_unit, uint16_t index)
{
	return ((dmar_unit->irte_reserved_bitmap[index >> 6U] & (1UL << (index & 0x3FU))) != 0UL);
}

int32_t dmar_reserve_irte(const struct intr_source *intr_src, uint16_t num, uint16_t *start_id)
{
	struct dmar_drhd_rt *dmar_unit;
	union pci_bdf sid;
	uint64_t mask = (1UL << num) - 1U;
	int32_t ret = -EINVAL;

	if (intr_src->is_msi) {
		dmar_unit = device_to_dmaru((uint8_t)intr_src->src.msi.bits.b, intr_src->src.msi.fields.devfun);
		sid.value = (uint16_t)(intr_src->src.msi.value);
	} else {
		dmar_unit = ioapic_to_dmaru(intr_src->src.ioapic_id, &sid);
	}

	if (is_dmar_unit_valid(dmar_unit, sid)) {
		*start_id = alloc_irtes(dmar_unit, num);
		if (*start_id < CONFIG_MAX_IR_ENTRIES) {
			dmar_unit->irte_reserved_bitmap[*start_id >> 6U] |= mask << (*start_id & 0x3FU);
		}
		ret = 0;
	}

	pr_dbg("%s: for dev 0x%x:%x.%x, reserve %u entry for MSI(%d), start from %d",
		__func__, sid.bits.b, sid.bits.d, sid.bits.f, num, intr_src->is_msi, *start_id);
	return ret;
}

int32_t dmar_assign_irte(const struct intr_source *intr_src, union dmar_ir_entry *irte,
	uint16_t idx_in, uint16_t *idx_out)
{
	struct dmar_drhd_rt *dmar_unit;
	union dmar_ir_entry *ir_table, *ir_entry;
	union pci_bdf sid;
	uint64_t trigger_mode;
	int32_t ret = -EINVAL;

	if (intr_src->is_msi) {
		dmar_unit = device_to_dmaru((uint8_t)intr_src->src.msi.bits.b, intr_src->src.msi.fields.devfun);
		sid.value = (uint16_t)(intr_src->src.msi.value);
		trigger_mode = 0x0UL;
	} else {
		dmar_unit = ioapic_to_dmaru(intr_src->src.ioapic_id, &sid);
		trigger_mode = irte->bits.remap.trigger_mode;
	}

	if (is_dmar_unit_valid(dmar_unit, sid)) {
		dmar_enable_intr_remapping(dmar_unit);

		ir_table = (union dmar_ir_entry *)hpa2hva(dmar_unit->ir_table_addr);
		*idx_out = idx_in;
		if (idx_in == INVALID_IRTE_ID) {
			*idx_out = alloc_irtes(dmar_unit, 1U);
		}
		if (*idx_out < CONFIG_MAX_IR_ENTRIES) {
			ir_entry = ir_table + *idx_out;

			if (intr_src->pid_paddr != 0UL) {
				union dmar_ir_entry irte_pi;

				/* irte is in remapped mode format, convert to posted mode format */
				irte_pi.value.lo_64 = 0UL;
				irte_pi.value.hi_64 = 0UL;

				irte_pi.bits.post.vector = irte->bits.remap.vector;

				irte_pi.bits.post.svt = 0x1UL;
				irte_pi.bits.post.sid = sid.value;
				irte_pi.bits.post.present = 0x1UL;
				irte_pi.bits.post.mode = 0x1UL;

				irte_pi.bits.post.pda_l = (intr_src->pid_paddr) >> 6U;
				irte_pi.bits.post.pda_h = (intr_src->pid_paddr) >> 32U;

				*ir_entry = irte_pi;
			} else {
				/* Fields that have not been initialized explicitly default to 0 */
				irte->bits.remap.svt = 0x1UL;
				irte->bits.remap.sid = sid.value;
				irte->bits.remap.present = 0x1UL;
				irte->bits.remap.trigger_mode = trigger_mode;

				*ir_entry = *irte;
			}
			iommu_flush_cache(ir_entry, sizeof(union dmar_ir_entry));
			dmar_invalid_iec(dmar_unit, *idx_out, 0U, false);
		}
		ret = 0;
	}

	return ret;
}

void dmar_free_irte(const struct intr_source *intr_src, uint16_t index)
{
	struct dmar_drhd_rt *dmar_unit;
	union dmar_ir_entry *ir_table, *ir_entry;
	union pci_bdf sid;

	if (intr_src->is_msi) {
		dmar_unit = device_to_dmaru((uint8_t)intr_src->src.msi.bits.b, intr_src->src.msi.fields.devfun);
	} else {
		dmar_unit = ioapic_to_dmaru(intr_src->src.ioapic_id, &sid);
	}

	if (is_dmar_unit_valid(dmar_unit, sid) && (index < CONFIG_MAX_IR_ENTRIES)) {
		ir_table = (union dmar_ir_entry *)hpa2hva(dmar_unit->ir_table_addr);
		ir_entry = ir_table + index;
		ir_entry->bits.remap.present = 0x0UL;

		iommu_flush_cache(ir_entry, sizeof(union dmar_ir_entry));
		dmar_invalid_iec(dmar_unit, index, 0U, false);

		if (!is_irte_reserved(dmar_unit, index)) {
			spinlock_obtain(&dmar_unit->lock);
			bitmap_clear_nolock(index & 0x3FU, &dmar_unit->irte_alloc_bitmap[index >> 6U]);
			spinlock_release(&dmar_unit->lock);
		}
	}

}
