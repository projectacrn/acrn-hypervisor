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

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <atomic.h>
#include <per_cpu.h>
#include <pgtable.h>
#include <lapic.h>
#include <vmcs.h>
#include <vlapic.h>
#include <ptdev.h>
#include <vmx.h>
#include <vm.h>
#include <ept.h>
#include <trace.h>
#include <logmsg.h>
#include "vlapic_priv.h"

#define VLAPIC_VERBOS 0

static inline uint32_t prio(uint32_t x)
{
	return (x >> 4U);
}

#define VLAPIC_VERSION		(16U)
#define	APICBASE_BSP		0x00000100UL
#define	APICBASE_X2APIC		0x00000400UL
#define APICBASE_XAPIC		0x00000800UL
#define APICBASE_LAPIC_MODE	(APICBASE_XAPIC | APICBASE_X2APIC)
#define	APICBASE_ENABLED	0x00000800UL
#define LOGICAL_ID_MASK		0xFU
#define CLUSTER_ID_MASK		0xFFFF0U

#define DBG_LEVEL_VLAPIC		6U

static inline struct acrn_vcpu *vlapic2vcpu(const struct acrn_vlapic *vlapic)
{
	return container_of(container_of(vlapic, struct acrn_vcpu_arch, vlapic), struct acrn_vcpu, arch);
}

#if VLAPIC_VERBOS
static inline void vlapic_dump_irr(const struct acrn_vlapic *vlapic, const char *msg)
{
	const struct lapic_reg *irrptr = &(vlapic->apic_page.irr[0]);

	for (uint8_t i = 0U; i < 8U; i++) {
		dev_dbg(DBG_LEVEL_VLAPIC, "%s irr%u 0x%08x", msg, i, irrptr[i].v);
	}
}

static inline void vlapic_dump_isr(const struct acrn_vlapic *vlapic, const char *msg)
{
	const struct lapic_reg *isrptr = &(vlapic->apic_page.isr[0]);

	for (uint8_t i = 0U; i < 8U; i++) {
		dev_dbg(DBG_LEVEL_VLAPIC, "%s isr%u 0x%08x", msg, i, isrptr[i].v);
	}
}
#else
static inline void vlapic_dump_irr(__unused const struct acrn_vlapic *vlapic, __unused const char *msg) {}

static inline void vlapic_dump_isr(__unused const struct acrn_vlapic *vlapic, __unused const char *msg) {}
#endif

const struct acrn_apicv_ops *apicv_ops;

static bool apicv_set_intr_ready(struct acrn_vlapic *vlapic, uint32_t vector);

static void apicv_trigger_pi_anv(uint16_t dest_pcpu_id, uint32_t anv);

static void vlapic_x2apic_self_ipi_handler(struct acrn_vlapic *vlapic);

/*
 * Post an interrupt to the vcpu running on 'hostcpu'. This will use a
 * hardware assist if available (e.g. Posted Interrupt) or fall back to
 * sending an 'ipinum' to interrupt the 'hostcpu'.
 */
static void vlapic_set_error(struct acrn_vlapic *vlapic, uint32_t mask);

static void vlapic_timer_expired(void *data);

static inline bool vlapic_enabled(const struct acrn_vlapic *vlapic)
{
	const struct lapic_regs *lapic = &(vlapic->apic_page);

	return (((vlapic->msr_apicbase & APICBASE_ENABLED) != 0UL) &&
			((lapic->svr.v & APIC_SVR_ENABLE) != 0U));
}

static struct acrn_vlapic *
vm_lapic_from_vcpu_id(struct acrn_vm *vm, uint16_t vcpu_id)
{
	struct acrn_vcpu *vcpu;

	vcpu = vcpu_from_vid(vm, vcpu_id);

	return vcpu_vlapic(vcpu);
}

static uint16_t vm_apicid2vcpu_id(struct acrn_vm *vm, uint32_t lapicid)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;
	uint16_t cpu_id = INVALID_CPU_ID;

	foreach_vcpu(i, vm, vcpu) {
		if (vcpu_vlapic(vcpu)->vapic_id == lapicid) {
			cpu_id = vcpu->vcpu_id;
			break;
		}
	}

	if (cpu_id == INVALID_CPU_ID) {
		pr_err("%s: bad lapicid %lu", __func__, lapicid);
	}

	return cpu_id;

}

static inline void vlapic_build_x2apic_id(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;
	uint32_t logical_id, cluster_id;

	lapic = &(vlapic->apic_page);
	lapic->id.v = vlapic->vapic_id;
	logical_id = lapic->id.v & LOGICAL_ID_MASK;
	cluster_id = (lapic->id.v & CLUSTER_ID_MASK) >> 4U;
	lapic->ldr.v = (cluster_id << 16U) | (1U << logical_id);
}

static inline uint32_t vlapic_find_isrv(const struct acrn_vlapic *vlapic)
{
	const struct lapic_regs *lapic = &(vlapic->apic_page);
	uint32_t i, val, bitpos, isrv = 0U;
	const struct lapic_reg *isrptr;

	isrptr = &lapic->isr[0];

	/* i ranges effectively from 7 to 1 */
	for (i = 7U; i > 0U; i--) {
		val = isrptr[i].v;
		if (val != 0U) {
			bitpos = (uint32_t)fls32(val);
			isrv = (i << 5U) | bitpos;
			break;
		}
	}

	return isrv;
}

static void
vlapic_write_dfr(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;

	lapic = &(vlapic->apic_page);
	lapic->dfr.v &= APIC_DFR_MODEL_MASK;
	lapic->dfr.v |= APIC_DFR_RESERVED;

	if ((lapic->dfr.v & APIC_DFR_MODEL_MASK) == APIC_DFR_MODEL_FLAT) {
		dev_dbg(DBG_LEVEL_VLAPIC, "vlapic DFR in Flat Model");
	} else if ((lapic->dfr.v & APIC_DFR_MODEL_MASK)
			== APIC_DFR_MODEL_CLUSTER) {
		dev_dbg(DBG_LEVEL_VLAPIC, "vlapic DFR in Cluster Model");
	} else {
		dev_dbg(DBG_LEVEL_VLAPIC, "DFR in Unknown Model %#x", lapic->dfr);
	}
}

static void
vlapic_write_ldr(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;

	lapic = &(vlapic->apic_page);
	lapic->ldr.v &= ~APIC_LDR_RESERVED;
	dev_dbg(DBG_LEVEL_VLAPIC, "vlapic LDR set to %#x", lapic->ldr);
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
			vlapic_timer_expired, vlapic2vcpu(vlapic),
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

	if ((vtimer->tmicr != 0U) && (!vlapic_lvtt_tsc_deadline(vlapic))) {
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

static void vlapic_write_dcr(struct acrn_vlapic *vlapic)
{
	uint32_t divisor_shift;
	struct vlapic_timer *vtimer;
	struct lapic_regs *lapic = &(vlapic->apic_page);

	vtimer = &vlapic->vtimer;
	divisor_shift = vlapic_timer_divisor_shift(lapic->dcr_timer.v);

	vtimer->divisor_shift = divisor_shift;
}

static void vlapic_write_icrtmr(struct acrn_vlapic *vlapic)
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

uint64_t vlapic_get_tsc_deadline_msr(const struct acrn_vlapic *vlapic)
{
	uint64_t ret;
	struct acrn_vcpu *vcpu = vlapic2vcpu(vlapic);

	if (is_lapic_pt_enabled(vcpu)) {
		/* If physical TSC_DEADLINE is zero which means it's not armed (automatically disarmed
		 * after timer triggered), return 0 and reset the virtual TSC_DEADLINE;
		 * If physical TSC_DEADLINE is not zero, return the virtual TSC_DEADLINE value.
		 */
		if (msr_read(MSR_IA32_TSC_DEADLINE) == 0UL) {
			vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE, 0UL);
			ret = 0UL;
		} else {
			ret = vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE);
		}
	} else if (!vlapic_lvtt_tsc_deadline(vlapic)) {
		ret = 0UL;
	} else {
		ret = (vlapic->vtimer.timer.fire_tsc == 0UL) ? 0UL :
			vcpu_get_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE);
	}

	return ret;
}

void vlapic_set_tsc_deadline_msr(struct acrn_vlapic *vlapic, uint64_t val_arg)
{
	struct hv_timer *timer;
	uint64_t val = val_arg;
	struct acrn_vcpu *vcpu = vlapic2vcpu(vlapic);

	if (is_lapic_pt_enabled(vcpu)) {
		vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE, val);
		/* If val is not zero, which mean guest intends to arm the tsc_deadline timer,
		 * if the calculated value to write to the physical TSC_DEADLINE msr is zero,
		 * we plus 1 to not disarm the physcial timer falsely;
		 * If val is zero, which means guest intends to disarm the tsc_deadline timer,
		 * we disarm the physical timer.
		 */
		if (val != 0UL) {
			val -= exec_vmread64(VMX_TSC_OFFSET_FULL);
			if (val == 0UL) {
				val += 1UL;
			}
			msr_write(MSR_IA32_TSC_DEADLINE, val);
		} else {
			msr_write(MSR_IA32_TSC_DEADLINE, 0);
		}
	} else if (vlapic_lvtt_tsc_deadline(vlapic)) {
		vcpu_set_guest_msr(vcpu, MSR_IA32_TSC_DEADLINE, val);

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
	} else {
		/* No action required */
	}
}

static void
vlapic_write_esr(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;

	lapic = &(vlapic->apic_page);
	lapic->esr.v = vlapic->esr_pending;
	vlapic->esr_pending = 0U;
}

static void
vlapic_set_tmr(struct acrn_vlapic *vlapic, uint32_t vector, bool level)
{
	struct lapic_reg *tmrptr = &(vlapic->apic_page.tmr[0]);
	if (level) {
		if (!bitmap32_test_and_set_lock((uint16_t)(vector & 0x1fU), &tmrptr[(vector & 0xffU) >> 5U].v)) {
			vcpu_set_eoi_exit_bitmap(vlapic2vcpu(vlapic), vector);
		}
	} else {
		if (bitmap32_test_and_clear_lock((uint16_t)(vector & 0x1fU), &tmrptr[(vector & 0xffU) >> 5U].v)) {
			vcpu_clear_eoi_exit_bitmap(vlapic2vcpu(vlapic), vector);
		}
	}
}

static void
vlapic_reset_tmr(struct acrn_vlapic *vlapic)
{
	int16_t i;
	struct lapic_regs *lapic;

	dev_dbg(DBG_LEVEL_VLAPIC,
			"vlapic resetting all vectors to edge-triggered");

	lapic = &(vlapic->apic_page);
	for (i = 0; i < 8; i++) {
		lapic->tmr[i].v = 0U;
	}

	vcpu_reset_eoi_exit_bitmaps(vlapic2vcpu(vlapic));
}

static void apicv_basic_accept_intr(struct acrn_vlapic *vlapic, uint32_t vector, bool level)
{
	struct lapic_regs *lapic;
	struct lapic_reg *irrptr;
	uint32_t idx;

	lapic = &(vlapic->apic_page);
	idx = vector >> 5U;
	irrptr = &lapic->irr[0];

	/* If the interrupt is set, don't try to do it again */
	if (!bitmap32_test_and_set_lock((uint16_t)(vector & 0x1fU), &irrptr[idx].v)) {
		/* update TMR if interrupt trigger mode has changed */
		vlapic_set_tmr(vlapic, vector, level);
		vcpu_make_request(vlapic2vcpu(vlapic), ACRN_REQUEST_EVENT);
	}
}

static void apicv_advanced_accept_intr(struct acrn_vlapic *vlapic, uint32_t vector, bool level)
{
	/* update TMR if interrupt trigger mode has changed */
	vlapic_set_tmr(vlapic, vector, level);

	if (apicv_set_intr_ready(vlapic, vector)) {
		struct acrn_vcpu *vcpu = vlapic2vcpu(vlapic);
		/*
		 * Send interrupt to vCPU via posted interrupt way:
		 * 1. If target vCPU is in root mode(isn't running),
		 *    record this request as ACRN_REQUEST_EVENT,then
		 *    will pick up the interrupt from PIR and inject
		 *    it to vCPU in next vmentry.
		 * 2. If target vCPU is in non-root mode(running),
		 *    send PI notification to vCPU and hardware will
		 *    sync PIR to vIRR automatically.
		 */
		bitmap_set_lock(ACRN_REQUEST_EVENT, &vcpu->arch.pending_req);

		if (get_pcpu_id() != pcpuid_from_vcpu(vcpu)) {
			apicv_trigger_pi_anv(pcpuid_from_vcpu(vcpu), (uint32_t)vcpu->arch.pid.control.bits.nv);
		}
	}
}

/*
 * @pre vector >= 16
 */
static void vlapic_accept_intr(struct acrn_vlapic *vlapic, uint32_t vector, bool level)
{
	struct lapic_regs *lapic;
	ASSERT(vector <= NR_MAX_VECTOR, "invalid vector %u", vector);

	lapic = &(vlapic->apic_page);
	if ((lapic->svr.v & APIC_SVR_ENABLE) == 0U) {
		dev_dbg(DBG_LEVEL_VLAPIC, "vlapic is software disabled, ignoring interrupt %u", vector);
	} else {
		signal_event(&vlapic2vcpu(vlapic)->events[VCPU_EVENT_VIRTUAL_INTERRUPT]);
		vlapic->ops->accept_intr(vlapic, vector, level);
	}
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
 * @param[in] anv Activation Notification Vectors (ANV)
 *
 * @return None
 */
static void apicv_trigger_pi_anv(uint16_t dest_pcpu_id, uint32_t anv)
{
	send_single_ipi(dest_pcpu_id, anv);
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
	uint32_t *lvt_ptr;

	switch (offset) {
	case APIC_OFFSET_CMCI_LVT:
		lvt_ptr = &lapic->lvt_cmci.v;
		break;
	default:
		/*
		 * The function caller could guarantee the pre condition.
		 * All the possible 'offset' other than APIC_OFFSET_CMCI_LVT
		 * could be handled here.
		 */
		i = lvt_off_to_idx(offset);
		lvt_ptr = &(lapic->lvt[i].v);
		break;
	}
	return lvt_ptr;
}

static inline uint32_t
vlapic_get_lvt(const struct acrn_vlapic *vlapic, uint32_t offset)
{
	uint32_t idx;

	idx = lvt_off_to_idx(offset);
	return vlapic->lvt_last[idx];
}

static void
vlapic_write_lvt(struct acrn_vlapic *vlapic, uint32_t offset)
{
	uint32_t *lvtptr, mask, val, idx;
	struct lapic_regs *lapic;
	bool error = false;

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
		struct acrn_vm *vm = vlapic2vcpu(vlapic)->vm;

		/* mask -> unmask: may from every vlapic in the vm */
		if (((last & APIC_LVT_M) != 0U) && ((val & APIC_LVT_M) == 0U)) {
			if ((vm->wire_mode == VPIC_WIRE_INTR) ||
				(vm->wire_mode == VPIC_WIRE_NULL)) {
				vm->wire_mode = VPIC_WIRE_LAPIC;
				dev_dbg(DBG_LEVEL_VLAPIC,
					"vpic wire mode -> LAPIC");
			} else {
				pr_err("WARNING:invalid vpic wire mode change");
				error = true;
			}
		/* unmask -> mask: only from the vlapic LINT0-ExtINT enabled */
		} else if (((last & APIC_LVT_M) == 0U) && ((val & APIC_LVT_M) != 0U)) {
			if (vm->wire_mode == VPIC_WIRE_LAPIC) {
				vm->wire_mode = VPIC_WIRE_NULL;
				dev_dbg(DBG_LEVEL_VLAPIC,
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

	if (error == false) {
		*lvtptr = val;
		idx = lvt_off_to_idx(offset);
		vlapic->lvt_last[idx] = val;
	}
}

static void
vlapic_mask_lvts(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);

	lapic->lvt_cmci.v |= APIC_LVT_M;
	vlapic_write_lvt(vlapic, APIC_OFFSET_CMCI_LVT);

	lapic->lvt[APIC_LVT_TIMER].v |= APIC_LVT_M;
	vlapic_write_lvt(vlapic, APIC_OFFSET_TIMER_LVT);

	lapic->lvt[APIC_LVT_THERMAL].v |= APIC_LVT_M;
	vlapic_write_lvt(vlapic, APIC_OFFSET_THERM_LVT);

	lapic->lvt[APIC_LVT_PMC].v |= APIC_LVT_M;
	vlapic_write_lvt(vlapic, APIC_OFFSET_PERF_LVT);

	lapic->lvt[APIC_LVT_LINT0].v |= APIC_LVT_M;
	vlapic_write_lvt(vlapic, APIC_OFFSET_LINT0_LVT);

	lapic->lvt[APIC_LVT_LINT1].v |= APIC_LVT_M;
	vlapic_write_lvt(vlapic, APIC_OFFSET_LINT1_LVT);

	lapic->lvt[APIC_LVT_ERROR].v |= APIC_LVT_M;
	vlapic_write_lvt(vlapic, APIC_OFFSET_ERROR_LVT);
}

/*
 * @pre vec = (lvt & APIC_LVT_VECTOR) >=16
 */
static void
vlapic_fire_lvt(struct acrn_vlapic *vlapic, uint32_t lvt)
{
	if ((lvt & APIC_LVT_M) == 0U) {
		struct acrn_vcpu *vcpu = vlapic2vcpu(vlapic);
		uint32_t vec = lvt & APIC_LVT_VECTOR;
		uint32_t mode = lvt & APIC_LVT_DM;

		switch (mode) {
		case APIC_LVT_DM_FIXED:
			vlapic_set_intr(vcpu, vec, LAPIC_TRIG_EDGE);
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
			break;
		}
	}
	return;
}

/*
 * Algorithm adopted from section "Interrupt, Task and Processor Priority"
 * in Intel Architecture Manual Vol 3a.
 */
static void
vlapic_update_ppr(struct acrn_vlapic *vlapic)
{
	uint32_t isrv, tpr, ppr;

	isrv = vlapic->isrv;
	tpr = vlapic->apic_page.tpr.v;

	if (prio(tpr) >= prio(isrv)) {
		ppr = tpr;
	} else {
		ppr = isrv & 0xf0U;
	}

	vlapic->apic_page.ppr.v = ppr;
	dev_dbg(DBG_LEVEL_VLAPIC, "%s 0x%02x", __func__, ppr);
}

static void
vlapic_process_eoi(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);
	struct lapic_reg *isrptr, *tmrptr;
	uint32_t i, vector, bitpos;

	isrptr = &lapic->isr[0];
	tmrptr = &lapic->tmr[0];

	if (vlapic->isrv != 0U) {
		vector = vlapic->isrv;
		i = (vector >> 5U);
		bitpos = (vector & 0x1fU);
		bitmap32_clear_nolock((uint16_t)bitpos, &isrptr[i].v);

		dev_dbg(DBG_LEVEL_VLAPIC, "EOI vector %u", vector);
		vlapic_dump_isr(vlapic, "vlapic_process_eoi");

		vlapic->isrv = vlapic_find_isrv(vlapic);
		vlapic_update_ppr(vlapic);

		if (bitmap32_test((uint16_t)bitpos, &tmrptr[i].v)) {
			/*
			 * Per Intel SDM 10.8.5, Software can inhibit the broadcast of
			 * EOI by setting bit 12 of the Spurious Interrupt Vector
			 * Register of the LAPIC.
			 * TODO: Check if the bit 12 "Suppress EOI Broadcasts" is set.
			 */
			vioapic_broadcast_eoi(vlapic2vcpu(vlapic)->vm, vector);
		}

		vcpu_make_request(vlapic2vcpu(vlapic), ACRN_REQUEST_EVENT);
	}

	dev_dbg(DBG_LEVEL_VLAPIC, "Gratuitous EOI");
}

static void
vlapic_set_error(struct acrn_vlapic *vlapic, uint32_t mask)
{
	uint32_t lvt, vec;

	vlapic->esr_pending |= mask;
	if (vlapic->esr_firing == 0) {
		vlapic->esr_firing = 1;

		/* The error LVT always uses the fixed delivery mode. */
		lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_ERROR_LVT);
		if ((lvt & APIC_LVT_M) == 0U) {
			vec = lvt & APIC_LVT_VECTOR;
			if (vec >= 16U) {
				vlapic_accept_intr(vlapic, vec, LAPIC_TRIG_EDGE);
			}
		}
		vlapic->esr_firing = 0;
	}
}
/*
 * @pre APIC_LVT_TIMER <= lvt_index <= APIC_LVT_MAX
 */
static int32_t
vlapic_trigger_lvt(struct acrn_vlapic *vlapic, uint32_t lvt_index)
{
	uint32_t lvt;
	int32_t ret = 0;

	if (vlapic_enabled(vlapic) == false) {
		struct acrn_vcpu *vcpu = vlapic2vcpu(vlapic);
		/*
		 * When the local APIC is global/hardware disabled,
		 * LINT[1:0] pins are configured as INTR and NMI pins,
		 * respectively.
		 */
		switch (lvt_index) {
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
	} else {

		switch (lvt_index) {
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
			lvt = 0U; /* make MISRA happy */
			ret =  -EINVAL;
			break;
		}

		if (ret == 0) {
			vlapic_fire_lvt(vlapic, lvt);
		}
	}
	return ret;
}

static inline void set_dest_mask_phys(struct acrn_vm *vm, uint64_t *dmask, uint32_t dest)
{
	uint16_t vcpu_id;

	vcpu_id = vm_apicid2vcpu_id(vm, dest);
	if (vcpu_id < vm->hw.created_vcpus) {
		bitmap_set_nolock(vcpu_id, dmask);
	}
}

/*
 * This function tells if a vlapic belongs to the destination.
 * If yes, return true, else reture false.
 *
 * @pre vlapic != NULL
 */
static inline bool is_dest_field_matched(const struct acrn_vlapic *vlapic, uint32_t dest)
{
	uint32_t logical_id, cluster_id, dest_logical_id, dest_cluster_id;
	uint32_t ldr = vlapic->apic_page.ldr.v;
	bool ret = false;

	if (is_x2apic_enabled(vlapic)) {
		logical_id = ldr & 0xFFFFU;
		cluster_id = (ldr >> 16U) & 0xFFFFU;
		dest_logical_id = dest & 0xFFFFU;
		dest_cluster_id = (dest >> 16U) & 0xFFFFU;
		if ((cluster_id == dest_cluster_id) && ((logical_id & dest_logical_id) != 0U)) {
			ret = true;
		}
	} else {
		uint32_t dfr = vlapic->apic_page.dfr.v;
		if ((dfr & APIC_DFR_MODEL_MASK) == APIC_DFR_MODEL_FLAT) {
			/*
			 * In the "Flat Model" the MDA is interpreted as an 8-bit wide
			 * bitmask. This model is available in the xAPIC mode only.
			 */
			logical_id = ldr >> 24U;
			dest_logical_id = dest & 0xffU;
			if ((logical_id & dest_logical_id) != 0U) {
				ret = true;
			}
		} else if ((dfr & APIC_DFR_MODEL_MASK) == APIC_DFR_MODEL_CLUSTER) {
			/*
			 * In the "Cluster Model" the MDA is used to identify a
			 * specific cluster and a set of APICs in that cluster.
			 */
			logical_id = (ldr >> 24U) & 0xfU;
			cluster_id = ldr >> 28U;
			dest_logical_id = dest & 0xfU;
			dest_cluster_id = (dest >> 4U) & 0xfU;
			if ((cluster_id == dest_cluster_id) && ((logical_id & dest_logical_id) != 0U)) {
				ret = true;
			}
		} else {
			/* Guest has configured a bad logical model for this vcpu. */
			dev_dbg(DBG_LEVEL_VLAPIC, "vlapic has bad logical model %x", dfr);
		}
	}

	return ret;
}

/*
 * This function populates 'dmask' with the set of vcpus that match the
 * addressing specified by the (dest, phys, lowprio) tuple.
 */
void
vlapic_calc_dest(struct acrn_vm *vm, uint64_t *dmask, bool is_broadcast,
		uint32_t dest, bool phys, bool lowprio)
{
	struct acrn_vlapic *vlapic, *lowprio_dest = NULL;
	struct acrn_vcpu *vcpu;
	uint16_t vcpu_id;

	*dmask = 0UL;
	if (is_broadcast) {
		/* Broadcast in both logical and physical modes. */
		*dmask = vm_active_cpus(vm);
	} else if (phys) {
		/* Physical mode: "dest" is local APIC ID. */
		set_dest_mask_phys(vm, dmask, dest);
	} else {
		/*
		 * Logical mode: "dest" is message destination addr
		 * to be compared with the logical APIC ID in LDR.
		 */
		foreach_vcpu(vcpu_id, vm, vcpu) {
			vlapic = vm_lapic_from_vcpu_id(vm, vcpu_id);
			if (!is_dest_field_matched(vlapic, dest)) {
				continue;
			}

			if (lowprio) {
				/*
				 * for lowprio delivery mode, the lowest-priority one
				 * among all "dest" matched processors accepts the intr.
				 */
				if (lowprio_dest == NULL) {
					lowprio_dest = vlapic;
				} else if (lowprio_dest->apic_page.ppr.v > vlapic->apic_page.ppr.v) {
					lowprio_dest = vlapic;
				} else {
					/* No other state currently, do nothing */
				}
			} else {
				bitmap_set_nolock(vcpu_id, dmask);
			}
		}

		if (lowprio && (lowprio_dest != NULL)) {
			bitmap_set_nolock(vlapic2vcpu(lowprio_dest)->vcpu_id, dmask);
		}
	}
}

/*
 * This function populates 'dmask' with the set of "possible" destination vcpu when lapic is passthru.
 * Hardware will handle the real delivery mode among all "possible" dest processors:
 * deliver to the lowprio one for lowprio mode.
 *
 * @pre is_x2apic_enabled(vlapic) == true
 */
void
vlapic_calc_dest_lapic_pt(struct acrn_vm *vm, uint64_t *dmask, bool is_broadcast,
		uint32_t dest, bool phys)
{
	struct acrn_vlapic *vlapic;
	struct acrn_vcpu *vcpu;
	uint16_t vcpu_id;

	*dmask = 0UL;
	if (is_broadcast) {
		/* Broadcast in both logical and physical modes. */
		*dmask = vm_active_cpus(vm);
	} else if (phys) {
		/* Physical mode: "dest" is local APIC ID. */
		set_dest_mask_phys(vm, dmask, dest);
	} else {
		/*
		 * Logical mode: "dest" is message destination addr
		 * to be compared with the logical APIC ID in LDR.
		 */
		foreach_vcpu(vcpu_id, vm, vcpu) {
			vlapic = vm_lapic_from_vcpu_id(vm, vcpu_id);
			if (!is_dest_field_matched(vlapic, dest)) {
				continue;
			}
			bitmap_set_nolock(vcpu_id, dmask);
		}
		dev_dbg(DBG_LEVEL_LAPICPT, "%s: logical destmod, dmask: 0x%016lx", __func__, *dmask);
	}
}

static void
vlapic_process_init_sipi(struct acrn_vcpu* target_vcpu, uint32_t mode, uint32_t icr_low)
{
	get_vm_lock(target_vcpu->vm);
	if (mode == APIC_DELMODE_INIT) {
		if ((icr_low & APIC_LEVEL_MASK) != APIC_LEVEL_DEASSERT) {

			dev_dbg(DBG_LEVEL_VLAPIC,
				"Sending INIT to %hu",
				target_vcpu->vcpu_id);

			if (target_vcpu->state != VCPU_INIT) {
				/* put target vcpu to INIT state and wait for SIPI */
				zombie_vcpu(target_vcpu, VCPU_ZOMBIE);
				reset_vcpu(target_vcpu, INIT_RESET);
			}
			/* new cpu model only need one SIPI to kick AP run,
			 * the second SIPI will be ignored as it move out of
			 * wait-for-SIPI state.
			*/
			target_vcpu->arch.nr_sipi = 1U;
		}
	} else if (mode == APIC_DELMODE_STARTUP) {
		/* Ignore SIPIs in any state other than wait-for-SIPI */
		if ((target_vcpu->state == VCPU_INIT) &&
			(target_vcpu->arch.nr_sipi != 0U)) {

			dev_dbg(DBG_LEVEL_VLAPIC,
				"Sending SIPI to %hu with vector %u",
				 target_vcpu->vcpu_id,
				(icr_low & APIC_VECTOR_MASK));

			target_vcpu->arch.nr_sipi--;
			if (target_vcpu->arch.nr_sipi <= 0U) {

				pr_err("Start Secondary VCPU%hu for VM[%d]...",
					target_vcpu->vcpu_id,
					target_vcpu->vm->vm_id);

				set_vcpu_startup_entry(target_vcpu, (icr_low & APIC_VECTOR_MASK) << 12U);
				vcpu_make_request(target_vcpu, ACRN_REQUEST_INIT_VMCS);
				launch_vcpu(target_vcpu);
			}
		}
	} else {
		/* No other state currently, do nothing */
	}
	put_vm_lock(target_vcpu->vm);
	return;
}

static void vlapic_write_icrlo(struct acrn_vlapic *vlapic)
{
	uint16_t vcpu_id;
	bool phys = false, is_broadcast = false;
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
		is_broadcast = (dest == 0xffffffffU);
	} else {
		dest = icr_high >> APIC_ID_SHIFT;
		is_broadcast = (dest == 0xffU);
	}
	vec = icr_low & APIC_VECTOR_MASK;
	mode = icr_low & APIC_DELMODE_MASK;
	phys = ((icr_low & APIC_DESTMODE_LOG) == 0UL);
	shorthand = icr_low & APIC_DEST_MASK;

	if ((mode == APIC_DELMODE_FIXED) && (vec < 16U)) {
		vlapic_set_error(vlapic, APIC_ESR_SEND_ILLEGAL_VECTOR);
		dev_dbg(DBG_LEVEL_VLAPIC, "Ignoring invalid IPI %u", vec);
	} else if (((shorthand == APIC_DEST_SELF) || (shorthand == APIC_DEST_ALLISELF))
			&& ((mode == APIC_DELMODE_NMI) || (mode == APIC_DELMODE_INIT)
			|| (mode == APIC_DELMODE_STARTUP))) {
		dev_dbg(DBG_LEVEL_VLAPIC, "Invalid ICR value");
	} else {
		struct acrn_vcpu *vcpu = vlapic2vcpu(vlapic);

		dev_dbg(DBG_LEVEL_VLAPIC,
			"icrlo 0x%08x icrhi 0x%08x triggered ipi %u",
				icr_low, icr_high, vec);

		switch (shorthand) {
		case APIC_DEST_DESTFLD:
			vlapic_calc_dest(vcpu->vm, &dmask, is_broadcast, dest, phys, false);
			break;
		case APIC_DEST_SELF:
			bitmap_set_nolock(vcpu->vcpu_id, &dmask);
			break;
		case APIC_DEST_ALLISELF:
			dmask = vm_active_cpus(vcpu->vm);
			break;
		case APIC_DEST_ALLESELF:
			dmask = vm_active_cpus(vcpu->vm);
			bitmap_clear_nolock(vlapic2vcpu(vlapic)->vcpu_id, &dmask);
			break;
		default:
			/*
			 * All possible values of 'shorthand' has been handled in prior
			 * case clauses.
			 */
			break;
		}

		for (vcpu_id = 0U; vcpu_id < vcpu->vm->hw.created_vcpus; vcpu_id++) {
			if ((dmask & (1UL << vcpu_id)) != 0UL) {
				target_vcpu = vcpu_from_vid(vcpu->vm, vcpu_id);

				if (mode == APIC_DELMODE_FIXED) {
					vlapic_set_intr(target_vcpu, vec, LAPIC_TRIG_EDGE);
					dev_dbg(DBG_LEVEL_VLAPIC,
						"vlapic sending ipi %u to vcpu_id %hu",
						vec, vcpu_id);
				} else if (mode == APIC_DELMODE_NMI) {
					vcpu_inject_nmi(target_vcpu);
					dev_dbg(DBG_LEVEL_VLAPIC,
						"vlapic send ipi nmi to vcpu_id %hu", vcpu_id);
				} else if (mode == APIC_DELMODE_INIT) {
					vlapic_process_init_sipi(target_vcpu, mode, icr_low);
				} else if (mode == APIC_DELMODE_STARTUP) {
					vlapic_process_init_sipi(target_vcpu, mode, icr_low);
				} else if (mode == APIC_DELMODE_SMI) {
					pr_info("vlapic: SMI IPI do not support\n");
				} else {
					pr_err("Unhandled icrlo write with mode %u\n", mode);
				}
			}
		}
	}
}

static inline uint32_t vlapic_find_highest_irr(const struct acrn_vlapic *vlapic)
{
	const struct lapic_regs *lapic = &(vlapic->apic_page);
	uint32_t i, val, bitpos, vec = 0U;
	const struct lapic_reg *irrptr;

	irrptr = &lapic->irr[0];

	/* i ranges effectively from 7 to 1 */
	for (i = 7U; i > 0U; i--) {
		val = irrptr[i].v;
		if (val != 0U) {
			bitpos = (uint32_t)fls32(val);
			vec = (i * 32U) + bitpos;
			break;
		}
	}

	return vec;
}

/**
 * @brief Find a deliverable virtual interrupts for vLAPIC in irr.
 *
 * @param[in]    vlapic Pointer to target vLAPIC data structure
 * @param[inout] vecptr Pointer to vector buffer and will be filled
 *               with eligible vector if any.
 *
 * @retval false There is no deliverable pending vector.
 * @retval true There is deliverable vector.
 *
 * @remark The vector does not automatically transition to the ISR as a
 *	   result of calling this function.
 *	   This function is only for case that APICv/VID is NOT supported.
 */
static bool vlapic_find_deliverable_intr(const struct acrn_vlapic *vlapic, uint32_t *vecptr)
{
	const struct lapic_regs *lapic = &(vlapic->apic_page);
	uint32_t vec;
	bool ret = false;

	vec = vlapic_find_highest_irr(vlapic);
	if (prio(vec) > prio(lapic->ppr.v)) {
		ret = true;
		if (vecptr != NULL) {
			*vecptr = vec;
		}
	}

	return ret;
}

/**
 * @brief Get a deliverable virtual interrupt from irr to isr.
 *
 * Transition 'vector' from IRR to ISR. This function is called with the
 * vector returned by 'vlapic_find_deliverable_intr()' when the guest is able to
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
static void vlapic_get_deliverable_intr(struct acrn_vlapic *vlapic, uint32_t vector)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);
	struct lapic_reg *irrptr, *isrptr;
	uint32_t idx;

	/*
	 * clear the ready bit for vector being accepted in irr
	 * and set the vector as in service in isr.
	 */
	idx = vector >> 5U;

	irrptr = &lapic->irr[0];
	bitmap32_clear_lock((uint16_t)(vector & 0x1fU), &irrptr[idx].v);

	vlapic_dump_irr(vlapic, "vlapic_get_deliverable_intr");

	isrptr = &lapic->isr[0];
	bitmap32_set_nolock((uint16_t)(vector & 0x1fU), &isrptr[idx].v);
	vlapic_dump_isr(vlapic, "vlapic_get_deliverable_intr");

	vlapic->isrv = vector;

	/*
	 * Update the PPR
	 */
	vlapic_update_ppr(vlapic);
}

static void
vlapic_write_svr(struct acrn_vlapic *vlapic)
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
			struct acrn_vm *vm = vlapic2vcpu(vlapic)->vm;
			/*
			 * The apic is now disabled so stop the apic timer
			 * and mask all the LVT entries.
			 */
			dev_dbg(DBG_LEVEL_VLAPIC, "vlapic is software-disabled");
			del_timer(&vlapic->vtimer.timer);

			vlapic_mask_lvts(vlapic);
			/* the only one enabled LINT0-ExtINT vlapic disabled */
			if (vm->wire_mode == VPIC_WIRE_NULL) {
				vm->wire_mode = VPIC_WIRE_INTR;
				dev_dbg(DBG_LEVEL_VLAPIC,
					"vpic wire mode -> INTR");
			}
		} else {
			/*
			 * The apic is now enabled so restart the apic timer
			 * if it is configured in periodic mode.
			 */
			dev_dbg(DBG_LEVEL_VLAPIC, "vlapic is software-enabled");
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

static int32_t vlapic_read(struct acrn_vlapic *vlapic, uint32_t offset_arg, uint64_t *data)
{
	int32_t ret = 0;
	struct lapic_regs *lapic = &(vlapic->apic_page);
	uint32_t i;
	uint32_t offset = offset_arg;
	*data = 0UL;

	if (offset > sizeof(*lapic)) {
		ret = -EACCES;
	} else {

		offset &= ~0x3UL;
		switch (offset) {
		case APIC_OFFSET_ID:
			*data = lapic->id.v;
			break;
		case APIC_OFFSET_VER:
			*data = lapic->version.v;
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
			if (is_x2apic_enabled(vlapic)) {
				*data |= ((uint64_t)lapic->icr_hi.v) << 32U;
			}
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
			ASSERT(*data == *reg, "inconsistent lvt value at offset %#x: %#lx/%#x", offset, *data, *reg);
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
		default:
			ret = -EACCES;
			break;
		}
	}

	dev_dbg(DBG_LEVEL_VLAPIC, "vlapic read offset %x, data %lx", offset, *data);
	return ret;
}

static int32_t vlapic_write(struct acrn_vlapic *vlapic, uint32_t offset, uint64_t data)
{
	struct lapic_regs *lapic = &(vlapic->apic_page);
	uint32_t *regptr;
	uint32_t data32 = (uint32_t)data;
	int32_t ret = 0;

	ASSERT(((offset & 0xfU) == 0U) && (offset < PAGE_SIZE),
		"%s: invalid offset %#x", __func__, offset);

	dev_dbg(DBG_LEVEL_VLAPIC, "vlapic write offset %#x, data %#lx", offset, data);

	if (offset <= sizeof(*lapic)) {
		switch (offset) {
		case APIC_OFFSET_ID:
			/* Force APIC ID as read only */
			break;
		case APIC_OFFSET_EOI:
			vlapic_process_eoi(vlapic);
			break;
		case APIC_OFFSET_LDR:
			lapic->ldr.v = data32;
			vlapic_write_ldr(vlapic);
			break;
		case APIC_OFFSET_DFR:
			lapic->dfr.v = data32;
			vlapic_write_dfr(vlapic);
			break;
		case APIC_OFFSET_SVR:
			lapic->svr.v = data32;
			vlapic_write_svr(vlapic);
			break;
		case APIC_OFFSET_ICR_LOW:
			if (is_x2apic_enabled(vlapic)) {
				lapic->icr_hi.v = (uint32_t)(data >> 32U);
			}
			lapic->icr_lo.v = data32;
			vlapic_write_icrlo(vlapic);
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
			vlapic_write_lvt(vlapic, offset);
			break;
		case APIC_OFFSET_TIMER_ICR:
			/* if TSCDEADLINE mode ignore icr_timer */
			if (vlapic_lvtt_tsc_deadline(vlapic)) {
				break;
			}
			lapic->icr_timer.v = data32;
			vlapic_write_icrtmr(vlapic);
			break;

		case APIC_OFFSET_TIMER_DCR:
			lapic->dcr_timer.v = data32;
			vlapic_write_dcr(vlapic);
			break;
		case APIC_OFFSET_ESR:
			vlapic_write_esr(vlapic);
			break;

		case APIC_OFFSET_SELF_IPI:
			if (is_x2apic_enabled(vlapic)) {
				lapic->self_ipi.v = data32;
				vlapic_x2apic_self_ipi_handler(vlapic);
				break;
			}
			/* falls through */

		default:
			ret = -EACCES;
			/* Read only */
			break;
		}
	} else {
		ret = -EACCES;
	}

	return ret;
}

/*
 * @pre vlapic != NULL && ops != NULL
 */
void
vlapic_reset(struct acrn_vlapic *vlapic, const struct acrn_apicv_ops *ops, enum reset_mode mode)
{
	struct lapic_regs *lapic;
	uint64_t preserved_lapic_mode = vlapic->msr_apicbase & APICBASE_LAPIC_MODE;
	uint32_t preserved_apic_id = vlapic->apic_page.id.v;

	vlapic->msr_apicbase = DEFAULT_APIC_BASE;

	if (vlapic2vcpu(vlapic)->vcpu_id == BSP_CPU_ID) {
		vlapic->msr_apicbase |= APICBASE_BSP;
	}
	if (mode == INIT_RESET) {
		if ((preserved_lapic_mode & APICBASE_ENABLED) != 0U ) {
			/* Per SDM 10.12.5.1 vol.3, need to preserve lapic mode after INIT */
			vlapic->msr_apicbase |= preserved_lapic_mode;
		}
	} else {
		/* Upon reset, vlapic is set to xAPIC mode. */
		vlapic->msr_apicbase |= APICBASE_XAPIC;
	}

	lapic = &(vlapic->apic_page);
	(void)memset((void *)lapic, 0U, sizeof(struct lapic_regs));

	if (mode == INIT_RESET) {
		if ((preserved_lapic_mode & APICBASE_ENABLED) != 0U ) {
			/* the local APIC ID register should be preserved in XAPIC or X2APIC mode */
			lapic->id.v = preserved_apic_id;
		}
	} else {
		lapic->id.v = vlapic->vapic_id;
		if (!is_x2apic_enabled(vlapic)) {
			lapic->id.v <<= APIC_ID_SHIFT;
		}
	}
	lapic->version.v = VLAPIC_VERSION;
	lapic->version.v |= (VLAPIC_MAXLVT_INDEX << MAXLVTSHIFT);
	lapic->dfr.v = 0xffffffffU;
	lapic->svr.v = APIC_SVR_VECTOR;
	vlapic_mask_lvts(vlapic);
	vlapic_reset_tmr(vlapic);

	lapic->icr_timer.v = 0U;
	lapic->dcr_timer.v = 0U;
	vlapic_write_dcr(vlapic);
	vlapic_reset_timer(vlapic);

	vlapic->svr_last = lapic->svr.v;

	vlapic->isrv = 0U;

	vlapic->ops = ops;
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
	vlapic_write_svr(vlapic);
	lapic->lvt[APIC_LVT_TIMER].v = regs->lvt[APIC_LVT_TIMER].v;
	lapic->lvt[APIC_LVT_LINT0].v = regs->lvt[APIC_LVT_LINT0].v;
	lapic->lvt[APIC_LVT_LINT1].v = regs->lvt[APIC_LVT_LINT1].v;
	lapic->lvt[APIC_LVT_ERROR].v = regs->lvt[APIC_LVT_ERROR].v;
	lapic->icr_timer = regs->icr_timer;
	lapic->ccr_timer = regs->ccr_timer;
	lapic->dcr_timer = regs->dcr_timer;
	vlapic_write_dcr(vlapic);
}

uint64_t vlapic_get_apicbase(const struct acrn_vlapic *vlapic)
{
	return vlapic->msr_apicbase;
}

static void ptapic_accept_intr(struct acrn_vlapic *vlapic, uint32_t vector, __unused bool level)
{
	pr_err("Invalid op %s, VM%u, vCPU%u, vector %u", __func__,
			vlapic2vcpu(vlapic)->vm->vm_id, vlapic2vcpu(vlapic)->vcpu_id, vector);
}

static void ptapic_inject_intr(struct acrn_vlapic *vlapic,
				__unused bool guest_irq_enabled, __unused bool injected)
{
	pr_err("Invalid op %s, VM%u, vCPU%u", __func__, vlapic2vcpu(vlapic)->vm->vm_id, vlapic2vcpu(vlapic)->vcpu_id);
}

static bool ptapic_has_pending_delivery_intr(__unused struct acrn_vcpu *vcpu)
{
	return false;
}

static bool ptapic_has_pending_intr(__unused struct acrn_vcpu *vcpu)
{
	return false;
}

static bool ptapic_invalid(__unused uint32_t offset)
{
	return false;
}

static const struct acrn_apicv_ops ptapic_ops = {
	.accept_intr = ptapic_accept_intr,
	.inject_intr = ptapic_inject_intr,
	.has_pending_delivery_intr = ptapic_has_pending_delivery_intr,
	.has_pending_intr = ptapic_has_pending_intr,
	.apic_read_access_may_valid  = ptapic_invalid,
	.apic_write_access_may_valid  = ptapic_invalid,
	.x2apic_read_msr_may_valid  = ptapic_invalid,
	.x2apic_write_msr_may_valid  = ptapic_invalid,
};

int32_t vlapic_set_apicbase(struct acrn_vlapic *vlapic, uint64_t new)
{
	int32_t ret = 0;
	uint64_t changed;
	bool change_in_vlapic_mode = false;

	if (vlapic->msr_apicbase != new) {
		changed = vlapic->msr_apicbase ^ new;
		change_in_vlapic_mode = ((changed & APICBASE_LAPIC_MODE) != 0U);

		/*
		 * TODO: Logic to check for change in Reserved Bits and Inject GP
		 */

		/*
		 * Logic to check for change in Bits 11:10 for vLAPIC mode switch
		 */
		if (change_in_vlapic_mode) {
			if ((new & APICBASE_LAPIC_MODE) ==
						(APICBASE_XAPIC | APICBASE_X2APIC)) {
				struct acrn_vcpu *vcpu = vlapic2vcpu(vlapic);

				if (is_lapic_pt_configured(vcpu->vm)) {
					/* vlapic need to be reset to make sure it is in correct state */
					vlapic_reset(vlapic, &ptapic_ops, SOFTWARE_RESET);
				}
				vlapic->msr_apicbase = new;
				vlapic_build_x2apic_id(vlapic);
				switch_apicv_mode_x2apic(vcpu);
				update_vm_vlapic_state(vcpu->vm);
			} else {
				/*
				 * TODO: Logic to check for Invalid transitions, Invalid State
				 * and mode switch according to SDM 10.12.5
				 * Fig. 10-27
				 */
			}
		}

		/*
		 * TODO: Logic to check for change in Bits 35:12 and Bit 7 and emulate
		 */
	}

	return ret;
}

void
vlapic_receive_intr(struct acrn_vm *vm, bool level, uint32_t dest, bool phys,
		uint32_t delmode, uint32_t vec, bool rh)
{
	bool lowprio;
	uint16_t vcpu_id;
	uint64_t dmask;
	struct acrn_vcpu *target_vcpu;

	if ((delmode != IOAPIC_RTE_DELMODE_FIXED) &&
			(delmode != IOAPIC_RTE_DELMODE_LOPRI) &&
			(delmode != IOAPIC_RTE_DELMODE_EXINT)) {
		dev_dbg(DBG_LEVEL_VLAPIC,
			"vlapic intr invalid delmode %#x", delmode);
	} else {
		lowprio = (delmode == IOAPIC_RTE_DELMODE_LOPRI) || rh;

		/*
		 * We don't provide any virtual interrupt redirection hardware so
		 * all interrupts originating from the ioapic or MSI specify the
		 * 'dest' in the legacy xAPIC format.
		 */
		vlapic_calc_dest(vm, &dmask, false, dest, phys, lowprio);

		for (vcpu_id = 0U; vcpu_id < vm->hw.created_vcpus; vcpu_id++) {
			struct acrn_vlapic *vlapic;
			if ((dmask & (1UL << vcpu_id)) != 0UL) {
				target_vcpu = vcpu_from_vid(vm, vcpu_id);

				/* only make request when vlapic enabled */
				vlapic = vcpu_vlapic(target_vcpu);
				if (vlapic_enabled(vlapic)) {
					if (delmode == IOAPIC_RTE_DELMODE_EXINT) {
						vcpu_inject_extint(target_vcpu);
					} else {
						vlapic_set_intr(target_vcpu, vec, level);
					}
				}
			}
		}
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
		dev_dbg(DBG_LEVEL_VLAPIC,
		    "vlapic ignoring interrupt to vector %u", vector);
	} else {
		vlapic_accept_intr(vlapic, vector, level);
	}
}

/**
 * @brief Triggers LAPIC local interrupt(LVT).
 *
 * @param[in] vm           Pointer to VM data structure
 * @param[in] vcpu_id_arg  ID of vCPU, BROADCAST_CPU_ID means triggering
 *			   interrupt to all vCPUs.
 * @param[in] lvt_index    The index which LVT would be to be fired.
 *
 * @retval 0 on success.
 * @retval -EINVAL on error that vcpu_id_arg or vector of the LVT is invalid.
 *
 * @pre vm != NULL
 */
int32_t
vlapic_set_local_intr(struct acrn_vm *vm, uint16_t vcpu_id_arg, uint32_t lvt_index)
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
			bitmap_set_nolock(vcpu_id, &dmask);
		}
		error = 0;
		for (vcpu_id = 0U; vcpu_id < vm->hw.created_vcpus; vcpu_id++) {
			if ((dmask & (1UL << vcpu_id)) != 0UL) {
				vlapic = vm_lapic_from_vcpu_id(vm, vcpu_id);
				error = vlapic_trigger_lvt(vlapic, lvt_index);
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
	union msi_addr_reg address;
	union msi_data_reg data;

	address.full = addr;
	data.full = (uint32_t) msg;
	dev_dbg(DBG_LEVEL_VLAPIC, "lapic MSI addr: %#lx msg: %#lx", address.full, data.full);

	if (address.bits.addr_base == MSI_ADDR_BASE) {
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
		dest = address.bits.dest_field;
		phys = (address.bits.dest_mode == MSI_ADDR_DESTMODE_PHYS);
		rh = (address.bits.rh == MSI_ADDR_RH);

		delmode = (uint32_t)(data.bits.delivery_mode);
		vec = (uint32_t)(data.bits.vector);

		dev_dbg(DBG_LEVEL_VLAPIC, "lapic MSI %s dest %#x, vec %u",
			phys ? "physical" : "logical", dest, vec);

		vlapic_receive_intr(vm, LAPIC_TRIG_EDGE, dest, phys, delmode, vec, rh);
		ret = 0;
	} else {
		dev_dbg(DBG_LEVEL_VLAPIC, "lapic MSI invalid addr %#lx", address.full);
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
		vlapic_set_intr(vcpu, lapic->lvt[APIC_LVT_TIMER].v & APIC_LVTT_VECTOR, LAPIC_TRIG_EDGE);
	}

	if (!vlapic_lvtt_period(vlapic)) {
		vlapic->vtimer.timer.fire_tsc = 0UL;
	}
}

/*
 * @pre vm != NULL
 */
bool is_x2apic_enabled(const struct acrn_vlapic *vlapic)
{
	bool ret = false;

	if ((vlapic_get_apicbase(vlapic) & APICBASE_LAPIC_MODE) == (APICBASE_X2APIC | APICBASE_XAPIC)) {
		ret = true;
	}

	return ret;
}

bool is_xapic_enabled(const struct acrn_vlapic *vlapic)
{
	bool ret = false;
	if ((vlapic_get_apicbase(vlapic) & APICBASE_LAPIC_MODE) == APICBASE_XAPIC) {
	        ret = true;
	}

	return ret;
}

static inline  uint32_t x2apic_msr_to_regoff(uint32_t msr)
{

	return (((msr - 0x800U) & 0x3FFU) << 4U);
}

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
	uint32_t papic_id, vapic_id = (uint32_t)(val >> 32U);
	uint32_t icr_low = (uint32_t)val;
	uint32_t mode = icr_low & APIC_DELMODE_MASK;
	uint16_t vcpu_id;
	struct acrn_vcpu *target_vcpu;
	bool phys;
	uint32_t shorthand;
	int32_t ret = -1;

	phys = ((icr_low & APIC_DESTMODE_LOG) == 0UL);
	shorthand = icr_low & APIC_DEST_MASK;

	if ((phys == false) || (shorthand  != APIC_DEST_DESTFLD)) {
		pr_err("Logical destination mode or shorthands \
				not supported in ICR forpartition mode\n");
		/*
		 * TODO: To support logical destination and shorthand modes
		 */
	} else {
		vcpu_id = vm_apicid2vcpu_id(vm, vapic_id);
		if ((vcpu_id < vm->hw.created_vcpus) && (vm->hw.vcpu_array[vcpu_id].state != VCPU_OFFLINE)) {
			target_vcpu = vcpu_from_vid(vm, vcpu_id);

			switch (mode) {
			case APIC_DELMODE_INIT:
				vlapic_process_init_sipi(target_vcpu, mode, icr_low);
			break;
			case APIC_DELMODE_STARTUP:
				vlapic_process_init_sipi(target_vcpu, mode, icr_low);
			break;
			default:
				/* convert the dest from virtual apic_id to physical apic_id */
				if (is_x2apic_enabled(vcpu_vlapic(target_vcpu))) {
					papic_id = per_cpu(lapic_id, pcpuid_from_vcpu(target_vcpu));
					dev_dbg(DBG_LEVEL_LAPICPT,
						"%s vapic_id: 0x%08lx papic_id: 0x%08lx icr_low:0x%08lx",
						 __func__, vapic_id, papic_id, icr_low);
					msr_write(MSR_IA32_EXT_APIC_ICR, (((uint64_t)papic_id) << 32U) | icr_low);
				}
			break;
			}
			ret = 0;
		}
	}
	return ret;
}

static bool apicv_basic_x2apic_read_msr_may_valid(uint32_t offset)
{
	return (offset != APIC_OFFSET_DFR) && (offset != APIC_OFFSET_ICR_HI);
}

static bool apicv_advanced_x2apic_read_msr_may_valid(uint32_t offset)
{
	return (offset == APIC_OFFSET_TIMER_CCR);
}

static bool apicv_basic_x2apic_write_msr_may_valid(uint32_t offset)
{
	return (offset != APIC_OFFSET_DFR) && (offset != APIC_OFFSET_ICR_HI);
}

static bool apicv_advanced_x2apic_write_msr_may_valid(uint32_t offset)
{
	return (offset != APIC_OFFSET_DFR) && (offset != APIC_OFFSET_ICR_HI) &&
		(offset != APIC_OFFSET_EOI) && (offset != APIC_OFFSET_SELF_IPI);
}

int32_t vlapic_x2apic_read(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *val)
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
		if (is_lapic_pt_configured(vcpu->vm)) {
			switch (msr) {
			case MSR_IA32_EXT_APIC_LDR:
			case MSR_IA32_EXT_XAPICID:
				offset = x2apic_msr_to_regoff(msr);
				error = vlapic_read(vlapic, offset, val);
				break;
			default:
				pr_err("%s: unexpected MSR[0x%x] read with lapic_pt", __func__, msr);
				break;
			}
		} else {
			offset = x2apic_msr_to_regoff(msr);
			if (vlapic->ops->x2apic_read_msr_may_valid(offset)) {
				error = vlapic_read(vlapic, offset, val);
			}
		}
	}

	return error;
}

int32_t vlapic_x2apic_write(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t val)
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
		if (is_lapic_pt_configured(vcpu->vm)) {
			switch (msr) {
			case MSR_IA32_EXT_APIC_ICR:
				error = vlapic_x2apic_pt_icr_access(vcpu->vm, val);
				break;
			default:
				pr_err("%s: unexpected MSR[0x%x] write with lapic_pt", __func__, msr);
				break;
			}
		} else {
			offset = x2apic_msr_to_regoff(msr);
			if (vlapic->ops->x2apic_write_msr_may_valid(offset)) {
				error = vlapic_write(vlapic, offset, val);
			}
		}
	}

	return error;
}

/**
 *  @pre vcpu != NULL
 */
void vlapic_create(struct acrn_vcpu *vcpu, uint16_t pcpu_id)
{
	struct acrn_vlapic *vlapic = vcpu_vlapic(vcpu);

	if (is_vcpu_bsp(vcpu)) {
		uint64_t *pml4_page =
			(uint64_t *)vcpu->vm->arch_vm.nworld_eptp;
		/* only need unmap it from SOS as UOS never mapped it */
		if (is_sos_vm(vcpu->vm)) {
			ept_del_mr(vcpu->vm, pml4_page,
				DEFAULT_APIC_BASE, PAGE_SIZE);
		}

		ept_add_mr(vcpu->vm, pml4_page,
			vlapic_apicv_get_apic_access_addr(),
			DEFAULT_APIC_BASE, PAGE_SIZE,
			EPT_WR | EPT_RD | EPT_UNCACHED);
	}

	vlapic_init_timer(vlapic);

	if (is_sos_vm(vcpu->vm)) {
		/*
		 * For SOS_VM type, pLAPIC IDs need to be used because
		 * host ACPI tables are passthru to SOS.
		 * Get APIC ID sequence format from cpu_storage
		 */
		vlapic->vapic_id = per_cpu(lapic_id, pcpu_id);
	} else {
		vlapic->vapic_id = (uint32_t)vcpu->vcpu_id;
	}

	dev_dbg(DBG_LEVEL_VLAPIC, "vlapic APIC ID : 0x%04x", vlapic->vapic_id);
}

/*
 *  @pre vcpu != NULL
 */
void vlapic_free(struct acrn_vcpu *vcpu)
{
	struct acrn_vlapic *vlapic = vcpu_vlapic(vcpu);

	del_timer(&vlapic->vtimer.timer);

}

/**
 * APIC-v functions
 * @pre get_pi_desc(vlapic2vcpu(vlapic)) != NULL
 */
static bool
apicv_set_intr_ready(struct acrn_vlapic *vlapic, uint32_t vector)
{
	struct pi_desc *pid;
	uint32_t idx;
	bool notify = false;

	pid = get_pi_desc(vlapic2vcpu(vlapic));
	idx = vector >> 6U;
	if (!bitmap_test_and_set_lock((uint16_t)(vector & 0x3fU), &pid->pir[idx])) {
		notify = (bitmap_test_and_set_lock(POSTED_INTR_ON, &pid->control.value) == false);
	}
	return notify;
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

static void apicv_basic_inject_intr(struct acrn_vlapic *vlapic,
		bool guest_irq_enabled, bool injected)
{
	uint32_t vector = 0U;

	if (guest_irq_enabled && (!injected)) {
		vlapic_update_ppr(vlapic);
		if (vlapic_find_deliverable_intr(vlapic, &vector)) {
			exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD, VMX_INT_INFO_VALID | vector);
			vlapic_get_deliverable_intr(vlapic, vector);
		}
	}

	vlapic_update_tpr_threshold(vlapic);
}

/*
 * @brief Send a Posted Interrupt to itself.
 *
 * Interrupts are disabled on pCPU at this point of time.
 * Upon the next VMEnter the self-IPI is serviced by the logical processor.
 * Since the IPI vector is Posted Interrupt vector, logical processor syncs
 * PIR to vIRR and updates RVI.
 *
 * @pre get_pi_desc(vlapic->vcpu) != NULL
 */

static void apicv_advanced_inject_intr(struct acrn_vlapic *vlapic,
		__unused bool guest_irq_enabled, __unused bool injected)
{
	struct acrn_vcpu *vcpu = vlapic2vcpu(vlapic);
	struct pi_desc *pid = get_pi_desc(vcpu);
	/*
	 * From SDM Vol3 26.3.2.5:
	 * Once the virtual interrupt is recognized, it will be delivered
	 * in VMX non-root operation immediately after VM entry(including
	 * any specified event injection) completes.
	 *
	 * So the hardware can handle vmcs event injection and
	 * evaluation/delivery of apicv virtual interrupts in one time
	 * vm-entry.
	 *
	 * Here to sync the pending interrupts to irr and update rvi
	 * self-IPI with Posted Interrupt Notification Vector is sent.
	 */
	if (bitmap_test(POSTED_INTR_ON, &(pid->control.value))) {
		apicv_trigger_pi_anv(pcpuid_from_vcpu(vcpu), (uint32_t)(vcpu->arch.pid.control.bits.nv));
	}
}

void vlapic_inject_intr(struct acrn_vlapic *vlapic, bool guest_irq_enabled, bool injected)
{
	vlapic->ops->inject_intr(vlapic, guest_irq_enabled, injected);
}

static bool apicv_basic_has_pending_delivery_intr(struct acrn_vcpu *vcpu)
{
	uint32_t vector;
	struct acrn_vlapic *vlapic = vcpu_vlapic(vcpu);

	vlapic_update_ppr(vlapic);

	/* check and raise request if we have a deliverable irq in LAPIC IRR */
	if (vlapic_find_deliverable_intr(vlapic, &vector)) {
		/* we have pending IRR */
		vcpu_make_request(vcpu, ACRN_REQUEST_EVENT);
	}

	return vcpu->arch.pending_req != 0UL;
}

static bool apicv_advanced_has_pending_delivery_intr(__unused struct acrn_vcpu *vcpu)
{
	return false;
}

bool vlapic_has_pending_delivery_intr(struct acrn_vcpu *vcpu)
{
	struct acrn_vlapic *vlapic = vcpu_vlapic(vcpu);
	return vlapic->ops->has_pending_delivery_intr(vcpu);
}

static bool apicv_basic_has_pending_intr(struct acrn_vcpu *vcpu)
{
	struct acrn_vlapic *vlapic = vcpu_vlapic(vcpu);
	uint32_t vector;

	vector = vlapic_find_highest_irr(vlapic);

	return vector != 0UL;
}

static bool apicv_advanced_has_pending_intr(struct acrn_vcpu *vcpu)
{
	return apicv_basic_has_pending_intr(vcpu);
}

bool vlapic_has_pending_intr(struct acrn_vcpu *vcpu)
{
	struct acrn_vlapic *vlapic = vcpu_vlapic(vcpu);
	return vlapic->ops->has_pending_intr(vcpu);
}

static bool apicv_basic_apic_read_access_may_valid(__unused uint32_t offset)
{
	return true;
}

static bool apicv_advanced_apic_read_access_may_valid(uint32_t offset)
{
	return ((offset == APIC_OFFSET_CMCI_LVT) || (offset == APIC_OFFSET_TIMER_CCR));
}

static bool apicv_basic_apic_write_access_may_valid(uint32_t offset)
{
	return (offset != APIC_OFFSET_SELF_IPI);
}

static bool apicv_advanced_apic_write_access_may_valid(uint32_t offset)
{
	return (offset == APIC_OFFSET_CMCI_LVT);
}

int32_t apic_access_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t err;
	uint32_t offset;
	uint64_t qual, access_type;
	struct acrn_vlapic *vlapic;
	struct mmio_request *mmio;

	qual = vcpu->arch.exit_qualification;
	access_type = apic_access_type(qual);

	/*
	 * We only support linear access for a data read/write during instruction execution.
	 * for other access types:
	 * a) we don't support vLAPIC work in real mode;
	 * 10 = guest-physical access during event delivery
	 * 15 = guest-physical access for an instruction fetch or during instruction execution
	 * b) we don't support fetch from APIC-access page since its memory type is UC;
	 * 2 = linear access for an instruction fetch
	 * c) we suppose the guest goes wrong when it will access the APIC-access page
	 * when process event-delivery. According chap 26.5.1.2 VM Exits During Event Injection,
	 * vol 3, sdm: If the "virtualize APIC accesses" VM-execution control is 1 and
	 * event delivery generates an access to the APIC-access page, that access is treated as
	 * described in Section 29.4 and may cause a VM exit.
	 * 3 = linear access (read or write) during event delivery
	 */
	if (((access_type == TYPE_LINEAR_APIC_INST_READ) || (access_type == TYPE_LINEAR_APIC_INST_WRITE)) &&
			(decode_instruction(vcpu) >= 0)) {
		vlapic = vcpu_vlapic(vcpu);
		offset = (uint32_t)apic_access_offset(qual);
		mmio = &vcpu->req.reqs.mmio;
		if (access_type == TYPE_LINEAR_APIC_INST_WRITE) {
			err = emulate_instruction(vcpu);
			if (err == 0) {
				if (vlapic->ops->apic_write_access_may_valid(offset)) {
					(void)vlapic_write(vlapic, offset, mmio->value);
				}
			}
		} else {
			if (vlapic->ops->apic_read_access_may_valid(offset)) {
				(void)vlapic_read(vlapic, offset, &mmio->value);
			} else {
				mmio->value = 0UL;
			}
			err = emulate_instruction(vcpu);
		}
		TRACE_2L(TRACE_VMEXIT_APICV_ACCESS, qual, (uint64_t)vlapic);
	} else {
		pr_err("%s, unhandled access type: %lu\n", __func__, access_type);
		err = -EINVAL;
	}

	return err;
}

int32_t veoi_vmexit_handler(struct acrn_vcpu *vcpu)
{
	struct acrn_vlapic *vlapic = NULL;

	uint32_t vector;
	struct lapic_regs *lapic;
	struct lapic_reg *tmrptr;
	uint32_t idx;

	vcpu_retain_rip(vcpu);

	vlapic = vcpu_vlapic(vcpu);
	lapic = &(vlapic->apic_page);
	vector = (uint32_t)(vcpu->arch.exit_qualification & 0xFFUL);

	tmrptr = &lapic->tmr[0];
	idx = vector >> 5U;

	if (bitmap32_test((uint16_t)(vector & 0x1fU), &tmrptr[idx].v)) {
		/* hook to vIOAPIC */
		vioapic_broadcast_eoi(vcpu->vm, vector);
	}

	TRACE_2L(TRACE_VMEXIT_APICV_VIRT_EOI, vector, 0UL);

	return 0;
}

static void vlapic_x2apic_self_ipi_handler(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;
	uint32_t vector;

	lapic = &(vlapic->apic_page);
	vector = lapic->self_ipi.v & APIC_VECTOR_MASK;
	if (vector < 16U) {
		vlapic_set_error(vlapic, APIC_ESR_SEND_ILLEGAL_VECTOR);
		dev_dbg(DBG_LEVEL_VLAPIC, "Ignoring invalid IPI %u", vector);
	} else {
		vlapic_set_intr(vlapic2vcpu(vlapic), vector, LAPIC_TRIG_EDGE);
	}
}

int32_t apic_write_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint64_t qual;
	int32_t err = 0;
	uint32_t offset;
	struct acrn_vlapic *vlapic = NULL;

	qual = vcpu->arch.exit_qualification;
	offset = (uint32_t)(qual & 0xFFFUL);

	vcpu_retain_rip(vcpu);
	vlapic = vcpu_vlapic(vcpu);

	switch (offset) {
	case APIC_OFFSET_ID:
		/* Force APIC ID as read only */
		break;
	case APIC_OFFSET_LDR:
		vlapic_write_ldr(vlapic);
		break;
	case APIC_OFFSET_DFR:
		vlapic_write_dfr(vlapic);
		break;
	case APIC_OFFSET_SVR:
		vlapic_write_svr(vlapic);
		break;
	case APIC_OFFSET_ESR:
		vlapic_write_esr(vlapic);
		break;
	case APIC_OFFSET_ICR_LOW:
		vlapic_write_icrlo(vlapic);
		break;
	case APIC_OFFSET_CMCI_LVT:
	case APIC_OFFSET_TIMER_LVT:
	case APIC_OFFSET_THERM_LVT:
	case APIC_OFFSET_PERF_LVT:
	case APIC_OFFSET_LINT0_LVT:
	case APIC_OFFSET_LINT1_LVT:
	case APIC_OFFSET_ERROR_LVT:
		vlapic_write_lvt(vlapic, offset);
		break;
	case APIC_OFFSET_TIMER_ICR:
		vlapic_write_icrtmr(vlapic);
		break;
	case APIC_OFFSET_TIMER_DCR:
		vlapic_write_dcr(vlapic);
		break;
	case APIC_OFFSET_SELF_IPI:
		if (is_x2apic_enabled(vlapic)) {
			vlapic_x2apic_self_ipi_handler(vlapic);
			break;
		}
		/* falls through */
	default:
		err = -EACCES;
		pr_err("Unhandled APIC-Write, offset:0x%x", offset);
		break;
	}

	TRACE_2L(TRACE_VMEXIT_APICV_WRITE, offset, 0UL);

	return err;
}

/*
 * TPR threshold (32 bits). Bits 3:0 of this field determine the threshold
 * below which bits 7:4 of VTPR (see Section 29.1.1) cannot fall.
 */
void vlapic_update_tpr_threshold(const struct acrn_vlapic *vlapic)
{
	uint32_t irr, tpr, threshold;

	tpr = vlapic->apic_page.tpr.v;
	tpr = ((tpr & 0xf0U) >> 4U);
	irr = vlapic_find_highest_irr(vlapic);
	irr >>= 4U;
	threshold = (irr > tpr) ? 0U : irr;

	exec_vmwrite32(VMX_TPR_THRESHOLD, threshold);
}

int32_t tpr_below_threshold_vmexit_handler(struct acrn_vcpu *vcpu)
{
	vcpu_make_request(vcpu, ACRN_REQUEST_EVENT);
	vcpu_retain_rip(vcpu);

	return 0;
}

static const struct acrn_apicv_ops apicv_basic_ops = {
	.accept_intr = apicv_basic_accept_intr,
	.inject_intr = apicv_basic_inject_intr,
	.has_pending_delivery_intr = apicv_basic_has_pending_delivery_intr,
	.has_pending_intr = apicv_basic_has_pending_intr,
	.apic_read_access_may_valid = apicv_basic_apic_read_access_may_valid,
	.apic_write_access_may_valid = apicv_basic_apic_write_access_may_valid,
	.x2apic_read_msr_may_valid = apicv_basic_x2apic_read_msr_may_valid,
	.x2apic_write_msr_may_valid = apicv_basic_x2apic_write_msr_may_valid,
};

static const struct acrn_apicv_ops apicv_advanced_ops = {
	.accept_intr = apicv_advanced_accept_intr,
	.inject_intr = apicv_advanced_inject_intr,
	.has_pending_delivery_intr = apicv_advanced_has_pending_delivery_intr,
	.has_pending_intr = apicv_advanced_has_pending_intr,
	.apic_read_access_may_valid  = apicv_advanced_apic_read_access_may_valid,
	.apic_write_access_may_valid  = apicv_advanced_apic_write_access_may_valid,
	.x2apic_read_msr_may_valid  = apicv_advanced_x2apic_read_msr_may_valid,
	.x2apic_write_msr_may_valid  = apicv_advanced_x2apic_write_msr_may_valid,
};

/*
 * set apicv ops for apicv basic mode or apicv advenced mode.
 */
void vlapic_set_apicv_ops(void)
{
	if (is_apicv_advanced_feature_supported()) {
		apicv_ops = &apicv_advanced_ops;
	} else {
		apicv_ops = &apicv_basic_ops;
	}
}
