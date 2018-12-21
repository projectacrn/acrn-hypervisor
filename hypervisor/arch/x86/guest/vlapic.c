/*-
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2017 Intel Corporation
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

#define pr_prefix		"vlapic: "

#include <hypervisor.h>

#include "instr_emul.h"

#include "vlapic_priv.h"
#include "vlapic.h"

#define VLAPIC_VERBOS 0

static inline uint32_t prio(uint32_t x)
{
	return (x >> 4U);
}

#define VLAPIC_VERSION		(16U)

#define	APICBASE_BSP		0x00000100UL
#define	APICBASE_X2APIC		0x00000400U
#define	APICBASE_ENABLED	0x00000800UL
#define LOGICAL_ID_MASK		0xFU
#define CLUSTER_ID_MASK		0xFFFF0U

#define ACRN_DBG_LAPIC		6U

#if VLAPIC_VERBOS
static inline void vlapic_dump_irr(const struct acrn_vlapic *vlapic, const char *msg)
{
	const struct lapic_reg *irrptr = &(vlapic->apic_page.irr[0]);

	for (uint8_t i = 0U; i < 8U; i++) {
		dev_dbg(ACRN_DBG_LAPIC, "%s irr%u 0x%08x", msg, i, irrptr[i].v);
	}
}

static inline void vlapic_dump_isr(const struct acrn_vlapic *vlapic, const char *msg)
{
	const struct lapic_reg *isrptr = &(vlapic->apic_page.isr[0]);

	for (uint8_t i = 0U; i < 8U; i++) {
		dev_dbg(ACRN_DBG_LAPIC, "%s isr%u 0x%08x", msg, i, isrptr[0].v);
	}
}
#else
static inline void vlapic_dump_irr(__unused const struct acrn_vlapic *vlapic, __unused const char *msg) {}

static inline void vlapic_dump_isr(__unused const struct acrn_vlapic *vlapic, __unused const char *msg) {}
#endif

static int32_t
apicv_set_intr_ready(struct acrn_vlapic *vlapic, uint32_t vector);

static int32_t
apicv_pending_intr(const struct acrn_vlapic *vlapic);

static void
apicv_batch_set_tmr(const struct acrn_vlapic *vlapic);

/*
 * Post an interrupt to the vcpu running on 'hostcpu'. This will use a
 * hardware assist if available (e.g. Posted Interrupt) or fall back to
 * sending an 'ipinum' to interrupt the 'hostcpu'.
 */
static void vlapic_set_error(struct acrn_vlapic *vlapic, uint32_t mask);

static void vlapic_timer_expired(void *data);

static inline bool is_x2apic_enabled(const struct acrn_vlapic *vlapic);

static struct acrn_vlapic *
vm_lapic_from_vcpu_id(struct acrn_vm *vm, uint16_t vcpu_id)
{
	struct acrn_vcpu *vcpu;

	vcpu = vcpu_from_vid(vm, vcpu_id);

	return vcpu_vlapic(vcpu);
}

static uint16_t vm_apicid2vcpu_id(struct acrn_vm *vm, uint8_t lapicid)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		struct acrn_vlapic *vlapic = vcpu_vlapic(vcpu);
		if (vlapic_get_apicid(vlapic) == lapicid) {
			return vcpu->vcpu_id;
		}
	}

	pr_err("%s: bad lapicid %hhu", __func__, lapicid);

	return phys_cpu_num;
}

static uint64_t
vm_active_cpus(const struct acrn_vm *vm)
{
	uint64_t dmask = 0UL;
	uint16_t i;
	const struct acrn_vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		bitmap_set_lock(vcpu->vcpu_id, &dmask);
	}

	return dmask;
}

uint32_t
vlapic_get_apicid(struct acrn_vlapic *vlapic)
{
	uint32_t apicid;
	if (is_x2apic_enabled(vlapic)) {
		apicid = vlapic->apic_page.id.v;
	} else {
		apicid = (vlapic->apic_page.id.v) >> APIC_ID_SHIFT;
	}

	return apicid;
}

static inline uint32_t
vlapic_build_id(const struct acrn_vlapic *vlapic)
{
	const struct acrn_vcpu *vcpu = vlapic->vcpu;
	uint32_t vlapic_id, lapic_regs_id;

#ifdef CONFIG_PARTITION_MODE
	/*
	 * Partition mode UOS is forced to use physical mode in xAPIC
	 * Hence ACRN needs to maintain physical APIC ids for partition
	 * mode.
	 */
	vlapic_id = per_cpu(lapic_id, vcpu->pcpu_id);
#else
	if (is_vm0(vcpu->vm)) {
		/* Get APIC ID sequence format from cpu_storage */
		vlapic_id = per_cpu(lapic_id, vcpu->vcpu_id);
	} else {
		vlapic_id = (uint32_t)vcpu->vcpu_id;
	}
#endif

	if (is_x2apic_enabled(vlapic)) {
		lapic_regs_id = vlapic_id;
	} else {
		lapic_regs_id = vlapic_id << APIC_ID_SHIFT;
	}

	dev_dbg(ACRN_DBG_LAPIC, "vlapic APIC PAGE ID : 0x%08x", lapic_regs_id);

	return lapic_regs_id;
}

static inline void vlapic_build_x2apic_id(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;
	uint32_t logical_id, cluster_id;

	lapic = &(vlapic->apic_page);
	lapic->id.v = vlapic_build_id(vlapic);
	logical_id = lapic->id.v & LOGICAL_ID_MASK;
	cluster_id = (lapic->id.v & CLUSTER_ID_MASK) >> 4U;
	lapic->ldr.v = (cluster_id << 16U) | (1U << logical_id);
}

static void
vlapic_dfr_write_handler(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;

	lapic = &(vlapic->apic_page);
	lapic->dfr.v &= APIC_DFR_MODEL_MASK;
	lapic->dfr.v |= APIC_DFR_RESERVED;

	if ((lapic->dfr.v & APIC_DFR_MODEL_MASK) == APIC_DFR_MODEL_FLAT) {
		dev_dbg(ACRN_DBG_LAPIC, "vlapic DFR in Flat Model");
	} else if ((lapic->dfr.v & APIC_DFR_MODEL_MASK)
			== APIC_DFR_MODEL_CLUSTER) {
		dev_dbg(ACRN_DBG_LAPIC, "vlapic DFR in Cluster Model");
	} else {
		dev_dbg(ACRN_DBG_LAPIC, "DFR in Unknown Model %#x", lapic->dfr);
	}
}

static void
vlapic_ldr_write_handler(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;

	lapic = &(vlapic->apic_page);
	lapic->ldr.v &= ~APIC_LDR_RESERVED;
	dev_dbg(ACRN_DBG_LAPIC, "vlapic LDR set to %#x", lapic->ldr);
}

static inline uint32_t
vlapic_timer_divisor_shift(uint32_t dcr)
{
	uint32_t val;

	val = ((dcr & 0x3U) | ((dcr & 0x8U) >> 1U));
	return ((val + 1U) & 0x7U);
}

static inline bool
vlapic_lvtt_oneshot(const struct acrn_vlapic *vlapic)
{
	return (((vlapic->apic_page.lvt[APIC_LVT_TIMER].v) & APIC_LVTT_TM)
				== APIC_LVTT_TM_ONE_SHOT);
}

static inline bool
vlapic_lvtt_period(const struct acrn_vlapic *vlapic)
{
	return (((vlapic->apic_page.lvt[APIC_LVT_TIMER].v) & APIC_LVTT_TM)
				==  APIC_LVTT_TM_PERIODIC);
}

static inline bool
vlapic_lvtt_tsc_deadline(const struct acrn_vlapic *vlapic)
{
	return (((vlapic->apic_page.lvt[APIC_LVT_TIMER].v) & APIC_LVTT_TM)
				==  APIC_LVTT_TM_TSCDLT);
}

static inline bool
vlapic_lvtt_masked(const struct acrn_vlapic *vlapic)
{
	return (((vlapic->apic_page.lvt[APIC_LVT_TIMER].v) & APIC_LVTT_M) != 0U);
}

/**
 * @pre vlapic != NULL
 */
static void vlapic_init_timer(struct acrn_vlapic *vlapic)
{
	struct vlapic_timer *vtimer;

	vtimer = &vlapic->vtimer;
	(void)memset(vtimer, 0U, sizeof(struct vlapic_timer));

	initialize_timer(&vtimer->timer,
			vlapic_timer_expired, vlapic->vcpu,
			0UL, 0, 0UL);
}

/**
 * @pre vlapic != NULL
 */
static void vlapic_reset_timer(struct acrn_vlapic *vlapic)
{
	struct hv_timer *timer;

	timer = &vlapic->vtimer.timer;
	del_timer(timer);
	timer->mode = TICK_MODE_ONESHOT;
	timer->fire_tsc = 0UL;
	timer->period_in_cycle = 0UL;
}

static bool
set_expiration(struct acrn_vlapic *vlapic)
{
	uint64_t now = rdtsc();
	uint64_t delta;
	struct vlapic_timer *vtimer;
	struct hv_timer *timer;
	uint32_t tmicr, divisor_shift;
	bool ret;

	vtimer = &vlapic->vtimer;
	tmicr = vtimer->tmicr;
	divisor_shift = vtimer->divisor_shift;

	if ((tmicr == 0U) || (divisor_shift > 8U)) {
		ret = false;
	} else {
		delta = (uint64_t)tmicr << divisor_shift;
		timer = &vtimer->timer;

		if (vlapic_lvtt_period(vlapic)) {
			timer->period_in_cycle = delta;
		}
		timer->fire_tsc = now + delta;
		ret = true;
	}
	return ret;
}

static void vlapic_update_lvtt(struct acrn_vlapic *vlapic,
			uint32_t val)
{
	uint32_t timer_mode = val & APIC_LVTT_TM;
	struct vlapic_timer *vtimer = &vlapic->vtimer;

	if (vtimer->mode != timer_mode) {
		struct hv_timer *timer = &vtimer->timer;

		/*
		 * A write to the LVT Timer Register that changes
		 * the timer mode disarms the local APIC timer.
		 */
		del_timer(timer);
		timer->mode = (timer_mode == APIC_LVTT_TM_PERIODIC) ?
				TICK_MODE_PERIODIC: TICK_MODE_ONESHOT;
		timer->fire_tsc = 0UL;
		timer->period_in_cycle = 0UL;

		vtimer->mode = timer_mode;
	}
}

static uint32_t vlapic_get_ccr(const struct acrn_vlapic *vlapic)
{
	uint64_t now = rdtsc();
	uint32_t remain_count = 0U;
	const struct vlapic_timer *vtimer;

	vtimer = &vlapic->vtimer;

	if ((vtimer->tmicr != 0U) && !vlapic_lvtt_tsc_deadline(vlapic)) {
		uint64_t fire_tsc = vtimer->timer.fire_tsc;

		if (now < fire_tsc) {
			uint32_t divisor_shift = vtimer->divisor_shift;
			uint64_t shifted_delta =
				(fire_tsc - now) >> divisor_shift;
			remain_count = (uint32_t)shifted_delta;
		}
	}

	return remain_count;
}

static void vlapic_dcr_write_handler(struct acrn_vlapic *vlapic)
{
	uint32_t divisor_shift;
	struct vlapic_timer *vtimer;
	struct lapic_regs *lapic = &(vlapic->apic_page);

	vtimer = &vlapic->vtimer;
	divisor_shift = vlapic_timer_divisor_shift(lapic->dcr_timer.v);

	vtimer->divisor_shift = divisor_shift;
}

static void vlapic_icrtmr_write_handler(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;
	struct vlapic_timer *vtimer;

	if (!vlapic_lvtt_tsc_deadline(vlapic)) {
		lapic = &(vlapic->apic_page);
		vtimer = &vlapic->vtimer;
		vtimer->tmicr = lapic->icr_timer.v;

		del_timer(&vtimer->timer);
		if (set_expiration(vlapic)) {
			/* vlapic_init_timer has been called,
			 * and timer->fire_tsc is not 0, here
			 * add_timer should not return error
			 */
			(void)add_timer(&vtimer->timer);
		}
	}
}

static uint64_t vlapic_get_tsc_deadline_msr(const struct acrn_vlapic *vlapic)
{
	uint64_t ret;
	if (!vlapic_lvtt_tsc_deadline(vlapic)) {
		ret = 0UL;
	} else {
		ret = (vlapic->vtimer.timer.fire_tsc == 0UL) ? 0UL :
			vcpu_get_guest_msr(vlapic->vcpu, MSR_IA32_TSC_DEADLINE);
	}

	return ret;

}

static void vlapic_set_tsc_deadline_msr(struct acrn_vlapic *vlapic,
			uint64_t val_arg)
{
	struct hv_timer *timer;
	uint64_t val = val_arg;

	if (vlapic_lvtt_tsc_deadline(vlapic)) {
		vcpu_set_guest_msr(vlapic->vcpu, MSR_IA32_TSC_DEADLINE, val);

		timer = &vlapic->vtimer.timer;
		del_timer(timer);

		if (val != 0UL) {
			/* transfer guest tsc to host tsc */
			val -= exec_vmread64(VMX_TSC_OFFSET_FULL);
			timer->fire_tsc = val;
			/* vlapic_init_timer has been called,
			 * and timer->fire_tsc is not 0,here
			 * add_timer should not return error
			 */
			(void)add_timer(timer);
		} else {
			timer->fire_tsc = 0UL;
		}
	}
}

static void
vlapic_esr_write_handler(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;

	lapic = &(vlapic->apic_page);
	lapic->esr.v = vlapic->esr_pending;
	vlapic->esr_pending = 0U;
}

/*
 * Returns 1 if the vcpu needs to be notified of the interrupt and 0 otherwise.
 * @pre vector >= 16
 */
static int32_t
vlapic_set_intr_ready(struct acrn_vlapic *vlapic, uint32_t vector, bool level)
{
	struct lapic_regs *lapic;
	struct lapic_reg *irrptr, *tmrptr;
	uint32_t mask;
	uint32_t idx;
	int32_t pending_intr;

	ASSERT(vector <= NR_MAX_VECTOR,
		"invalid vector %u", vector);

	lapic = &(vlapic->apic_page);
	if ((lapic->svr.v & APIC_SVR_ENABLE) == 0U) {
		dev_dbg(ACRN_DBG_LAPIC,
			"vlapic is software disabled, ignoring interrupt %u",
			vector);
		return 0;
	}

	if (is_apicv_intr_delivery_supported()) {
		pending_intr = apicv_set_intr_ready(vlapic, vector);
		if ((pending_intr != 0)
			&& is_apicv_posted_intr_supported()
			&& (get_cpu_id() != vlapic->vcpu->pcpu_id)) {
			/*
			 * Send interrupt to vCPU via posted interrupt way:
			 * 1. If target vCPU is in non-root mode(running),
			 *    send PI notification to vCPU and hardware will
			 *    sync PIR to vIRR automatically.
			 * 2. If target vCPU is in root mode(isn't running),
			 *    record this request as ACRN_REQUEST_EVENT,then
			 *    will pick up the interrupt from PIR and inject
			 *    it to vCPU in next vmentry.
			 */
			bitmap_set_lock(ACRN_REQUEST_EVENT,
				&vlapic->vcpu->arch.pending_req);
			vlapic_post_intr(vlapic->vcpu->pcpu_id);
			return 0;
		}
		return pending_intr;
	}

	idx = vector >> 5U;
	mask = 1U << (vector & 0x1fU);

	irrptr = &lapic->irr[0];
	/* If the interrupt is set, don't try to do it again */
	if (bitmap32_test_and_set_lock((uint16_t)(vector & 0x1fU),
							&irrptr[idx].v)) {
		return 0;
	}

	/*
	 * Verify that the trigger-mode of the interrupt matches with
	 * the vlapic TMR registers.
	 */
	tmrptr = &lapic->tmr[0];
	if ((tmrptr[idx].v & mask) != (level ? mask : 0U)) {
		dev_dbg(ACRN_DBG_LAPIC,
		"vlapic TMR[%u] is 0x%08x but interrupt is %s-triggered",
		idx, tmrptr[idx].v, level ? "level" : "edge");
	}

	vlapic_dump_irr(vlapic, "vlapic_set_intr_ready");
	return 1;
}

/**
 * @brief Send notification vector to target pCPU.
 *
 * If APICv Posted-Interrupt is enabled and target pCPU is in non-root mode,
 * pCPU will sync pending virtual interrupts from PIR to vIRR automatically,
 * without VM exit.
 * If pCPU in root-mode, virtual interrupt will be injected in next VM entry.
 *
 * @param[in] dest_pcpu_id Target CPU ID.
 *
 * @return None
 */
void vlapic_post_intr(uint16_t dest_pcpu_id)
{
	send_single_ipi(dest_pcpu_id, VECTOR_POSTED_INTR);
}

/**
 * @brief Get physical address to PIR description.
 *
 * If APICv Posted-interrupt is supported, this address will be configured
 * to VMCS "Posted-interrupt descriptor address" field.
 *
 * @param[in] vcpu Target vCPU
 *
 * @return physicall address to PIR
 *
 * @pre vcpu != NULL
 */
uint64_t apicv_get_pir_desc_paddr(struct acrn_vcpu *vcpu)
{
	struct acrn_vlapic *vlapic;

	vlapic = &vcpu->arch.vlapic;
	return hva2hpa(&(vlapic->pir_desc));
}

/**
 * @pre offset value shall be one of the folllowing values:
 *	APIC_OFFSET_CMCI_LVT
 *	APIC_OFFSET_TIMER_LVT
 *	APIC_OFFSET_THERM_LVT
 *	APIC_OFFSET_PERF_LVT
 *	APIC_OFFSET_LINT0_LVT
 *	APIC_OFFSET_LINT1_LVT
 *	APIC_OFFSET_ERROR_LVT
 */
static inline uint32_t
lvt_off_to_idx(uint32_t offset)
{
	uint32_t index;

	switch (offset) {
	case APIC_OFFSET_CMCI_LVT:
		index = APIC_LVT_CMCI;
		break;
	case APIC_OFFSET_TIMER_LVT:
		index = APIC_LVT_TIMER;
		break;
	case APIC_OFFSET_THERM_LVT:
		index = APIC_LVT_THERMAL;
		break;
	case APIC_OFFSET_PERF_LVT:
		index = APIC_LVT_PMC;
		break;
	case APIC_OFFSET_LINT0_LVT:
		index = APIC_LVT_LINT0;
		break;
	case APIC_OFFSET_LINT1_LVT:
		index = APIC_LVT_LINT1;
		break;
	case APIC_OFFSET_ERROR_LVT:
	default:
		/*
		 * The function caller could guarantee the pre condition.
		 * So, all of the possible 'offset' other than
		 * APIC_OFFSET_ERROR_LVT has been handled in prior cases.
		 */
		index = APIC_LVT_ERROR;
		break;
	}

	return index;
}

/**
 * @pre offset value shall be one of the folllowing values:
 *	APIC_OFFSET_CMCI_LVT
 *	APIC_OFFSET_TIMER_LVT
 *	APIC_OFFSET_THERM_LVT
 *	APIC_OFFSET_PERF_LVT
 *	APIC_OFFSET_LINT0_LVT
 *	APIC_OFFSET_LINT1_LVT
 *	APIC_OFFSET_ERROR_LVT
 */
static inline uint32_t *
vlapic_get_lvtptr(struct acrn_vlapic *vlapic, uint32_t offset)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);
	uint32_t i;

	switch (offset) {
	case APIC_OFFSET_CMCI_LVT:
		return &lapic->lvt_cmci.v;
	default:
		/*
		 * The function caller could guarantee the pre condition.
		 * All the possible 'offset' other than APIC_OFFSET_CMCI_LVT
		 * could be handled here.
		 */
		i = lvt_off_to_idx(offset);
		return &(lapic->lvt[i].v);
	}
}

static inline uint32_t
vlapic_get_lvt(const struct acrn_vlapic *vlapic, uint32_t offset)
{
	uint32_t idx, val;

	idx = lvt_off_to_idx(offset);
	val = atomic_load32(&vlapic->lvt_last[idx]);
	return val;
}

static void
vlapic_lvt_write_handler(struct acrn_vlapic *vlapic, uint32_t offset)
{
	uint32_t *lvtptr, mask, val, idx;
	struct lapic_regs *lapic;

	lapic = &(vlapic->apic_page);
	lvtptr = vlapic_get_lvtptr(vlapic, offset);
	val = *lvtptr;

	if ((lapic->svr.v & APIC_SVR_ENABLE) == 0U) {
		val |= APIC_LVT_M;
	}
	mask = APIC_LVT_M | APIC_LVT_DS | APIC_LVT_VECTOR;
	switch (offset) {
	case APIC_OFFSET_TIMER_LVT:
		mask |= APIC_LVTT_TM;
		break;
	case APIC_OFFSET_ERROR_LVT:
		break;
	case APIC_OFFSET_LINT0_LVT:
	case APIC_OFFSET_LINT1_LVT:
		mask |= APIC_LVT_TM | APIC_LVT_RIRR | APIC_LVT_IIPP;
		/* FALLTHROUGH */
	default:
		mask |= APIC_LVT_DM;
		break;
	}
	val &= mask;

	/* vlapic mask/unmask LINT0 for ExtINT? */
	if ((offset == APIC_OFFSET_LINT0_LVT) &&
		((val & APIC_LVT_DM) == APIC_LVT_DM_EXTINT)) {
		uint32_t last = vlapic_get_lvt(vlapic, offset);

		/* mask -> unmask: may from every vlapic in the vm */
		if (((last & APIC_LVT_M) != 0U) && ((val & APIC_LVT_M) == 0U)) {
			if ((vlapic->vm->wire_mode == VPIC_WIRE_INTR) ||
				(vlapic->vm->wire_mode == VPIC_WIRE_NULL)) {
				vlapic->vm->wire_mode = VPIC_WIRE_LAPIC;
				dev_dbg(ACRN_DBG_LAPIC,
					"vpic wire mode -> LAPIC");
			} else {
				pr_err("WARNING:invalid vpic wire mode change");
				return;
			}
		/* unmask -> mask: only from the vlapic LINT0-ExtINT enabled */
		} else if (((last & APIC_LVT_M) == 0U) && ((val & APIC_LVT_M) != 0U)) {
			if (vlapic->vm->wire_mode == VPIC_WIRE_LAPIC) {
				vlapic->vm->wire_mode = VPIC_WIRE_NULL;
				dev_dbg(ACRN_DBG_LAPIC,
						"vpic wire mode -> NULL");
			}
		} else {
			/* APIC_LVT_M unchanged. No action required. */
		}
	} else if (offset == APIC_OFFSET_TIMER_LVT) {
		vlapic_update_lvtt(vlapic, val);
	} else {
		/* No action required. */
	}

	*lvtptr = val;
	idx = lvt_off_to_idx(offset);
	atomic_store32(&vlapic->lvt_last[idx], val);
}

static void
vlapic_mask_lvts(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);

	lapic->lvt_cmci.v |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_CMCI_LVT);

	lapic->lvt[APIC_LVT_TIMER].v |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_TIMER_LVT);

	lapic->lvt[APIC_LVT_THERMAL].v |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_THERM_LVT);

	lapic->lvt[APIC_LVT_PMC].v |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_PERF_LVT);

	lapic->lvt[APIC_LVT_LINT0].v |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_LINT0_LVT);

	lapic->lvt[APIC_LVT_LINT1].v |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_LINT1_LVT);

	lapic->lvt[APIC_LVT_ERROR].v |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_ERROR_LVT);
}

/*
 * @pre vec = (lvt & APIC_LVT_VECTOR) >=16
 */
static void
vlapic_fire_lvt(struct acrn_vlapic *vlapic, uint32_t lvt)
{
	uint32_t vec, mode;
	struct acrn_vcpu *vcpu = vlapic->vcpu;

	if ((lvt & APIC_LVT_M) != 0U) {
		return;
	}

	vec = lvt & APIC_LVT_VECTOR;
	mode = lvt & APIC_LVT_DM;

	switch (mode) {
	case APIC_LVT_DM_FIXED:
		if (vlapic_set_intr_ready(vlapic, vec, false) != 0) {
			vcpu_make_request(vcpu, ACRN_REQUEST_EVENT);
		}
		break;
	case APIC_LVT_DM_NMI:
		vcpu_inject_nmi(vcpu);
		break;
	case APIC_LVT_DM_EXTINT:
		vcpu_inject_extint(vcpu);
		break;
	default:
		/* Other modes ignored */
		pr_warn("func:%s other mode is not support\n",__func__);
		return;
	}
	return;
}

static void
dump_isrvec_stk(const struct acrn_vlapic *vlapic)
{
	uint32_t i;
	const struct lapic_reg *isrptr;

	isrptr = &(vlapic->apic_page.isr[0]);
	for (i = 0U; i < 8U; i++) {
		printf("ISR%u 0x%08x\n", i, isrptr[i].v);
	}

	for (i = 0U; i <= vlapic->isrvec_stk_top; i++) {
		printf("isrvec_stk[%u] = %hhu\n", i, vlapic->isrvec_stk[i]);
	}
}

/*
 * Algorithm adopted from section "Interrupt, Task and Processor Priority"
 * in Intel Architecture Manual Vol 3a.
 */
static void
vlapic_update_ppr(struct acrn_vlapic *vlapic)
{
	uint32_t top_isrvec;
	uint32_t tpr, ppr;

	/*
	 * Note that the value on the stack at index 0 is always 0.
	 *
	 * This is a placeholder for the value of ISRV when none of the
	 * bits is set in the ISRx registers.
	 */
	top_isrvec = (uint32_t)vlapic->isrvec_stk[vlapic->isrvec_stk_top];
	tpr = vlapic->apic_page.tpr.v;

	/* update ppr */
	{
		int32_t lastprio, curprio;
		struct lapic_reg *isrptr;
		uint32_t i, idx, vector;
		uint32_t isrvec;

		if ((vlapic->isrvec_stk_top == 0U) && (top_isrvec != 0U)) {
			panic("isrvec_stk is corrupted: %u", top_isrvec);
		}

		/*
		 * Make sure that the priority of the nested interrupts is
		 * always increasing.
		 */
		lastprio = -1;
		for (i = 1U; i <= vlapic->isrvec_stk_top; i++) {
			isrvec = (uint32_t)vlapic->isrvec_stk[i];
			curprio = (int32_t)prio(isrvec);
			if (curprio <= lastprio) {
				dump_isrvec_stk(vlapic);
				panic("isrvec_stk does not satisfy invariant");
			}
			lastprio = curprio;
		}

		/*
		 * Make sure that each bit set in the ISRx registers has a
		 * corresponding entry on the isrvec stack.
		 */
		i = 1U;
		isrptr = &(vlapic->apic_page.isr[0]);
		for (vector = 0U; vector < 256U; vector++) {
			idx = vector >> 5U;
			if ((isrptr[idx].v & (1U << (vector & 0x1fU)))
									!= 0U) {
				isrvec = (uint32_t)vlapic->isrvec_stk[i];
				if ((i > vlapic->isrvec_stk_top) ||
					((i < ISRVEC_STK_SIZE) &&
					(isrvec != vector))) {
					dump_isrvec_stk(vlapic);
					panic("ISR and isrvec_stk out of sync");
				}
				i++;
			}
		}
	}

	if (prio(tpr) >= prio(top_isrvec)) {
		ppr = tpr;
	} else {
		ppr = top_isrvec & 0xf0U;
	}

	vlapic->apic_page.ppr.v = ppr;
	dev_dbg(ACRN_DBG_LAPIC, "%s 0x%02x", __func__, ppr);
}

static void
vlapic_process_eoi(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);
	struct lapic_reg *isrptr, *tmrptr;
	uint32_t i, vector, bitpos;

	isrptr = &lapic->isr[0];
	tmrptr = &lapic->tmr[0];

	/* i ranges effectively from 7 to 0 */
	for (i = 8U; i > 0U; ) {
		i--;
		bitpos = (uint32_t)fls32(isrptr[i].v);
		if (bitpos != INVALID_BIT_INDEX) {
			if (vlapic->isrvec_stk_top == 0U) {
				panic("invalid vlapic isrvec_stk_top %u",
					vlapic->isrvec_stk_top);
			}
			isrptr[i].v &= ~(1U << bitpos);
			vector = (i * 32U) + bitpos;
			dev_dbg(ACRN_DBG_LAPIC, "EOI vector %u", vector);
			vlapic_dump_isr(vlapic, "vlapic_process_eoi");
			vlapic->isrvec_stk_top--;
			vlapic_update_ppr(vlapic);
			if ((tmrptr[i].v & (1U << bitpos)) != 0U) {
				/* hook to vIOAPIC */
				vioapic_process_eoi(vlapic->vm, vector);
			}
			return;
		}
	}

	dev_dbg(ACRN_DBG_LAPIC, "Gratuitous EOI");
}

static void
vlapic_set_error(struct acrn_vlapic *vlapic, uint32_t mask)
{
	uint32_t lvt;

	vlapic->esr_pending |= mask;
	if (vlapic->esr_firing == 0) {
		vlapic->esr_firing = 1;

		/* The error LVT always uses the fixed delivery mode. */
		lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_ERROR_LVT);
		vlapic_fire_lvt(vlapic, lvt | APIC_LVT_DM_FIXED);
		vlapic->esr_firing = 0;
	}
}
/*
 * @pre vector <= 255
 */
static int32_t
vlapic_trigger_lvt(struct acrn_vlapic *vlapic, uint32_t vector)
{
	uint32_t lvt;
	struct acrn_vcpu *vcpu = vlapic->vcpu;

	if (vlapic_enabled(vlapic) == false) {
		/*
		 * When the local APIC is global/hardware disabled,
		 * LINT[1:0] pins are configured as INTR and NMI pins,
		 * respectively.
		 */
		switch (vector) {
		case APIC_LVT_LINT0:
			vcpu_inject_extint(vcpu);
			break;
		case APIC_LVT_LINT1:
			vcpu_inject_nmi(vcpu);
			break;
		default:
			/*
			 * Only LINT[1:0] pins will be handled here.
			 * Gracefully return if prior case clauses have not
			 * been met.
			 */
			break;
		}
		return 0;
	}

	switch (vector) {
	case APIC_LVT_LINT0:
		lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_LINT0_LVT);
		break;
	case APIC_LVT_LINT1:
		lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_LINT1_LVT);
		break;
	case APIC_LVT_TIMER:
		lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_TIMER_LVT);
		lvt |= APIC_LVT_DM_FIXED;
		break;
	case APIC_LVT_ERROR:
		lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_ERROR_LVT);
		lvt |= APIC_LVT_DM_FIXED;
		break;
	case APIC_LVT_PMC:
		lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_PERF_LVT);
		break;
	case APIC_LVT_THERMAL:
		lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_THERM_LVT);
		break;
	case APIC_LVT_CMCI:
		lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_CMCI_LVT);
		break;
	default:
		return -EINVAL;
	}
	if (vector < 16U) {
		vlapic_set_error(vlapic, APIC_ESR_RECEIVE_ILLEGAL_VECTOR);
	} else {
		vlapic_fire_lvt(vlapic, lvt);
	}
	return 0;
}

/*
 * This function populates 'dmask' with the set of vcpus that match the
 * addressing specified by the (dest, phys, lowprio) tuple.
 */
static void
vlapic_calcdest(struct acrn_vm *vm, uint64_t *dmask, uint32_t dest,
		bool phys, bool lowprio)
{
	struct acrn_vlapic *vlapic;
	struct acrn_vlapic *target = NULL;
	uint32_t dfr, ldr, ldest, cluster;
	uint32_t mda_flat_ldest, mda_cluster_ldest, mda_ldest, mda_cluster_id;
	uint64_t amask;
	uint16_t vcpu_id;

	if (dest == 0xffU) {
		/*
		 * Broadcast in both logical and physical modes.
		 */
		*dmask = vm_active_cpus(vm);
		return;
	}

	if (phys) {
		/*
		 * Physical mode: destination is LAPIC ID.
		 */
		*dmask = 0UL;
		vcpu_id = vm_apicid2vcpu_id(vm, (uint8_t)dest);
		if (vcpu_id < vm->hw.created_vcpus) {
			bitmap_set_lock(vcpu_id, dmask);
		}
	} else {
		/*
		 * Logical mode: match each APIC that has a bit set
		 * in its LDR that matches a bit in the ldest.
		 */
		*dmask = 0UL;
		amask = vm_active_cpus(vm);
		for (vcpu_id = 0U; vcpu_id < vm->hw.created_vcpus; vcpu_id++) {
			if ((amask & (1UL << vcpu_id)) != 0UL) {
				vlapic = vm_lapic_from_vcpu_id(vm, vcpu_id);

				if (is_x2apic_enabled(vlapic)){
					ldr = vlapic->apic_page.ldr.v;
					ldest = ldr & 0xFFFFU;

					mda_cluster_id = (dest >> 16U) & 0xFFFFU;
					mda_ldest = dest & 0xFFFFU;
					if (mda_cluster_id != ((ldr >> 16U) & 0xFFFFU)) {
						continue;
					}
				} else {
					/*
					 * In the "Flat Model" the MDA is interpreted as an 8-bit wide
					 * bitmask. This model is only available in the xAPIC mode.
					 */
					mda_flat_ldest = dest & 0xffU;

					/*
					 * In the "Cluster Model" the MDA is used to identify a
					 * specific cluster and a set of APICs in that cluster.
					 */
					mda_cluster_id = (dest >> 4U) & 0xfU;
					mda_cluster_ldest = dest & 0xfU;

					dfr = vlapic->apic_page.dfr.v;
					ldr = vlapic->apic_page.ldr.v;

					if ((dfr & APIC_DFR_MODEL_MASK) ==
							APIC_DFR_MODEL_FLAT) {
						ldest = ldr >> 24U;
						mda_ldest = mda_flat_ldest;
					} else if ((dfr & APIC_DFR_MODEL_MASK) ==
							APIC_DFR_MODEL_CLUSTER) {

						cluster = ldr >> 28U;
						ldest = (ldr >> 24U) & 0xfU;

						if (cluster != mda_cluster_id) {
							continue;
						}
						mda_ldest = mda_cluster_ldest;
					} else {
						/*
						 * Guest has configured a bad logical
						 * model for this vcpu - skip it.
						 */
						dev_dbg(ACRN_DBG_LAPIC,
								"CANNOT deliver interrupt");
						dev_dbg(ACRN_DBG_LAPIC,
								"vlapic has bad logical model %x", dfr);
						continue;
					}
				}
				if ((mda_ldest & ldest) != 0U) {
					if (lowprio) {
						if (target == NULL) {
							target = vlapic;
						} else if (target->apic_page.ppr.v >
								vlapic->apic_page.ppr.v) {
							target = vlapic;
						} else {
							/* target is the dest */
						}
					} else {
						bitmap_set_lock(vcpu_id, dmask);
					}
				}
			}
		}

		if (lowprio && (target != NULL)) {
			bitmap_set_lock(target->vcpu->vcpu_id, dmask);
		}
	}
}

	void
calcvdest(struct acrn_vm *vm, uint64_t *dmask, uint32_t dest, bool phys)
{
	vlapic_calcdest(vm, dmask, dest, phys, false);
}

static void
vlapic_set_tpr(struct acrn_vlapic *vlapic, uint32_t val)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);

	if (lapic->tpr.v != val) {
		dev_dbg(ACRN_DBG_LAPIC,
			"vlapic TPR changed from %#x to %#x", lapic->tpr, val);
		lapic->tpr.v = val;
		vlapic_update_ppr(vlapic);
	}
}

static uint32_t
vlapic_get_tpr(const struct acrn_vlapic *vlapic)
{
	const struct lapic_regs *lapic = &(vlapic->apic_page);

	return lapic->tpr.v;
}

void
vlapic_set_cr8(struct acrn_vlapic *vlapic, uint64_t val)
{
	uint32_t tpr;

	if ((val & ~0xfUL) != 0U) {
		struct acrn_vcpu *vcpu = vlapic->vcpu;
		vcpu_inject_gp(vcpu, 0U);
	} else {
		/* It is safe to narrow val as the higher 60 bits are 0s. */
		tpr = (uint32_t)val << 4U;
		vlapic_set_tpr(vlapic, tpr);
	}
}

uint64_t
vlapic_get_cr8(const struct acrn_vlapic *vlapic)
{
	uint32_t tpr;

	tpr = vlapic_get_tpr(vlapic);
	return ((uint64_t)tpr >> 4UL);
}

static void
vlapic_process_init_sipi(struct acrn_vcpu* target_vcpu, uint32_t mode,
				uint32_t icr_low, uint16_t vcpu_id)
{
	if (mode == APIC_DELMODE_INIT) {
		if ((icr_low & APIC_LEVEL_MASK) == APIC_LEVEL_DEASSERT) {
			return;
		}

		dev_dbg(ACRN_DBG_LAPIC,
				"Sending INIT from VCPU %hu to %hu",
				target_vcpu->vcpu_id, vcpu_id);

		/* put target vcpu to INIT state and wait for SIPI */
		pause_vcpu(target_vcpu, VCPU_PAUSED);
		reset_vcpu(target_vcpu);
		/* new cpu model only need one SIPI to kick AP run,
		 * the second SIPI will be ignored as it move out of
		 * wait-for-SIPI state.
		*/
		target_vcpu->arch.nr_sipi = 1U;
	} else if (mode == APIC_DELMODE_STARTUP) {
		/* Ignore SIPIs in any state other than wait-for-SIPI */
		if ((target_vcpu->state != VCPU_INIT) ||
			(target_vcpu->arch.nr_sipi == 0U)) {
				return;
		}

		dev_dbg(ACRN_DBG_LAPIC,
				"Sending SIPI from VCPU %hu to %hu with vector %u",
				target_vcpu->vcpu_id, vcpu_id,
				(icr_low & APIC_VECTOR_MASK));

		target_vcpu->arch.nr_sipi--;
		if (target_vcpu->arch.nr_sipi > 0U) {
			return;
		}

		pr_err("Start Secondary VCPU%hu for VM[%d]...",
				target_vcpu->vcpu_id,
				target_vcpu->vm->vm_id);
		set_ap_entry(target_vcpu, (icr_low & APIC_VECTOR_MASK) << 12U);
		schedule_vcpu(target_vcpu);
	}
}

static int32_t
vlapic_icrlo_write_handler(struct acrn_vlapic *vlapic)
{
	uint16_t vcpu_id;
	bool phys;
	uint64_t dmask = 0UL;
	uint32_t icr_low, icr_high, dest;
	uint32_t vec, mode, shorthand;
	struct lapic_regs *lapic;
	struct acrn_vcpu *target_vcpu;

	lapic = &(vlapic->apic_page);
	lapic->icr_lo.v &= ~APIC_DELSTAT_PEND;

	icr_low = lapic->icr_lo.v;
	icr_high = lapic->icr_hi.v;
	if (is_x2apic_enabled(vlapic)) {
		dest = icr_high;
	} else {
		dest = icr_high >> APIC_ID_SHIFT;
	}
	vec = icr_low & APIC_VECTOR_MASK;
	mode = icr_low & APIC_DELMODE_MASK;
	phys = ((icr_low & APIC_DESTMODE_LOG) == 0UL);
	shorthand = icr_low & APIC_DEST_MASK;

	if ((mode == APIC_DELMODE_FIXED) && (vec < 16U)) {
		vlapic_set_error(vlapic, APIC_ESR_SEND_ILLEGAL_VECTOR);
		dev_dbg(ACRN_DBG_LAPIC, "Ignoring invalid IPI %u", vec);
		return 0;
	}

	dev_dbg(ACRN_DBG_LAPIC,
		"icrlo 0x%08x icrhi 0x%08x triggered ipi %u",
			icr_low, icr_high, vec);

	if (((shorthand == APIC_DEST_SELF) || (shorthand == APIC_DEST_ALLISELF))
		&& ((mode == APIC_DELMODE_NMI) || (mode == APIC_DELMODE_INIT)
		|| (mode == APIC_DELMODE_STARTUP))) {
		dev_dbg(ACRN_DBG_LAPIC, "Invalid ICR value");
		return 0;
	}

	switch (shorthand) {
	case APIC_DEST_DESTFLD:
		vlapic_calcdest(vlapic->vm, &dmask, dest, phys, false);
		break;
	case APIC_DEST_SELF:
		bitmap_set_lock(vlapic->vcpu->vcpu_id, &dmask);
		break;
	case APIC_DEST_ALLISELF:
		dmask = vm_active_cpus(vlapic->vm);
		break;
	case APIC_DEST_ALLESELF:
		dmask = vm_active_cpus(vlapic->vm);
		bitmap_clear_lock(vlapic->vcpu->vcpu_id, &dmask);
		break;
	default:
		/*
		 * All possible values of 'shorthand' has been handled in prior
		 * case clauses.
		 */
		break;
	}

	for (vcpu_id = 0U; vcpu_id < vlapic->vm->hw.created_vcpus; vcpu_id++) {
		if ((dmask & (1UL << vcpu_id)) != 0UL) {
			target_vcpu = vcpu_from_vid(vlapic->vm, vcpu_id);

			if (mode == APIC_DELMODE_FIXED) {
				vlapic_set_intr(target_vcpu, vec, LAPIC_TRIG_EDGE);
				dev_dbg(ACRN_DBG_LAPIC,
					"vlapic sending ipi %u to vcpu_id %hu",
					vec, vcpu_id);
			} else if (mode == APIC_DELMODE_NMI) {
				vcpu_inject_nmi(target_vcpu);
				dev_dbg(ACRN_DBG_LAPIC,
					"vlapic send ipi nmi to vcpu_id %hu", vcpu_id);
			} else if (mode == APIC_DELMODE_INIT) {
				vlapic_process_init_sipi(target_vcpu, mode, icr_low, vcpu_id);
			} else if (mode == APIC_DELMODE_STARTUP) {
				vlapic_process_init_sipi(target_vcpu, mode, icr_low, vcpu_id);
			} else if (mode == APIC_DELMODE_SMI) {
				pr_info("vlapic: SMI IPI do not support\n");
			} else {
				pr_err("Unhandled icrlo write with mode %u\n", mode);
			}
		}
	}

	return 0;	/* handled completely in the kernel */
}

/**
 * @brief Get pending virtual interrupts for vLAPIC.
 *
 * @param[in]    vlapic Pointer to target vLAPIC data structure
 * @param[inout] vecptr Pointer to vector buffer and will be filled
 *               with eligible vector if any.
 *
 * @retval 0 There is no eligible pending vector.
 * @retval 1 There is pending vector.
 *
 * @remark The vector does not automatically transition to the ISR as a
 *	   result of calling this function.
 */
int32_t
vlapic_pending_intr(const struct acrn_vlapic *vlapic, uint32_t *vecptr)
{
	const struct lapic_regs *lapic = &(vlapic->apic_page);
	uint32_t i, vector, val, bitpos;
	const struct lapic_reg *irrptr;

	if (is_apicv_intr_delivery_supported()) {
		return apicv_pending_intr(vlapic);
	}

	irrptr = &lapic->irr[0];

	/* i ranges effectively from 7 to 0 */
	for (i = 8U; i > 0U; ) {
		i--;
		val = atomic_load32(&irrptr[i].v);
		bitpos = (uint32_t)fls32(val);
		if (bitpos != INVALID_BIT_INDEX) {
			vector = (i * 32U) + bitpos;
			if (prio(vector) > prio(lapic->ppr.v)) {
				if (vecptr != NULL) {
					*vecptr = vector;
				}
				return 1;
			}
			break;
		}
	}
	return 0;
}

/**
 * @brief Accept virtual interrupt.
 *
 * Transition 'vector' from IRR to ISR. This function is called with the
 * vector returned by 'vlapic_pending_intr()' when the guest is able to
 * accept this interrupt (i.e. RFLAGS.IF = 1 and no conditions exist that
 * block interrupt delivery).
 *
 * @param[in] vlapic Pointer to target vLAPIC data structure
 * @param[in] vector Target virtual interrupt vector
 *
 * @return None
 *
 * @pre vlapic != NULL
 */
void
vlapic_intr_accepted(struct acrn_vlapic *vlapic, uint32_t vector)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);
	struct lapic_reg *irrptr, *isrptr;
	uint32_t idx, stk_top;

	/*
	 * clear the ready bit for vector being accepted in irr
	 * and set the vector as in service in isr.
	 */
	idx = vector >> 5U;

	irrptr = &lapic->irr[0];
	atomic_clear32(&irrptr[idx].v, 1U << (vector & 0x1fU));
	vlapic_dump_irr(vlapic, "vlapic_intr_accepted");

	isrptr = &lapic->isr[0];
	isrptr[idx].v |= 1U << (vector & 0x1fU);
	vlapic_dump_isr(vlapic, "vlapic_intr_accepted");

	/*
	 * Update the PPR
	 */
	vlapic->isrvec_stk_top++;

	stk_top = vlapic->isrvec_stk_top;
	if (stk_top >= ISRVEC_STK_SIZE) {
		panic("isrvec_stk_top overflow %u", stk_top);
	}

	vlapic->isrvec_stk[stk_top] = (uint8_t)vector;
	vlapic_update_ppr(vlapic);
}

static void
vlapic_svr_write_handler(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;
	uint32_t old, new, changed;

	lapic = &(vlapic->apic_page);

	new = lapic->svr.v;
	old = vlapic->svr_last;
	vlapic->svr_last = new;

	changed = old ^ new;
	if ((changed & APIC_SVR_ENABLE) != 0U) {
		if ((new & APIC_SVR_ENABLE) == 0U) {
			/*
			 * The apic is now disabled so stop the apic timer
			 * and mask all the LVT entries.
			 */
			dev_dbg(ACRN_DBG_LAPIC, "vlapic is software-disabled");
			del_timer(&vlapic->vtimer.timer);

			vlapic_mask_lvts(vlapic);
			/* the only one enabled LINT0-ExtINT vlapic disabled */
			if (vlapic->vm->wire_mode == VPIC_WIRE_NULL) {
				vlapic->vm->wire_mode = VPIC_WIRE_INTR;
				dev_dbg(ACRN_DBG_LAPIC,
					"vpic wire mode -> INTR");
			}
		} else {
			/*
			 * The apic is now enabled so restart the apic timer
			 * if it is configured in periodic mode.
			 */
			dev_dbg(ACRN_DBG_LAPIC, "vlapic is software-enabled");
			if (vlapic_lvtt_period(vlapic)) {
				if (set_expiration(vlapic)) {
					/* vlapic_init_timer has been called,
					 * and timer->fire_tsc is not 0,here
					 *  add_timer should not return error
					 */
					(void)add_timer(&vlapic->vtimer.timer);
				}
			}
		}
	}
}

static int32_t
vlapic_read(struct acrn_vlapic *vlapic, uint32_t offset_arg,
		uint64_t *data)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);
	uint32_t i;
	uint32_t offset = offset_arg;

	if (offset > sizeof(*lapic)) {
		*data = 0UL;
		goto done;
	}

	offset &= ~0x3UL;
	switch (offset) {
	case APIC_OFFSET_ID:
		*data = lapic->id.v;
		break;
	case APIC_OFFSET_VER:
		*data = lapic->version.v;
		break;
	case APIC_OFFSET_TPR:
		*data = vlapic_get_tpr(vlapic);
		break;
	case APIC_OFFSET_APR:
		*data = lapic->apr.v;
		break;
	case APIC_OFFSET_PPR:
		*data = lapic->ppr.v;
		break;
	case APIC_OFFSET_EOI:
		*data = lapic->eoi.v;
		break;
	case APIC_OFFSET_LDR:
		*data = lapic->ldr.v;
		break;
	case APIC_OFFSET_DFR:
		*data = lapic->dfr.v;
		break;
	case APIC_OFFSET_SVR:
		*data = lapic->svr.v;
		break;
	case APIC_OFFSET_ISR0:
	case APIC_OFFSET_ISR1:
	case APIC_OFFSET_ISR2:
	case APIC_OFFSET_ISR3:
	case APIC_OFFSET_ISR4:
	case APIC_OFFSET_ISR5:
	case APIC_OFFSET_ISR6:
	case APIC_OFFSET_ISR7:
		i = (offset - APIC_OFFSET_ISR0) >> 4U;
		*data = lapic->isr[i].v;
		break;
	case APIC_OFFSET_TMR0:
	case APIC_OFFSET_TMR1:
	case APIC_OFFSET_TMR2:
	case APIC_OFFSET_TMR3:
	case APIC_OFFSET_TMR4:
	case APIC_OFFSET_TMR5:
	case APIC_OFFSET_TMR6:
	case APIC_OFFSET_TMR7:
		i = (offset - APIC_OFFSET_TMR0) >> 4U;
		*data = lapic->tmr[i].v;
		break;
	case APIC_OFFSET_IRR0:
	case APIC_OFFSET_IRR1:
	case APIC_OFFSET_IRR2:
	case APIC_OFFSET_IRR3:
	case APIC_OFFSET_IRR4:
	case APIC_OFFSET_IRR5:
	case APIC_OFFSET_IRR6:
	case APIC_OFFSET_IRR7:
		i = (offset - APIC_OFFSET_IRR0) >> 4U;
		*data = lapic->irr[i].v;
		break;
	case APIC_OFFSET_ESR:
		*data = lapic->esr.v;
		break;
	case APIC_OFFSET_ICR_LOW:
		*data = lapic->icr_lo.v;
		break;
	case APIC_OFFSET_ICR_HI:
		*data = lapic->icr_hi.v;
		break;
	case APIC_OFFSET_CMCI_LVT:
	case APIC_OFFSET_TIMER_LVT:
	case APIC_OFFSET_THERM_LVT:
	case APIC_OFFSET_PERF_LVT:
	case APIC_OFFSET_LINT0_LVT:
	case APIC_OFFSET_LINT1_LVT:
	case APIC_OFFSET_ERROR_LVT:
		*data = vlapic_get_lvt(vlapic, offset);
#ifdef INVARIANTS
		reg = vlapic_get_lvtptr(vlapic, offset);
		ASSERT(*data == *reg,
			"inconsistent lvt value at offset %#x: %#lx/%#x",
			offset, *data, *reg);
#endif
		break;
	case APIC_OFFSET_TIMER_ICR:
		/* if TSCDEADLINE mode always return 0*/
		if (vlapic_lvtt_tsc_deadline(vlapic)) {
			*data = 0UL;
		} else {
			*data = lapic->icr_timer.v;
		}
		break;
	case APIC_OFFSET_TIMER_CCR:
		*data = vlapic_get_ccr(vlapic);
		break;
	case APIC_OFFSET_TIMER_DCR:
		*data = lapic->dcr_timer.v;
		break;
	case APIC_OFFSET_RRR:
	default:
		*data = 0UL;
		break;
	}
done:
	dev_dbg(ACRN_DBG_LAPIC,
			"vlapic read offset %#x, data %#lx", offset, *data);
	return 0;
}

static int32_t
vlapic_write(struct acrn_vlapic *vlapic, uint32_t offset,
		uint64_t data)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);
	uint32_t *regptr;
	uint32_t data32 = (uint32_t)data;
	int32_t retval;

	ASSERT(((offset & 0xfU) == 0U) && (offset < PAGE_SIZE),
		"%s: invalid offset %#x", __func__, offset);

	dev_dbg(ACRN_DBG_LAPIC, "vlapic write offset %#x, data %#lx",
		offset, data);

	retval = 0;
	if (offset <= sizeof(*lapic)) {
		switch (offset) {
		case APIC_OFFSET_ID:
			/* Force APIC ID as read only */
			break;
		case APIC_OFFSET_TPR:
			vlapic_set_tpr(vlapic, data32 & 0xffU);
			break;
		case APIC_OFFSET_EOI:
			vlapic_process_eoi(vlapic);
			break;
		case APIC_OFFSET_LDR:
			lapic->ldr.v = data32;
			vlapic_ldr_write_handler(vlapic);
			break;
		case APIC_OFFSET_DFR:
			lapic->dfr.v = data32;
			vlapic_dfr_write_handler(vlapic);
			break;
		case APIC_OFFSET_SVR:
			lapic->svr.v = data32;
			vlapic_svr_write_handler(vlapic);
			break;
		case APIC_OFFSET_ICR_LOW:
			if (is_x2apic_enabled(vlapic)) {
				lapic->icr_hi.v = (uint32_t)(data >> 32U);
				lapic->icr_lo.v = data32;
			} else {
				lapic->icr_lo.v = data32;
			}
			retval = vlapic_icrlo_write_handler(vlapic);
			break;
		case APIC_OFFSET_ICR_HI:
			lapic->icr_hi.v = data32;
			break;
		case APIC_OFFSET_CMCI_LVT:
		case APIC_OFFSET_TIMER_LVT:
		case APIC_OFFSET_THERM_LVT:
		case APIC_OFFSET_PERF_LVT:
		case APIC_OFFSET_LINT0_LVT:
		case APIC_OFFSET_LINT1_LVT:
		case APIC_OFFSET_ERROR_LVT:
			regptr = vlapic_get_lvtptr(vlapic, offset);
			*regptr = data32;
			vlapic_lvt_write_handler(vlapic, offset);
			break;
		case APIC_OFFSET_TIMER_ICR:
			/* if TSCDEADLINE mode ignore icr_timer */
			if (vlapic_lvtt_tsc_deadline(vlapic)) {
				break;
			}
			lapic->icr_timer.v = data32;
			vlapic_icrtmr_write_handler(vlapic);
			break;

		case APIC_OFFSET_TIMER_DCR:
			lapic->dcr_timer.v = data32;
			vlapic_dcr_write_handler(vlapic);
			break;

		case APIC_OFFSET_ESR:
			vlapic_esr_write_handler(vlapic);
			break;

		case APIC_OFFSET_VER:
		case APIC_OFFSET_APR:
		case APIC_OFFSET_PPR:
		case APIC_OFFSET_RRR:
			break;
	/*The following cases fall to the default one:
	 *	APIC_OFFSET_ISR0 ... APIC_OFFSET_ISR7
	 *	APIC_OFFSET_TMR0 ... APIC_OFFSET_TMR7
	 *	APIC_OFFSET_IRR0 ... APIC_OFFSET_IRR7
	 */
		case APIC_OFFSET_TIMER_CCR:
			break;
		default:
			/* Read only */
			break;
		}
	}

	return retval;
}

void
vlapic_reset(struct acrn_vlapic *vlapic)
{
	uint32_t i;
	struct lapic_regs *lapic;

	/*
	 * Upon reset, vlapic is set to xAPIC mode.
	 */
	vlapic->msr_apicbase = DEFAULT_APIC_BASE | APICBASE_ENABLED;

	if (vlapic->vcpu->vcpu_id == BOOT_CPU_ID) {
		vlapic->msr_apicbase |= APICBASE_BSP;
	}

	lapic = &(vlapic->apic_page);
	(void)memset((void *)lapic, 0U, sizeof(struct lapic_regs));
	(void)memset((void *)&(vlapic->pir_desc), 0U, sizeof(vlapic->pir_desc));

	lapic->id.v = vlapic_build_id(vlapic);
	lapic->version.v = VLAPIC_VERSION;
	lapic->version.v |= (VLAPIC_MAXLVT_INDEX << MAXLVTSHIFT);
	lapic->dfr.v = 0xffffffffU;
	lapic->svr.v = APIC_SVR_VECTOR;
	vlapic_mask_lvts(vlapic);
	vlapic_reset_tmr(vlapic);

	lapic->icr_timer.v = 0U;
	lapic->dcr_timer.v = 0U;
	vlapic_reset_timer(vlapic);

	vlapic->svr_last = lapic->svr.v;

	for (i = 0U; i < (VLAPIC_MAXLVT_INDEX + 1U); i++) {
		vlapic->lvt_last[i] = 0U;
	}

	for (i = 0U; i < ISRVEC_STK_SIZE; i++) {
		vlapic->isrvec_stk[i] = 0U;
	}

	vlapic->isrvec_stk_top = 0U;
}

/**
 * @pre vlapic->vm != NULL
 */
void
vlapic_init(struct acrn_vlapic *vlapic)
{
	ASSERT(vlapic->vcpu->vcpu_id < phys_cpu_num,
		"%s: vcpu_id is not initialized", __func__);

	vlapic_init_timer(vlapic);

	vlapic_reset(vlapic);
}

void vlapic_restore(struct acrn_vlapic *vlapic, const struct lapic_regs *regs)
{
	struct lapic_regs *lapic;
	int32_t i;

	lapic = &(vlapic->apic_page);

	lapic->tpr = regs->tpr;
	lapic->apr = regs->apr;
	lapic->ppr = regs->ppr;
	lapic->ldr = regs->ldr;
	lapic->dfr = regs->dfr;
	for (i = 0; i < 8; i++) {
		lapic->tmr[i].v = regs->tmr[i].v;
	}
	lapic->svr = regs->svr;
	vlapic_svr_write_handler(vlapic);
	lapic->lvt[APIC_LVT_TIMER].v = regs->lvt[APIC_LVT_TIMER].v;
	lapic->lvt[APIC_LVT_LINT0].v = regs->lvt[APIC_LVT_LINT0].v;
	lapic->lvt[APIC_LVT_LINT1].v = regs->lvt[APIC_LVT_LINT1].v;
	lapic->lvt[APIC_LVT_ERROR].v = regs->lvt[APIC_LVT_ERROR].v;
	lapic->icr_timer = regs->icr_timer;
	lapic->ccr_timer = regs->ccr_timer;
	lapic->dcr_timer = regs->dcr_timer;
}

static uint64_t
vlapic_get_apicbase(const struct acrn_vlapic *vlapic)
{

	return vlapic->msr_apicbase;
}

static int32_t
vlapic_set_apicbase(struct acrn_vlapic *vlapic, uint64_t new)
{

	uint64_t changed;
	changed = vlapic->msr_apicbase ^ new;

	if (changed == APICBASE_X2APIC) {
		if ((new & APICBASE_X2APIC) == APICBASE_X2APIC) {
			vlapic->msr_apicbase = new;
			vlapic_build_x2apic_id(vlapic);
			switch_apicv_mode_x2apic(vlapic->vcpu);
			return 0;
		}
	}

	if (vlapic->msr_apicbase != new) {
		dev_dbg(ACRN_DBG_LAPIC,
			"NOT support to change APIC_BASE MSR from %#lx to %#lx",
			vlapic->msr_apicbase, new);
		return (-1);
	}

	return 0;
}

void
vlapic_deliver_intr(struct acrn_vm *vm, bool level, uint32_t dest, bool phys,
		uint32_t delmode, uint32_t vec, bool rh)
{
	bool lowprio;
	uint16_t vcpu_id;
	uint64_t dmask;
	struct acrn_vcpu *target_vcpu;

	if ((delmode != IOAPIC_RTE_DELFIXED) &&
			(delmode != IOAPIC_RTE_DELLOPRI) &&
			(delmode != IOAPIC_RTE_DELEXINT)) {
		dev_dbg(ACRN_DBG_LAPIC,
			"vlapic intr invalid delmode %#x", delmode);
	} else {
		lowprio = (delmode == IOAPIC_RTE_DELLOPRI) || rh;

		/*
		 * We don't provide any virtual interrupt redirection hardware so
		 * all interrupts originating from the ioapic or MSI specify the
		 * 'dest' in the legacy xAPIC format.
		 */
		vlapic_calcdest(vm, &dmask, dest, phys, lowprio);

		for (vcpu_id = 0U; vcpu_id < vm->hw.created_vcpus; vcpu_id++) {
			struct acrn_vlapic *vlapic;
			if ((dmask & (1UL << vcpu_id)) != 0UL) {
				target_vcpu = vcpu_from_vid(vm, vcpu_id);

				/* only make request when vlapic enabled */
				vlapic = vcpu_vlapic(target_vcpu);
				if (vlapic_enabled(vlapic)) {
					if (delmode == IOAPIC_RTE_DELEXINT) {
						vcpu_inject_extint(target_vcpu);
					} else {
						vlapic_set_intr(target_vcpu, vec, level);
					}
				}
			}
		}
	}
}

bool
vlapic_enabled(const struct acrn_vlapic *vlapic)
{
	bool ret;
	const struct lapic_regs *lapic = &(vlapic->apic_page);

	if (((vlapic->msr_apicbase & APICBASE_ENABLED) != 0UL) &&
			((lapic->svr.v & APIC_SVR_ENABLE) != 0U)) {
		ret = true;
	} else {
	        ret = false;
	}

	return ret;
}

static void
vlapic_set_tmr(struct acrn_vlapic *vlapic, uint32_t vector, bool level)
{
	struct lapic_regs *lapic;
	struct lapic_reg *tmrptr;
	uint32_t mask, idx;

	lapic = &(vlapic->apic_page);
	tmrptr = &lapic->tmr[0];
	idx = vector >> 5U;
	mask = 1U << (vector & 0x1fU);
	if (level) {
		tmrptr[idx].v |= mask;
	} else {
		tmrptr[idx].v &= ~mask;
	}
}

/*
 * APICv batch set tmr will try to set multi vec at the same time
 * to avoid unnecessary VMCS read/update.
 */
void
vlapic_apicv_batch_set_tmr(struct acrn_vlapic *vlapic)
{
	if (is_apicv_intr_delivery_supported()) {
		apicv_batch_set_tmr(vlapic);
	}
}

void
vlapic_reset_tmr(struct acrn_vlapic *vlapic)
{
	struct acrn_vcpu *vcpu = vlapic->vcpu;
	uint32_t vector;

	dev_dbg(ACRN_DBG_LAPIC,
			"vlapic resetting all vectors to edge-triggered");

	for (vector = 0U; vector <= 255U; vector++) {
		vlapic_set_tmr(vlapic, vector, false);
	}

	vcpu_make_request(vcpu, ACRN_REQUEST_TMR_UPDATE);
}

void
vlapic_set_tmr_one_vec(struct acrn_vlapic *vlapic, uint32_t delmode,
	uint32_t vector, bool level)
{
	ASSERT(vector <= NR_MAX_VECTOR,
		"invalid vector %u", vector);

	/*
	 * A level trigger is valid only for fixed and lowprio delivery modes.
	 */
	if ((delmode != APIC_DELMODE_FIXED) && (delmode != APIC_DELMODE_LOWPRIO)) {
		dev_dbg(ACRN_DBG_LAPIC,
			"Ignoring level trigger-mode for delivery-mode %u",
			delmode);
	} else {
		/* NOTE
		 * We don't check whether the vcpu is in the dest here. That means
		 * all vcpus of vm will do tmr update.
		 *
		 * If there is new caller to this function, need to refine this
		 * part of work.
		 */
		dev_dbg(ACRN_DBG_LAPIC, "vector %u set to level-triggered", vector);
		vlapic_set_tmr(vlapic, vector, level);
	}
}

/*
 *  @pre vcpu != NULL
 *  @pre vector <= 255U
 */
void
vlapic_set_intr(struct acrn_vcpu *vcpu, uint32_t vector, bool level)
{
	struct acrn_vlapic *vlapic;

	vlapic = vcpu_vlapic(vcpu);
	if (vector < 16U) {
		vlapic_set_error(vlapic, APIC_ESR_RECEIVE_ILLEGAL_VECTOR);
		dev_dbg(ACRN_DBG_LAPIC,
		    "vlapic ignoring interrupt to vector %u", vector);
	} else {
		if (vlapic_set_intr_ready(vlapic, vector, level) != 0) {
			vcpu_make_request(vcpu, ACRN_REQUEST_EVENT);
		}
	}
}

/**
 * @brief Triggers LAPIC local interrupt(LVT).
 *
 * @param[in] vm           Pointer to VM data structure
 * @param[in] vcpu_id_arg  ID of vCPU, BROADCAST_CPU_ID means triggering
 *			   interrupt to all vCPUs.
 * @param[in] vector       Vector to be fired.
 *
 * @retval 0 on success.
 * @retval -EINVAL on error that vcpu_id_arg or vector is invalid.
 *
 * @pre vm != NULL
 */
int32_t
vlapic_set_local_intr(struct acrn_vm *vm, uint16_t vcpu_id_arg, uint32_t vector)
{
	struct acrn_vlapic *vlapic;
	uint64_t dmask = 0UL;
	int32_t error;
	uint16_t vcpu_id = vcpu_id_arg;

	if ((vcpu_id != BROADCAST_CPU_ID) && (vcpu_id >= vm->hw.created_vcpus)) {
	        error = -EINVAL;
	} else {
		if (vcpu_id == BROADCAST_CPU_ID) {
			dmask = vm_active_cpus(vm);
		} else {
			bitmap_set_lock(vcpu_id, &dmask);
		}
		error = 0;
		for (vcpu_id = 0U; vcpu_id < vm->hw.created_vcpus; vcpu_id++) {
			if ((dmask & (1UL << vcpu_id)) != 0UL) {
				vlapic = vm_lapic_from_vcpu_id(vm, vcpu_id);
				error = vlapic_trigger_lvt(vlapic, vector);
				if (error != 0) {
					break;
				}
			}
		}
	}

	return error;
}

/**
 * @brief Inject MSI to target VM.
 *
 * @param[in] vm   Pointer to VM data structure
 * @param[in] addr MSI address.
 * @param[in] msg  MSI data.
 *
 * @retval 0 on success.
 * @retval -1 on error that addr is invalid.
 *
 * @pre vm != NULL
 */
int32_t
vlapic_intr_msi(struct acrn_vm *vm, uint64_t addr, uint64_t msg)
{
	uint32_t delmode, vec;
	uint32_t dest;
	bool phys, rh;
	int32_t ret;

	dev_dbg(ACRN_DBG_LAPIC, "lapic MSI addr: %#lx msg: %#lx", addr, msg);

	if ((addr & MSI_ADDR_MASK) == MSI_ADDR_BASE) {
		/*
		 * Extract the x86-specific fields from the MSI addr/msg
		 * params according to the Intel Arch spec, Vol3 Ch 10.
		 *
		 * The PCI specification does not support level triggered
		 * MSI/MSI-X so ignore trigger level in 'msg'.
		 *
		 * The 'dest' is interpreted as a logical APIC ID if both
		 * the Redirection Hint and Destination Mode are '1' and
		 * physical otherwise.
		 */
		dest = (uint32_t)(addr >> 12U) & 0xffU;
		phys = ((addr & MSI_ADDR_LOG) != MSI_ADDR_LOG);
		rh = ((addr & MSI_ADDR_RH) == MSI_ADDR_RH);

		delmode = (uint32_t)msg & APIC_DELMODE_MASK;
		vec = (uint32_t)msg & 0xffU;

		dev_dbg(ACRN_DBG_LAPIC, "lapic MSI %s dest %#x, vec %u",
			phys ? "physical" : "logical", dest, vec);

		vlapic_deliver_intr(vm, LAPIC_TRIG_EDGE, dest, phys, delmode, vec, rh);
		ret = 0;
	} else {
		dev_dbg(ACRN_DBG_LAPIC, "lapic MSI invalid addr %#lx", addr);
	        ret = -1;
	}

	return ret;
}

/* interrupt context */
static void vlapic_timer_expired(void *data)
{
	struct acrn_vcpu *vcpu = (struct acrn_vcpu *)data;
	struct acrn_vlapic *vlapic;
	struct lapic_regs *lapic;

	vlapic = vcpu_vlapic(vcpu);
	lapic = &(vlapic->apic_page);

	/* inject vcpu timer interrupt if not masked */
	if (!vlapic_lvtt_masked(vlapic)) {
		vlapic_intr_edge(vcpu, lapic->lvt[APIC_LVT_TIMER].v & APIC_LVTT_VECTOR);
	}

	if (!vlapic_lvtt_period(vlapic)) {
		vlapic->vtimer.timer.fire_tsc = 0UL;
	}
}

static inline bool is_x2apic_enabled(const struct acrn_vlapic *vlapic)
{
	bool ret;
	if ((vlapic_get_apicbase(vlapic) & APICBASE_X2APIC) == 0UL) {
		ret = false;
	} else {
	        ret = true;
	}

	return ret;
}

static inline  uint32_t x2apic_msr_to_regoff(uint32_t msr)
{

	return (((msr - 0x800U) & 0x3FFU) << 4U);
}

#ifdef CONFIG_PARTITION_MODE
/*
 * If x2apic is pass-thru to guests, we have to special case the following
 * 1. INIT Delivery mode
 * 2. SIPI Delivery mode
 * For all other cases, send IPI on the wire.
 * No shorthand and Physical destination mode are only supported.
 */

static int32_t
vlapic_x2apic_pt_icr_access(struct acrn_vm *vm, uint64_t val)
{
	uint64_t apic_id = (uint32_t) (val >> 32U);
	uint32_t icr_low = val;
	uint32_t mode = icr_low & APIC_DELMODE_MASK;
	uint16_t vcpu_id;
	struct acrn_vcpu *target_vcpu;
	bool phys;
	uint32_t shorthand;

	phys = ((icr_low & APIC_DESTMODE_LOG) == 0UL);
	shorthand = icr_low & APIC_DEST_MASK;

	if ((phys == false) || (shorthand  != APIC_DEST_DESTFLD)) {
		pr_err("Logical destination mode or shorthands \
				not supported in ICR forpartition mode\n");
		return -1;
	}

	vcpu_id = vm_apicid2vcpu_id(vm, apic_id);
	target_vcpu = vcpu_from_vid(vm, vcpu_id);

	if (target_vcpu == NULL) {
		return 0;
	}
	switch (mode) {
	case APIC_DELMODE_INIT:
		vlapic_process_init_sipi(target_vcpu, mode, icr_low, vcpu_id);
	break;
	case APIC_DELMODE_STARTUP:
		vlapic_process_init_sipi(target_vcpu, mode, icr_low, vcpu_id);
	break;
	default:
		msr_write(MSR_IA32_EXT_APIC_ICR, (apic_id << 32U) | icr_low);
	break;
	}
	return 0;
}
#endif

static int32_t vlapic_x2apic_access(struct acrn_vcpu *vcpu, uint32_t msr, bool write,
								uint64_t *val)
{
	struct acrn_vlapic *vlapic;
	uint32_t offset;
	int32_t error = -1;

	/*
	 * If vLAPIC is in xAPIC mode and guest tries to access x2APIC MSRs
	 * inject a GP to guest
	 */
	vlapic = vcpu_vlapic(vcpu);
	if (is_x2apic_enabled(vlapic)) {
#ifdef CONFIG_PARTITION_MODE
		if (vcpu->vm->vm_desc->lapic_pt) {
			if (msr == MSR_IA32_EXT_APIC_ICR) {
				error = vlapic_x2apic_pt_icr_access(vcpu->vm, *val);
			}
			return error;
		}
#endif
		offset = x2apic_msr_to_regoff(msr);
		if (write) {
			if (!is_x2apic_read_only_msr(msr)) {
				error = vlapic_write(vlapic, offset, *val);
			}
		} else {
			if (!is_x2apic_write_only_msr(msr)) {
				error = vlapic_read(vlapic, offset, val);
			}
		}
	}

	return error;
}

int32_t
vlapic_rdmsr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *rval)
{
	int32_t error = 0;
	struct acrn_vlapic *vlapic;

	dev_dbg(ACRN_DBG_LAPIC, "cpu[%hu] rdmsr: %x", vcpu->vcpu_id, msr);
	vlapic = vcpu_vlapic(vcpu);

	switch (msr) {
	case MSR_IA32_APIC_BASE:
		*rval = vlapic_get_apicbase(vlapic);
		break;

	case MSR_IA32_TSC_DEADLINE:
		*rval = vlapic_get_tsc_deadline_msr(vlapic);
		break;

	default:
		if (is_x2apic_msr(msr)) {
			error = vlapic_x2apic_access(vcpu, msr, false, rval);
		} else {
			error = -1;
			dev_dbg(ACRN_DBG_LAPIC,
				"Invalid vlapic msr 0x%x access\n", msr);
		}
		break;
	}

	return error;
}

int32_t
vlapic_wrmsr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t wval)
{
	int32_t error = 0;
	struct acrn_vlapic *vlapic;

	vlapic = vcpu_vlapic(vcpu);

	switch (msr) {
	case MSR_IA32_APIC_BASE:
		error = vlapic_set_apicbase(vlapic, wval);
		break;

	case MSR_IA32_TSC_DEADLINE:
		vlapic_set_tsc_deadline_msr(vlapic, wval);
		break;

	default:
		if (is_x2apic_msr(msr)) {
			error = vlapic_x2apic_access(vcpu, msr, true, &wval);
		} else {
			error = -1;
			dev_dbg(ACRN_DBG_LAPIC,
				"Invalid vlapic msr 0x%x access\n", msr);
		}
		break;
	}

	dev_dbg(ACRN_DBG_LAPIC, "cpu[%hu] wrmsr: %x wval=%#x",
		vcpu->vcpu_id, msr, wval);
	return error;
}

int32_t vlapic_create(struct acrn_vcpu *vcpu)
{
	vcpu->arch.vlapic.vm = vcpu->vm;
	vcpu->arch.vlapic.vcpu = vcpu;

	if (is_vcpu_bsp(vcpu)) {
		uint64_t *pml4_page =
			(uint64_t *)vcpu->vm->arch_vm.nworld_eptp;
		/* only need unmap it from SOS as UOS never mapped it */
		if (is_vm0(vcpu->vm)) {
			ept_mr_del(vcpu->vm, pml4_page,
				DEFAULT_APIC_BASE, PAGE_SIZE);
		}

		ept_mr_add(vcpu->vm, pml4_page,
			vlapic_apicv_get_apic_access_addr(),
			DEFAULT_APIC_BASE, PAGE_SIZE,
			EPT_WR | EPT_RD | EPT_UNCACHED);
	}

	vlapic_init(vcpu_vlapic(vcpu));
	return 0;
}

/*
 *  @pre vcpu != NULL
 */
void vlapic_free(struct acrn_vcpu *vcpu)
{
	struct acrn_vlapic *vlapic = NULL;

	vlapic = vcpu_vlapic(vcpu);

	del_timer(&vlapic->vtimer.timer);

}

/**
 * APIC-v functions
 * **/
static int32_t
apicv_set_intr_ready(struct acrn_vlapic *vlapic, uint32_t vector)
{
	struct vlapic_pir_desc *pir_desc;
	uint64_t mask;
	uint32_t idx;
	int32_t notify;

	pir_desc = &(vlapic->pir_desc);

	idx = vector >> 6U;
	mask = 1UL << (vector & 0x3fU);

	atomic_set64(&pir_desc->pir[idx], mask);
	notify = (atomic_cmpxchg64(&pir_desc->pending, 0UL, 1UL) == 0UL) ? 1 : 0;
	return notify;
}

static int32_t
apicv_pending_intr(const struct acrn_vlapic *vlapic)
{
	const struct vlapic_pir_desc *pir_desc;
	const struct lapic_regs *lapic;
	uint64_t pending, pirval;
	uint32_t i, ppr, vpr;

	pir_desc = &(vlapic->pir_desc);

	pending = atomic_load64(&pir_desc->pending);
	if (pending == 0U) {
		return 0;
	}

	lapic = &(vlapic->apic_page);
	ppr = lapic->ppr.v & 0xF0U;

	if (ppr == 0U) {
		return 1;
	}

	/* i ranges effectively from 3 to 0 */
	for (i = 4U; i > 0U; ) {
		i--;
		pirval = pir_desc->pir[i];
		if (pirval != 0U) {
			vpr = (((i * 64U) + (uint32_t)fls64(pirval)) & 0xF0U);
			return (vpr > ppr) ? 1 : 0;
		}
	}

	return 0;
}

/* Update the VMX_EOI_EXIT according to related tmr */
#define	EOI_STEP_LEN	(64U)
#define	TMR_STEP_LEN	(32U)
static void
apicv_batch_set_tmr(const struct acrn_vlapic *vlapic)
{
	const struct lapic_regs *lapic = &(vlapic->apic_page);
	uint64_t val;
	const struct lapic_reg *ptr;
	uint32_t s, e;

	ptr = &lapic->tmr[0];
	s = 0U;
	e = 256U;

	while (s < e) {
		val = ptr[(s / TMR_STEP_LEN) + 1].v;
		val <<= TMR_STEP_LEN;
		val |= ptr[s / TMR_STEP_LEN].v;
		exec_vmwrite64(vmx_eoi_exit(s), val);

		s += EOI_STEP_LEN;
	}
}

/**
 *APIC-v: Get the HPA to APIC-access page
 * **/
uint64_t
vlapic_apicv_get_apic_access_addr(void)
{
	/*APIC-v APIC-access address */
	static uint8_t apicv_apic_access_addr[PAGE_SIZE] __aligned(PAGE_SIZE);

	return hva2hpa(apicv_apic_access_addr);
}

/**
 *APIC-v: Get the HPA to virtualized APIC registers page
 * **/
uint64_t
vlapic_apicv_get_apic_page_addr(struct acrn_vlapic *vlapic)
{
	return hva2hpa(&(vlapic->apic_page));
}

/*
 * Transfer the pending interrupts in the PIR descriptor to the IRR
 * in the virtual APIC page.
 */

void
vlapic_apicv_inject_pir(struct acrn_vlapic *vlapic)
{
	struct vlapic_pir_desc *pir_desc;
	struct lapic_regs *lapic;
	uint64_t val, pirval;
	uint16_t rvi, pirbase = 0U, i;
	uint16_t intr_status_old, intr_status_new;
	struct lapic_reg *irr = NULL;

	pir_desc = &(vlapic->pir_desc);
	if (atomic_cmpxchg64(&pir_desc->pending, 1UL, 0UL) == 1UL) {
		pirval = 0UL;
		lapic = &(vlapic->apic_page);
		irr = &lapic->irr[0];

		for (i = 0U; i < 4U; i++) {
			val = atomic_readandclear64(&pir_desc->pir[i]);
			if (val != 0UL) {
				irr[i * 2U].v |= (uint32_t)val;
				irr[(i * 2U) + 1U].v |= (uint32_t)(val >> 32U);

				pirbase = 64U * i;
				pirval = val;
			}
		}

		/*
		 * Update RVI so the processor can evaluate pending virtual
		 * interrupts on VM-entry.
		 *
		 * It is possible for pirval to be 0 here, even though the
		 * pending bit has been set. The scenario is:
		 * CPU-Y is sending a posted interrupt to CPU-X, which
		 * is running a guest and processing posted interrupts in h/w.
		 * CPU-X will eventually exit and the state seen in s/w is
		 * the pending bit set, but no PIR bits set.
		 *
		 *      CPU-X                      CPU-Y
		 *   (vm running)                (host running)
		 *   rx posted interrupt
		 *   CLEAR pending bit
		 *				 SET PIR bit
		 *   READ/CLEAR PIR bits
		 *				 SET pending bit
		 *   (vm exit)
		 *   pending bit set, PIR 0
		 */
		if (pirval != 0UL) {
			rvi = pirbase + fls64(pirval);

			intr_status_old = 0xFFFFU &
					exec_vmread16(VMX_GUEST_INTR_STATUS);

			intr_status_new = (intr_status_old & 0xFF00U) | rvi;
			if (intr_status_new > intr_status_old) {
				exec_vmwrite16(VMX_GUEST_INTR_STATUS,
						intr_status_new);
			}
		}
	}
}

int32_t apic_access_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t err = 0;
	uint32_t offset = 0U;
	uint64_t qual, access_type;
	struct acrn_vlapic *vlapic;
	struct mmio_request *mmio = &vcpu->req.reqs.mmio;

	qual = vcpu->arch.exit_qualification;
	access_type = apic_access_type(qual);

	/*parse offset if linear access*/
	if (access_type <= 3UL) {
		offset = (uint32_t)apic_access_offset(qual);
	}

	vlapic = vcpu_vlapic(vcpu);

	err = decode_instruction(vcpu);
	/* apic access should already fetched instruction, decode_instruction
	 * will not trigger #PF, so if it failed, just return error_no
	 */
	if (err < 0) {
		return err;
	}

	if (access_type == 1UL) {
		if (emulate_instruction(vcpu) == 0) {
			err = vlapic_write(vlapic, offset, mmio->value);
		}
	} else if (access_type == 0UL) {
		err = vlapic_read(vlapic, offset, &mmio->value);
		if (err < 0) {
			return err;
		}
		err = emulate_instruction(vcpu);
	} else {
		pr_err("Unhandled APIC access type: %lu\n", access_type);
		err = -EINVAL;
	}

	TRACE_2L(TRACE_VMEXIT_APICV_ACCESS, qual, (uint64_t)vlapic);
	return err;
}

int32_t veoi_vmexit_handler(struct acrn_vcpu *vcpu)
{
	struct acrn_vlapic *vlapic = NULL;

	uint32_t vector;
	struct lapic_regs *lapic;
	struct lapic_reg *tmrptr;
	uint32_t idx, mask;

	vcpu_retain_rip(vcpu);

	vlapic = vcpu_vlapic(vcpu);
	lapic = &(vlapic->apic_page);
	vector = (uint32_t)(vcpu->arch.exit_qualification & 0xFFUL);

	tmrptr = &lapic->tmr[0];
	idx = vector >> 5U;
	mask = 1U << (vector & 0x1fU);

	if ((tmrptr[idx].v & mask) != 0U) {
		/* hook to vIOAPIC */
		vioapic_process_eoi(vlapic->vm, vector);
	}

	TRACE_2L(TRACE_VMEXIT_APICV_VIRT_EOI, vector, 0UL);

	return 0;
}

static void vlapic_x2apic_self_ipi_handler(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;
	uint32_t vector;
	struct acrn_vcpu *target_vcpu;

	lapic = &(vlapic->apic_page);
	vector = lapic->self_ipi.v & 0xFFU;
	target_vcpu = vlapic->vcpu;
	vlapic_set_intr(target_vcpu, vector, LAPIC_TRIG_EDGE);
}

int32_t apic_write_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint64_t qual;
	int32_t error, handled;
	uint32_t offset;
	struct acrn_vlapic *vlapic = NULL;

	qual = vcpu->arch.exit_qualification;
	offset = (uint32_t)(qual & 0xFFFUL);

	handled = 1;
	vcpu_retain_rip(vcpu);
	vlapic = vcpu_vlapic(vcpu);

	switch (offset) {
	case APIC_OFFSET_ID:
		/* Force APIC ID as read only */
		break;
	case APIC_OFFSET_EOI:
		vlapic_process_eoi(vlapic);
		break;
	case APIC_OFFSET_LDR:
		vlapic_ldr_write_handler(vlapic);
		break;
	case APIC_OFFSET_DFR:
		vlapic_dfr_write_handler(vlapic);
		break;
	case APIC_OFFSET_SVR:
		vlapic_svr_write_handler(vlapic);
		break;
	case APIC_OFFSET_ESR:
		vlapic_esr_write_handler(vlapic);
		break;
	case APIC_OFFSET_ICR_LOW:
		error = vlapic_icrlo_write_handler(vlapic);
		if (error != 0) {
			handled = 0;
		}
		break;
	case APIC_OFFSET_CMCI_LVT:
	case APIC_OFFSET_TIMER_LVT:
	case APIC_OFFSET_THERM_LVT:
	case APIC_OFFSET_PERF_LVT:
	case APIC_OFFSET_LINT0_LVT:
	case APIC_OFFSET_LINT1_LVT:
	case APIC_OFFSET_ERROR_LVT:
		vlapic_lvt_write_handler(vlapic, offset);
		break;
	case APIC_OFFSET_TIMER_ICR:
		vlapic_icrtmr_write_handler(vlapic);
		break;
	case APIC_OFFSET_TIMER_DCR:
		vlapic_dcr_write_handler(vlapic);
		break;
	case APIC_OFFSET_SELF_IPI:
		if (is_x2apic_enabled(vlapic)) {
			vlapic_x2apic_self_ipi_handler(vlapic);
		}
		break;
	default:
		handled = 0;
		pr_err("Unhandled APIC-Write, offset:0x%x", offset);
		break;
	}

	TRACE_2L(TRACE_VMEXIT_APICV_WRITE, offset, 0UL);

	return handled;
}

int32_t tpr_below_threshold_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
	pr_err("Unhandled %s.", __func__);
	return 0;
}
