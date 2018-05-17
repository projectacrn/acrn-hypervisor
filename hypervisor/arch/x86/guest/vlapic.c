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

#define pr_fmt(fmt)	"vlapic: " fmt

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <acrn_hv_defs.h>
#include <bsp_extern.h>
#include <hv_debug.h>

#include "instr_emul_wrapper.h"
#include "instr_emul.h"

#include "vlapic_priv.h"

#define VLAPIC_VERBOS 0
#define	PRIO(x)			((x) >> 4)

#define VLAPIC_VERSION		(16)

#define	APICBASE_RESERVED	0x000002ff
#define	APICBASE_BSP		0x00000100
#define	APICBASE_X2APIC		0x00000400
#define	APICBASE_ENABLED	0x00000800

#define ACRN_DBG_LAPIC	6

#if VLAPIC_VERBOS
#define	VLAPIC_CTR_IRR(vlapic, msg)					\
do {									\
	struct lapic_reg *irrptr = &(vlapic)->apic_page->irr[0];	\
	dev_dbg(ACRN_DBG_LAPIC, msg " irr0 0x%08x", irrptr[0].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " irr1 0x%08x", irrptr[1].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " irr2 0x%08x", irrptr[2].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " irr3 0x%08x", irrptr[3].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " irr4 0x%08x", irrptr[4].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " irr5 0x%08x", irrptr[5].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " irr6 0x%08x", irrptr[6].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " irr7 0x%08x", irrptr[7].val);	\
} while (0)

#define	VLAPIC_CTR_ISR(vlapic, msg)					\
do {									\
	struct lapic_reg *isrptr = &(vlapic)->apic_page->isr[0];	\
	dev_dbg(ACRN_DBG_LAPIC, msg " isr0 0x%08x", isrptr[0].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " isr1 0x%08x", isrptr[1].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " isr2 0x%08x", isrptr[2].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " isr3 0x%08x", isrptr[3].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " isr4 0x%08x", isrptr[4].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " isr5 0x%08x", isrptr[5].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " isr6 0x%08x", isrptr[6].val);	\
	dev_dbg(ACRN_DBG_LAPIC, msg " isr7 0x%08x", isrptr[7].val);	\
} while (0)
#else
#define	VLAPIC_CTR_IRR(vlapic, msg)
#define	VLAPIC_CTR_ISR(vlapic, msg)
#endif

/*APIC-v APIC-access address */
static void *apicv_apic_access_addr;

static int
apicv_set_intr_ready(struct vlapic *vlapic, int vector, bool level);

static int
apicv_pending_intr(struct vlapic *vlapic, int *vecptr);

static void
apicv_set_tmr(struct vlapic *vlapic, int vector, bool level);

static void
apicv_batch_set_tmr(struct vlapic *vlapic);

/*
 * Post an interrupt to the vcpu running on 'hostcpu'. This will use a
 * hardware assist if available (e.g. Posted Interrupt) or fall back to
 * sending an 'ipinum' to interrupt the 'hostcpu'.
 */
static void vlapic_set_error(struct vlapic *vlapic, uint32_t mask);

static int vlapic_timer_expired(void *data);

static struct vlapic *
vm_lapic_from_vcpu_id(struct vm *vm, int vcpu_id)
{
	struct vcpu *vcpu;

	vcpu = vcpu_from_vid(vm, vcpu_id);
	ASSERT(vcpu != NULL, "vm%d, vcpu%d", vm->attr.id, vcpu_id);

	return vcpu->arch_vcpu.vlapic;
}

struct vlapic *
vm_lapic_from_pcpuid(struct vm *vm, int pcpu_id)
{
	struct vcpu *vcpu;

	vcpu = vcpu_from_pid(vm, pcpu_id);
	ASSERT(vcpu != NULL, "vm%d, pcpu%d", vm->attr.id, pcpu_id);

	return vcpu->arch_vcpu.vlapic;
}

static int vm_apicid2vcpu_id(struct vm *vm, uint8_t lapicid)
{
	int i;
	struct vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		if (vlapic_get_apicid(vcpu->arch_vcpu.vlapic) == lapicid)
			return vcpu->vcpu_id;
	}

	pr_err("%s: bad lapicid %d", __func__, lapicid);

	return phy_cpu_num;
}

static uint64_t
vm_active_cpus(struct vm *vm)
{
	uint64_t dmask = 0;
	int i;
	struct vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		bitmap_set(vcpu->vcpu_id, &dmask);
	}

	return dmask;
}

uint32_t
vlapic_get_id(struct vlapic *vlapic)
{
	return vlapic->apic_page->id;
}

uint8_t
vlapic_get_apicid(struct vlapic *vlapic)
{
	return vlapic->apic_page->id >> APIC_ID_SHIFT;
}

static inline uint32_t
vlapic_build_id(struct vlapic *vlapic)
{
	struct vcpu *vcpu = vlapic->vcpu;
	uint32_t id;

	if (is_vm0(vcpu->vm)) {
		/* Get APIC ID sequence format from cpu_storage */
		id = per_cpu(lapic_id, vcpu->vcpu_id);
	} else
		id = vcpu->vcpu_id;

	dev_dbg(ACRN_DBG_LAPIC, "vlapic APIC PAGE ID : 0x%08x",
		(id << APIC_ID_SHIFT));

	return (id << APIC_ID_SHIFT);
}

static void
vlapic_dfr_write_handler(struct vlapic *vlapic)
{
	struct lapic *lapic;

	lapic = vlapic->apic_page;
	lapic->dfr &= APIC_DFR_MODEL_MASK;
	lapic->dfr |= APIC_DFR_RESERVED;

	if ((lapic->dfr & APIC_DFR_MODEL_MASK) == APIC_DFR_MODEL_FLAT)
		dev_dbg(ACRN_DBG_LAPIC, "vlapic DFR in Flat Model");
	else if ((lapic->dfr & APIC_DFR_MODEL_MASK) == APIC_DFR_MODEL_CLUSTER)
		dev_dbg(ACRN_DBG_LAPIC, "vlapic DFR in Cluster Model");
	else
		dev_dbg(ACRN_DBG_LAPIC, "DFR in Unknown Model %#x", lapic->dfr);
}

static void
vlapic_ldr_write_handler(struct vlapic *vlapic)
{
	struct lapic *lapic;

	lapic = vlapic->apic_page;
	lapic->ldr &= ~APIC_LDR_RESERVED;
	dev_dbg(ACRN_DBG_LAPIC, "vlapic LDR set to %#x", lapic->ldr);
}

static void
vlapic_id_write_handler(struct vlapic *vlapic)
{
	struct lapic *lapic;

	/*
	 * We don't allow the ID register to be modified so reset it back to
	 * its default value.
	 */
	lapic = vlapic->apic_page;
	lapic->id = vlapic_get_id(vlapic);
}

static inline bool
vlapic_lvtt_oneshot(struct vlapic *vlapic)
{
	return ((vlapic->apic_page->lvt_timer & APIC_LVTT_TM)
				== APIC_LVTT_TM_ONE_SHOT);
}

static inline bool
vlapic_lvtt_period(struct vlapic *vlapic)
{
	return ((vlapic->apic_page->lvt_timer & APIC_LVTT_TM)
				==  APIC_LVTT_TM_PERIODIC);
}

static inline bool
vlapic_lvtt_tsc_deadline(struct vlapic *vlapic)
{
	return ((vlapic->apic_page->lvt_timer & APIC_LVTT_TM)
				==  APIC_LVTT_TM_TSCDLT);
}

static inline bool
vlapic_lvtt_masked(struct vlapic *vlapic)
{
	return !!(vlapic->apic_page->lvt_timer & APIC_LVTT_M);
}

static void vlapic_create_timer(struct vlapic *vlapic)
{
	struct vlapic_timer *vlapic_timer;

	if (vlapic == NULL)
		return;

	vlapic_timer = &vlapic->vlapic_timer;
	memset(vlapic_timer, 0, sizeof(struct vlapic_timer));

	initialize_timer(&vlapic_timer->timer,
			vlapic_timer_expired, vlapic->vcpu,
			0, 0, 0);
}

static void vlapic_reset_timer(struct vlapic *vlapic)
{
	struct timer *timer;

	if (vlapic == NULL)
		return;

	timer = &vlapic->vlapic_timer.timer;
	del_timer(timer);
	timer->mode = 0;
	timer->fire_tsc = 0;
	timer->period_in_cycle = 0;
}

static void vlapic_update_lvtt(struct vlapic *vlapic,
			uint32_t val)
{
	uint32_t timer_mode = val & APIC_LVTT_TM;
	struct vlapic_timer *vlapic_timer = &vlapic->vlapic_timer;

	if (vlapic_timer->mode != timer_mode) {
		struct timer *timer = &vlapic_timer->timer;

		/*
		 * A write to the LVT Timer Register that changes
		 * the timer mode disarms the local APIC timer.
		 */
		del_timer(timer);
		timer->mode = (timer_mode == APIC_LVTT_TM_PERIODIC) ?
				TICK_MODE_PERIODIC: TICK_MODE_ONESHOT;
		timer->fire_tsc = 0;
		timer->period_in_cycle = 0;

		vlapic_timer->mode = timer_mode;
	}
}

static uint32_t vlapic_get_ccr(__unused struct vlapic *vlapic)
{
	return 0;
}

static void vlapic_dcr_write_handler(__unused struct vlapic *vlapic)
{
}

static void vlapic_icrtmr_write_handler(__unused struct vlapic *vlapic)
{
}


static uint64_t vlapic_get_tsc_deadline_msr(struct vlapic *vlapic)
{
	if (!vlapic_lvtt_tsc_deadline(vlapic))
		return 0;

	return (vlapic->vlapic_timer.timer.fire_tsc == 0) ? 0 :
			vlapic->vcpu->guest_msrs[IDX_TSC_DEADLINE];

}

static void vlapic_set_tsc_deadline_msr(struct vlapic *vlapic,
			uint64_t val)
{
	struct timer *timer;

	if (!vlapic_lvtt_tsc_deadline(vlapic))
		return;

	vlapic->vcpu->guest_msrs[IDX_TSC_DEADLINE] = val;

	timer = &vlapic->vlapic_timer.timer;
	del_timer(timer);

	if (val != 0UL) {
		struct vcpu_arch *arch = &vlapic->vcpu->arch_vcpu;

		/* transfer guest tsc to host tsc */
		val -= arch->contexts[arch->cur_context].tsc_offset;
		timer->fire_tsc = val;

		add_timer(timer);
	} else
		timer->fire_tsc = 0;
}

static void
vlapic_esr_write_handler(struct vlapic *vlapic)
{
	struct lapic *lapic;

	lapic = vlapic->apic_page;
	lapic->esr = vlapic->esr_pending;
	vlapic->esr_pending = 0;
}

/*
 * Returns 1 if the vcpu needs to be notified of the interrupt and 0 otherwise.
 */
static int
vlapic_set_intr_ready(struct vlapic *vlapic, int vector, bool level)
{
	struct lapic *lapic;
	struct lapic_reg *irrptr, *tmrptr;
	uint32_t mask;
	int idx;

	ASSERT((vector >= 0) && (vector <= NR_MAX_VECTOR),
		"invalid vector %d", vector);

	lapic = vlapic->apic_page;
	if (!(lapic->svr & APIC_SVR_ENABLE)) {
		dev_dbg(ACRN_DBG_LAPIC,
			"vlapic is software disabled, ignoring interrupt %d",
			vector);
		return 0;
	}

	if (vector < 16) {
		vlapic_set_error(vlapic, APIC_ESR_RECEIVE_ILLEGAL_VECTOR);
		dev_dbg(ACRN_DBG_LAPIC,
			"vlapic ignoring interrupt to vector %d", vector);
		return 1;
	}

	if (vlapic->ops.apicv_set_intr_ready)
		return (*vlapic->ops.apicv_set_intr_ready)
			(vlapic, vector, level);

	idx = vector / 32;
	mask = 1 << (vector % 32);

	irrptr = &lapic->irr[0];
	atomic_set_int(&irrptr[idx].val, mask);

	/*
	 * Verify that the trigger-mode of the interrupt matches with
	 * the vlapic TMR registers.
	 */
	tmrptr = &lapic->tmr[0];
	if ((tmrptr[idx].val & mask) != (level ? mask : 0)) {
		dev_dbg(ACRN_DBG_LAPIC,
		"vlapic TMR[%d] is 0x%08x but interrupt is %s-triggered",
		idx, tmrptr[idx].val, level ? "level" : "edge");
	}

	VLAPIC_CTR_IRR(vlapic, "vlapic_set_intr_ready");
	return 1;
}

static inline uint32_t *
vlapic_get_lvtptr(struct vlapic *vlapic, uint32_t offset)
{
	struct lapic *lapic = vlapic->apic_page;
	int i;

	switch (offset) {
	case APIC_OFFSET_CMCI_LVT:
		return &lapic->lvt_cmci;
	case APIC_OFFSET_TIMER_LVT ... APIC_OFFSET_ERROR_LVT:
		i = (offset - APIC_OFFSET_TIMER_LVT) >> 2;
		return (&lapic->lvt_timer) + i;
	default:
		panic("vlapic_get_lvt: invalid LVT\n");
	}
}

static inline int
lvt_off_to_idx(uint32_t offset)
{
	int index;

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
		index = APIC_LVT_ERROR;
		break;
	default:
		index = -1;
		break;
	}
	ASSERT(index >= 0 && index <= VLAPIC_MAXLVT_INDEX,
		"%s: invalid lvt index %d for offset %#x",
		__func__, index, offset);

	return index;
}

static inline uint32_t
vlapic_get_lvt(struct vlapic *vlapic, uint32_t offset)
{
	int idx;
	uint32_t val;

	idx = lvt_off_to_idx(offset);
	val = atomic_load((int *)&vlapic->lvt_last[idx]);
	return val;
}

static void
vlapic_lvt_write_handler(struct vlapic *vlapic, uint32_t offset)
{
	uint32_t *lvtptr, mask, val;
	struct lapic *lapic;
	int idx;

	lapic = vlapic->apic_page;
	lvtptr = vlapic_get_lvtptr(vlapic, offset);
	val = *lvtptr;
	idx = lvt_off_to_idx(offset);

	if (!(lapic->svr & APIC_SVR_ENABLE))
		val |= APIC_LVT_M;
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
	if (offset == APIC_OFFSET_LINT0_LVT &&
		((val & APIC_LVT_DM) == APIC_LVT_DM_EXTINT)) {
		uint32_t last = vlapic_get_lvt(vlapic, offset);

		/* mask -> unmask: may from every vlapic in the vm */
		if ((last & APIC_LVT_M) && ((val & APIC_LVT_M) == 0)) {
			if (vlapic->vm->vpic_wire_mode == VPIC_WIRE_INTR ||
				vlapic->vm->vpic_wire_mode == VPIC_WIRE_NULL) {
				vlapic->vm->vpic_wire_mode = VPIC_WIRE_LAPIC;
				dev_dbg(ACRN_DBG_LAPIC,
					"vpic wire mode -> LAPIC");
			} else {
				pr_err("WARNING:invalid vpic wire mode change");
				return;
			}
		/* unmask -> mask: only from the vlapic LINT0-ExtINT enabled */
		} else if (((last & APIC_LVT_M) == 0) && (val & APIC_LVT_M)) {
			if (vlapic->vm->vpic_wire_mode == VPIC_WIRE_LAPIC) {
				vlapic->vm->vpic_wire_mode = VPIC_WIRE_NULL;
				dev_dbg(ACRN_DBG_LAPIC,
						"vpic wire mode -> NULL");
			}
		}
	} else if (offset == APIC_OFFSET_TIMER_LVT)
		vlapic_update_lvtt(vlapic, val);

	*lvtptr = val;
	atomic_store((int *)&vlapic->lvt_last[idx], val);
}

static void
vlapic_mask_lvts(struct vlapic *vlapic)
{
	struct lapic *lapic = vlapic->apic_page;

	lapic->lvt_cmci |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_CMCI_LVT);

	lapic->lvt_timer |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_TIMER_LVT);

	lapic->lvt_thermal |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_THERM_LVT);

	lapic->lvt_pcint |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_PERF_LVT);

	lapic->lvt_lint0 |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_LINT0_LVT);

	lapic->lvt_lint1 |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_LINT1_LVT);

	lapic->lvt_error |= APIC_LVT_M;
	vlapic_lvt_write_handler(vlapic, APIC_OFFSET_ERROR_LVT);
}

static int
vlapic_fire_lvt(struct vlapic *vlapic, uint32_t lvt)
{
	uint32_t vec, mode;

	if (lvt & APIC_LVT_M)
		return 0;

	vec = lvt & APIC_LVT_VECTOR;
	mode = lvt & APIC_LVT_DM;

	switch (mode) {
	case APIC_LVT_DM_FIXED:
		if (vec < 16) {
			vlapic_set_error(vlapic, APIC_ESR_SEND_ILLEGAL_VECTOR);
			return 0;
		}
		if (vlapic_set_intr_ready(vlapic, vec, false))
			vcpu_make_request(vlapic->vcpu, ACRN_REQUEST_EVENT);
		break;
	case APIC_LVT_DM_NMI:
		vcpu_inject_nmi(vlapic->vcpu);
		break;
	case APIC_LVT_DM_EXTINT:
		vcpu_inject_extint(vlapic->vcpu);
		break;
	default:
		/* Other modes ignored */
		return 0;
	}
	return 1;
}

static void
dump_isrvec_stk(struct vlapic *vlapic)
{
	int i;
	struct lapic_reg *isrptr;

	isrptr = &vlapic->apic_page->isr[0];
	for (i = 0; i < 8; i++)
		printf("ISR%d 0x%08x\n", i, isrptr[i].val);

	for (i = 0; i <= vlapic->isrvec_stk_top; i++)
		printf("isrvec_stk[%d] = %d\n", i, vlapic->isrvec_stk[i]);
}

/*
 * Algorithm adopted from section "Interrupt, Task and Processor Priority"
 * in Intel Architecture Manual Vol 3a.
 */
static void
vlapic_update_ppr(struct vlapic *vlapic)
{
	int isrvec, tpr, ppr;

	/*
	 * Note that the value on the stack at index 0 is always 0.
	 *
	 * This is a placeholder for the value of ISRV when none of the
	 * bits is set in the ISRx registers.
	 */
	isrvec = vlapic->isrvec_stk[vlapic->isrvec_stk_top];
	tpr = vlapic->apic_page->tpr;

	/* update ppr */
	{
		int i, lastprio, curprio, vector, idx;
		struct lapic_reg *isrptr;

		if (vlapic->isrvec_stk_top == 0 && isrvec != 0)
			panic("isrvec_stk is corrupted: %d", isrvec);

		/*
		 * Make sure that the priority of the nested interrupts is
		 * always increasing.
		 */
		lastprio = -1;
		for (i = 1; i <= vlapic->isrvec_stk_top; i++) {
			curprio = PRIO(vlapic->isrvec_stk[i]);
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
		i = 1;
		isrptr = &vlapic->apic_page->isr[0];
		for (vector = 0; vector < 256; vector++) {
			idx = vector / 32;
			if (isrptr[idx].val & (1 << (vector % 32))) {
				if ((i > vlapic->isrvec_stk_top) ||
					((i < ISRVEC_STK_SIZE) &&
					(vlapic->isrvec_stk[i] != vector))) {
					dump_isrvec_stk(vlapic);
					panic("ISR and isrvec_stk out of sync");
				}
				i++;
			}
		}
	}

	if (PRIO(tpr) >= PRIO(isrvec))
		ppr = tpr;
	else
		ppr = isrvec & 0xf0;

	vlapic->apic_page->ppr = ppr;
	dev_dbg(ACRN_DBG_LAPIC, "%s 0x%02x", __func__, ppr);
}

static void
vlapic_process_eoi(struct vlapic *vlapic)
{
	struct lapic *lapic = vlapic->apic_page;
	struct lapic_reg *isrptr, *tmrptr;
	int i, bitpos, vector;

	isrptr = &lapic->isr[0];
	tmrptr = &lapic->tmr[0];

	for (i = 7; i >= 0; i--) {
		bitpos = fls(isrptr[i].val);
		if (bitpos >= 0) {
			if (vlapic->isrvec_stk_top <= 0) {
				panic("invalid vlapic isrvec_stk_top %d",
					vlapic->isrvec_stk_top);
			}
			isrptr[i].val &= ~(1 << bitpos);
			vector = i * 32 + bitpos;
			dev_dbg(ACRN_DBG_LAPIC, "EOI vector %d", vector);
			VLAPIC_CTR_ISR(vlapic, "vlapic_process_eoi");
			vlapic->isrvec_stk_top--;
			vlapic_update_ppr(vlapic);
			if ((tmrptr[i].val & (1 << bitpos)) != 0) {
				/* hook to vIOAPIC */
				vioapic_process_eoi(vlapic->vm, vector);
			}
			return;
		}
	}
	dev_dbg(ACRN_DBG_LAPIC, "Gratuitous EOI");
}

static void
vlapic_set_error(struct vlapic *vlapic, uint32_t mask)
{
	uint32_t lvt;

	vlapic->esr_pending |= mask;
	if (vlapic->esr_firing)
		return;
	vlapic->esr_firing = 1;

	/* The error LVT always uses the fixed delivery mode. */
	lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_ERROR_LVT);
	vlapic_fire_lvt(vlapic, lvt | APIC_LVT_DM_FIXED);
	vlapic->esr_firing = 0;
}

static int
vlapic_trigger_lvt(struct vlapic *vlapic, int vector)
{
	uint32_t lvt;

	if (vlapic_enabled(vlapic) == false) {
		/*
		 * When the local APIC is global/hardware disabled,
		 * LINT[1:0] pins are configured as INTR and NMI pins,
		 * respectively.
		 */
		switch (vector) {
		case APIC_LVT_LINT0:
			vcpu_inject_extint(vlapic->vcpu);
			break;
		case APIC_LVT_LINT1:
			vcpu_inject_nmi(vlapic->vcpu);
			break;
		default:
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
	vlapic_fire_lvt(vlapic, lvt);
	return 0;
}

/*
 * This function populates 'dmask' with the set of vcpus that match the
 * addressing specified by the (dest, phys, lowprio) tuple.
 */
static void
vlapic_calcdest(struct vm *vm, uint64_t *dmask, uint32_t dest,
		bool phys, bool lowprio)
{
	struct vlapic *vlapic;
	struct vlapic *target = NULL;
	uint32_t dfr, ldr, ldest, cluster;
	uint32_t mda_flat_ldest, mda_cluster_ldest, mda_ldest, mda_cluster_id;
	uint64_t amask;
	int vcpu_id;

	if (dest == 0xff) {
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
		*dmask = 0;
		vcpu_id = vm_apicid2vcpu_id(vm, dest);
		if (vcpu_id < phy_cpu_num)
			bitmap_set(vcpu_id, dmask);
	} else {
		/*
		 * In the "Flat Model" the MDA is interpreted as an 8-bit wide
		 * bitmask. This model is only available in the xAPIC mode.
		 */
		mda_flat_ldest = dest & 0xff;

		/*
		 * In the "Cluster Model" the MDA is used to identify a
		 * specific cluster and a set of APICs in that cluster.
		 */
		mda_cluster_id = (dest >> 4) & 0xf;
		mda_cluster_ldest = dest & 0xf;

		/*
		 * Logical mode: match each APIC that has a bit set
		 * in its LDR that matches a bit in the ldest.
		 */
		*dmask = 0;
		amask = vm_active_cpus(vm);
		while ((vcpu_id = bitmap_ffs(&amask)) >= 0) {
			bitmap_clr(vcpu_id, &amask);

			vlapic = vm_lapic_from_vcpu_id(vm, vcpu_id);
			dfr = vlapic->apic_page->dfr;
			ldr = vlapic->apic_page->ldr;

			if ((dfr & APIC_DFR_MODEL_MASK) ==
					APIC_DFR_MODEL_FLAT) {
				ldest = ldr >> 24;
				mda_ldest = mda_flat_ldest;
			} else if ((dfr & APIC_DFR_MODEL_MASK) ==
					APIC_DFR_MODEL_CLUSTER) {

				cluster = ldr >> 28;
				ldest = (ldr >> 24) & 0xf;

				if (cluster != mda_cluster_id)
					continue;
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

			if ((mda_ldest & ldest) != 0) {
				if (lowprio) {
					if (target == NULL)
						target = vlapic;
					else if (target->apic_page->ppr >
						vlapic->apic_page->ppr)
						target = vlapic;
				} else {
					bitmap_set(vcpu_id, dmask);
				}
			}
		}

		if (lowprio && (target != NULL))
			bitmap_set(target->vcpu->vcpu_id, dmask);
	}
}

void
calcvdest(struct vm *vm, uint64_t *dmask, uint32_t dest, bool phys)
{
	vlapic_calcdest(vm, dmask, dest, phys, false);
}

static void
vlapic_set_tpr(struct vlapic *vlapic, uint8_t val)
{
	struct lapic *lapic = vlapic->apic_page;

	if (lapic->tpr != val) {
		dev_dbg(ACRN_DBG_LAPIC,
			"vlapic TPR changed from %#x to %#x", lapic->tpr, val);
		lapic->tpr = val;
		vlapic_update_ppr(vlapic);
	}
}

static uint8_t
vlapic_get_tpr(struct vlapic *vlapic)
{
	struct lapic *lapic = vlapic->apic_page;

	return lapic->tpr;
}

void
vlapic_set_cr8(struct vlapic *vlapic, uint64_t val)
{
	uint8_t tpr;

	if (val & ~0xf) {
		vcpu_inject_gp(vlapic->vcpu);
		return;
	}

	tpr = val << 4;
	vlapic_set_tpr(vlapic, tpr);
}

uint64_t
vlapic_get_cr8(struct vlapic *vlapic)
{
	uint8_t tpr;

	tpr = vlapic_get_tpr(vlapic);
	return tpr >> 4;
}

static int
vlapic_icrlo_write_handler(struct vlapic *vlapic)
{
	int i;
	bool phys;
	uint64_t dmask;
	uint64_t icrval;
	uint32_t dest, vec, mode;
	struct lapic *lapic;
	struct vcpu *target_vcpu;
	uint32_t target_vcpu_id;

	lapic = vlapic->apic_page;
	lapic->icr_lo &= ~APIC_DELSTAT_PEND;
	icrval = ((uint64_t)lapic->icr_hi << 32) | lapic->icr_lo;

	dest = icrval >> (32 + 24);
	vec = icrval & APIC_VECTOR_MASK;
	mode = icrval & APIC_DELMODE_MASK;
	phys = ((icrval & APIC_DESTMODE_LOG) == 0);

	if (mode == APIC_DELMODE_FIXED && vec < 16) {
		vlapic_set_error(vlapic, APIC_ESR_SEND_ILLEGAL_VECTOR);
		dev_dbg(ACRN_DBG_LAPIC, "Ignoring invalid IPI %d", vec);
		return 0;
	}

	dev_dbg(ACRN_DBG_LAPIC,
		"icrlo 0x%016llx triggered ipi %d", icrval, vec);

	if (mode == APIC_DELMODE_FIXED || mode == APIC_DELMODE_NMI) {
		switch (icrval & APIC_DEST_MASK) {
		case APIC_DEST_DESTFLD:
			vlapic_calcdest(vlapic->vm, &dmask, dest, phys, false);
			break;
		case APIC_DEST_SELF:
			bitmap_setof(vlapic->vcpu->vcpu_id, &dmask);
			break;
		case APIC_DEST_ALLISELF:
			dmask = vm_active_cpus(vlapic->vm);
			break;
		case APIC_DEST_ALLESELF:
			dmask = vm_active_cpus(vlapic->vm);
			bitmap_clr(vlapic->vcpu->vcpu_id, &dmask);
			break;
		default:
			dmask = 0;	/* satisfy gcc */
			break;
		}

		while ((i = bitmap_ffs(&dmask)) >= 0) {
			bitmap_clr(i, &dmask);
			target_vcpu = vcpu_from_vid(vlapic->vm, i);
			if (target_vcpu == NULL)
				return 0;

			if (mode == APIC_DELMODE_FIXED) {
				vlapic_set_intr(target_vcpu, vec,
					LAPIC_TRIG_EDGE);
				dev_dbg(ACRN_DBG_LAPIC,
					"vlapic sending ipi %d to vcpu_id %d",
					vec, i);
			} else {
				vcpu_inject_nmi(target_vcpu);
				dev_dbg(ACRN_DBG_LAPIC,
					"vlapic send ipi nmi to vcpu_id %d", i);
			}
		}

		return 0;	/* handled completely in the kernel */
	}

	if (phys) {
		/* INIT/SIPI is sent in Physical mode with LAPIC ID as its
		 * destination, so the dest need to be changed to VCPU ID;
		 */
		target_vcpu_id = vm_apicid2vcpu_id(vlapic->vm, dest);
		target_vcpu = vcpu_from_vid(vlapic->vm, target_vcpu_id);
		if (target_vcpu == NULL) {
			pr_err("Target VCPU not found");
			return 0;
		}

		if (mode == APIC_DELMODE_INIT) {
			if ((icrval & APIC_LEVEL_MASK) == APIC_LEVEL_DEASSERT)
				return 0;

			dev_dbg(ACRN_DBG_LAPIC,
				"Sending INIT from VCPU %d to %d",
				vlapic->vcpu->vcpu_id, target_vcpu_id);

			/* put target vcpu to INIT state and wait for SIPI */
			pause_vcpu(target_vcpu, VCPU_PAUSED);
			reset_vcpu(target_vcpu);
			/* new cpu model only need one SIPI to kick AP run,
			 * the second SIPI will be ignored as it move out of
			 * wait-for-SIPI state.
			 */
			target_vcpu->arch_vcpu.nr_sipi = 1;

			return 0;
		}

		if (mode == APIC_DELMODE_STARTUP) {

			/* Ignore SIPIs in any state other than wait-for-SIPI */
			if ((target_vcpu->state != VCPU_INIT) ||
					(target_vcpu->arch_vcpu.nr_sipi == 0))
				return 0;

			dev_dbg(ACRN_DBG_LAPIC,
				"Sending SIPI from VCPU %d to %d with vector %d",
				vlapic->vcpu->vcpu_id, target_vcpu_id, vec);

			if (--target_vcpu->arch_vcpu.nr_sipi > 0)
				return 0;

			target_vcpu->arch_vcpu.cpu_mode = REAL_MODE;
			target_vcpu->arch_vcpu.sipi_vector = vec;
			pr_err("Start Secondary VCPU%d for VM[%d]...",
					target_vcpu->vcpu_id,
					target_vcpu->vm->attr.id);
			schedule_vcpu(target_vcpu);

			return 0;
		}
	}

	/*
	 * This will cause a return to userland.
	 */
	return 1;
}

int
vlapic_pending_intr(struct vlapic *vlapic, int *vecptr)
{
	struct lapic *lapic = vlapic->apic_page;
	int i, bitpos;
	uint32_t vector;
	uint32_t val;
	struct lapic_reg *irrptr;

	if (vlapic->ops.apicv_pending_intr)
		return (*vlapic->ops.apicv_pending_intr)(vlapic, vecptr);

	irrptr = &lapic->irr[0];

	for (i = 7; i >= 0; i--) {
		val = atomic_load((int *)&irrptr[i].val);
		bitpos = fls(val);
		if (bitpos >= 0) {
			vector = i * 32 + bitpos;
			if (PRIO(vector) > PRIO(lapic->ppr)) {
				if (vecptr != NULL)
					*vecptr = vector;
				return 1;
			}
			break;
		}
	}
	return 0;
}

void
vlapic_intr_accepted(struct vlapic *vlapic, int vector)
{
	struct lapic *lapic = vlapic->apic_page;
	struct lapic_reg *irrptr, *isrptr;
	int idx, stk_top;

	if (vlapic->ops.apicv_intr_accepted)
		return (*vlapic->ops.apicv_intr_accepted)(vlapic, vector);

	/*
	 * clear the ready bit for vector being accepted in irr
	 * and set the vector as in service in isr.
	 */
	idx = vector / 32;

	irrptr = &lapic->irr[0];
	atomic_clear_int(&irrptr[idx].val, 1 << (vector % 32));
	VLAPIC_CTR_IRR(vlapic, "vlapic_intr_accepted");

	isrptr = &lapic->isr[0];
	isrptr[idx].val |= 1 << (vector % 32);
	VLAPIC_CTR_ISR(vlapic, "vlapic_intr_accepted");

	/*
	 * Update the PPR
	 */
	vlapic->isrvec_stk_top++;

	stk_top = vlapic->isrvec_stk_top;
	if (stk_top >= ISRVEC_STK_SIZE)
		panic("isrvec_stk_top overflow %d", stk_top);

	vlapic->isrvec_stk[stk_top] = vector;
	vlapic_update_ppr(vlapic);
}

static void
vlapic_svr_write_handler(struct vlapic *vlapic)
{
	struct lapic *lapic;
	uint32_t old, new, changed;

	lapic = vlapic->apic_page;

	new = lapic->svr;
	old = vlapic->svr_last;
	vlapic->svr_last = new;

	changed = old ^ new;
	if ((changed & APIC_SVR_ENABLE) != 0) {
		if ((new & APIC_SVR_ENABLE) == 0) {
			/*
			 * TODO: The apic is now disabled so stop the apic timer
			 * and mask all the LVT entries.
			 */
			dev_dbg(ACRN_DBG_LAPIC, "vlapic is software-disabled");
			vlapic_mask_lvts(vlapic);
			/* the only one enabled LINT0-ExtINT vlapic disabled */
			if (vlapic->vm->vpic_wire_mode == VPIC_WIRE_NULL) {
				vlapic->vm->vpic_wire_mode = VPIC_WIRE_INTR;
				dev_dbg(ACRN_DBG_LAPIC,
					"vpic wire mode -> INTR");
			}
		} else {
			/*
			 * The apic is now enabled so restart the apic timer
			 * if it is configured in periodic mode.
			 */
			dev_dbg(ACRN_DBG_LAPIC, "vlapic is software-enabled");
			if (vlapic_lvtt_period(vlapic))
				vlapic_icrtmr_write_handler(vlapic);
		}
	}
}

static int
vlapic_read(struct vlapic *vlapic, int mmio_access, uint64_t offset,
		uint64_t *data)
{
	struct lapic *lapic = vlapic->apic_page;
	int i;

	if (!mmio_access) {
		/*
		 * XXX Generate GP fault for MSR accesses in xAPIC mode
		 */
		dev_dbg(ACRN_DBG_LAPIC,
			"x2APIC MSR read from offset %#lx in xAPIC mode",
			offset);
		*data = 0;
		goto done;
	}

	if (offset > sizeof(*lapic)) {
		*data = 0;
		goto done;
	}

	offset &= ~3;
	switch (offset) {
	case APIC_OFFSET_ID:
		*data = lapic->id;
		break;
	case APIC_OFFSET_VER:
		*data = lapic->version;
		break;
	case APIC_OFFSET_TPR:
		*data = vlapic_get_tpr(vlapic);
		break;
	case APIC_OFFSET_APR:
		*data = lapic->apr;
		break;
	case APIC_OFFSET_PPR:
		*data = lapic->ppr;
		break;
	case APIC_OFFSET_EOI:
		*data = lapic->eoi;
		break;
	case APIC_OFFSET_LDR:
		*data = lapic->ldr;
		break;
	case APIC_OFFSET_DFR:
		*data = lapic->dfr;
		break;
	case APIC_OFFSET_SVR:
		*data = lapic->svr;
		break;
	case APIC_OFFSET_ISR0 ... APIC_OFFSET_ISR7:
		i = (offset - APIC_OFFSET_ISR0) >> 4;
		*data = lapic->isr[i].val;
		break;
	case APIC_OFFSET_TMR0 ... APIC_OFFSET_TMR7:
		i = (offset - APIC_OFFSET_TMR0) >> 4;
		*data = lapic->tmr[i].val;
		break;
	case APIC_OFFSET_IRR0 ... APIC_OFFSET_IRR7:
		i = (offset - APIC_OFFSET_IRR0) >> 4;
		*data = lapic->irr[i].val;
		break;
	case APIC_OFFSET_ESR:
		*data = lapic->esr;
		break;
	case APIC_OFFSET_ICR_LOW:
		*data = lapic->icr_lo;
		break;
	case APIC_OFFSET_ICR_HI:
		*data = lapic->icr_hi;
		break;
	case APIC_OFFSET_CMCI_LVT:
	case APIC_OFFSET_TIMER_LVT ... APIC_OFFSET_ERROR_LVT:
		*data = vlapic_get_lvt(vlapic, offset);
#ifdef INVARIANTS
		reg = vlapic_get_lvtptr(vlapic, offset);
		ASSERT(*data == *reg,
			"inconsistent lvt value at offset %#lx: %#lx/%#x",
			offset, *data, *reg);
#endif
		break;
	case APIC_OFFSET_TIMER_ICR:
		/* if TSCDEADLINE mode always return 0*/
		if (vlapic_lvtt_tsc_deadline(vlapic))
			*data = 0;
		else
			*data = vlapic_get_ccr(vlapic);
		break;
	case APIC_OFFSET_TIMER_CCR:
		/* TODO */
		*data = vlapic_get_ccr(vlapic);
		break;
	case APIC_OFFSET_TIMER_DCR:
		*data = lapic->dcr_timer;
		break;
	case APIC_OFFSET_SELF_IPI:
		/*
		 * XXX generate a GP fault if vlapic is in x2apic mode
		 */
		*data = 0;
		break;
	case APIC_OFFSET_RRR:
	default:
		*data = 0;
		break;
	}
done:
	dev_dbg(ACRN_DBG_LAPIC,
			"vlapic read offset %#x, data %#lx", offset, *data);
	return 0;
}

static int
vlapic_write(struct vlapic *vlapic, int mmio_access, uint64_t offset,
		uint64_t data)
{
	struct lapic *lapic = vlapic->apic_page;
	uint32_t *regptr;
	int retval;

	ASSERT((offset & 0xf) == 0 && offset < CPU_PAGE_SIZE,
		"%s: invalid offset %#lx", __func__, offset);

	dev_dbg(ACRN_DBG_LAPIC, "vlapic write offset %#lx, data %#lx",
		offset, data);

	if (offset > sizeof(*lapic))
		return 0;

	/*
	 * XXX Generate GP fault for MSR accesses in xAPIC mode
	 */
	if (!mmio_access) {
		dev_dbg(ACRN_DBG_LAPIC,
			"x2APIC MSR write of %#lx to offset %#lx in xAPIC mode",
			data, offset);
		return 0;
	}

	retval = 0;
	switch (offset) {
	case APIC_OFFSET_ID:
		lapic->id = data;
		vlapic_id_write_handler(vlapic);
		break;
	case APIC_OFFSET_TPR:
		vlapic_set_tpr(vlapic, data & 0xff);
		break;
	case APIC_OFFSET_EOI:
		vlapic_process_eoi(vlapic);
		break;
	case APIC_OFFSET_LDR:
		lapic->ldr = data;
		vlapic_ldr_write_handler(vlapic);
		break;
	case APIC_OFFSET_DFR:
		lapic->dfr = data;
		vlapic_dfr_write_handler(vlapic);
		break;
	case APIC_OFFSET_SVR:
		lapic->svr = data;
		vlapic_svr_write_handler(vlapic);
		break;
	case APIC_OFFSET_ICR_LOW:
		lapic->icr_lo = data;
		retval = vlapic_icrlo_write_handler(vlapic);
		break;
	case APIC_OFFSET_ICR_HI:
		lapic->icr_hi = data;
		break;
	case APIC_OFFSET_CMCI_LVT:
	case APIC_OFFSET_TIMER_LVT ... APIC_OFFSET_ERROR_LVT:
		regptr = vlapic_get_lvtptr(vlapic, offset);
		*regptr = data;
		vlapic_lvt_write_handler(vlapic, offset);
		break;
	case APIC_OFFSET_TIMER_ICR:
		/* if TSCDEADLINE mode ignore icr_timer */
		if (vlapic_lvtt_tsc_deadline(vlapic))
			break;
		lapic->icr_timer = data;
		vlapic_icrtmr_write_handler(vlapic);
		break;

	case APIC_OFFSET_TIMER_DCR:
		lapic->dcr_timer = data;
		vlapic_dcr_write_handler(vlapic);
		break;

	case APIC_OFFSET_ESR:
		vlapic_esr_write_handler(vlapic);
		break;

	case APIC_OFFSET_SELF_IPI:
		break;

	case APIC_OFFSET_VER:
	case APIC_OFFSET_APR:
	case APIC_OFFSET_PPR:
	case APIC_OFFSET_RRR:
	case APIC_OFFSET_ISR0 ... APIC_OFFSET_ISR7:
	case APIC_OFFSET_TMR0 ... APIC_OFFSET_TMR7:
	case APIC_OFFSET_IRR0 ... APIC_OFFSET_IRR7:
		break;
	case APIC_OFFSET_TIMER_CCR:
		break;
	default:
		/* Read only */
		break;
	}

	return retval;
}

static void
vlapic_reset(struct vlapic *vlapic)
{
	struct lapic *lapic;

	lapic = vlapic->apic_page;
	memset(lapic, 0, sizeof(struct lapic));

	lapic->id = vlapic_build_id(vlapic);
	lapic->version = VLAPIC_VERSION;
	lapic->version |= (VLAPIC_MAXLVT_INDEX << MAXLVTSHIFT);
	lapic->dfr = 0xffffffff;
	lapic->svr = APIC_SVR_VECTOR;
	vlapic_mask_lvts(vlapic);
	vlapic_reset_tmr(vlapic);

	lapic->icr_timer = 0;
	lapic->dcr_timer = 0;
	vlapic_reset_timer(vlapic);

	vlapic->svr_last = lapic->svr;
}

void
vlapic_init(struct vlapic *vlapic)
{
	ASSERT(vlapic->vm != NULL, "%s: vm is not initialized", __func__);
	ASSERT(vlapic->vcpu->vcpu_id >= 0 &&
		vlapic->vcpu->vcpu_id < phy_cpu_num,
		"%s: vcpu_id is not initialized", __func__);
	ASSERT(vlapic->apic_page != NULL,
		"%s: apic_page is not initialized", __func__);

	/*
	 * If the vlapic is configured in x2apic mode then it will be
	 * accessed in the critical section via the MSR emulation code.
	 */
	vlapic->msr_apicbase = DEFAULT_APIC_BASE | APICBASE_ENABLED;

	if (vlapic->vcpu->vcpu_id == 0)
		vlapic->msr_apicbase |= APICBASE_BSP;

	vlapic_create_timer(vlapic);

	vlapic_reset(vlapic);
}

void vlapic_restore(struct vlapic *vlapic, struct lapic_regs *regs)
{
	struct lapic *lapic;
	int i;

	lapic = vlapic->apic_page;

	lapic->tpr = regs->tpr;
	lapic->apr = regs->apr;
	lapic->ppr = regs->ppr;
	lapic->ldr = regs->ldr;
	lapic->dfr = regs->dfr;
	for (i = 0; i < 8; i++)
		lapic->tmr[i].val = regs->tmr[i];
	lapic->svr = regs->svr;
	vlapic_svr_write_handler(vlapic);
	lapic->lvt_timer = regs->lvtt;
	lapic->lvt_lint0 = regs->lvt0;
	lapic->lvt_lint1 = regs->lvt1;
	lapic->lvt_error = regs->lvterr;
	lapic->icr_timer = regs->ticr;
	lapic->ccr_timer = regs->tccr;
	lapic->dcr_timer = regs->tdcr;
}

static uint64_t
vlapic_get_apicbase(struct vlapic *vlapic)
{

	return vlapic->msr_apicbase;
}

static int
vlapic_set_apicbase(struct vlapic *vlapic, uint64_t new)
{

	if (vlapic->msr_apicbase != new) {
		dev_dbg(ACRN_DBG_LAPIC,
			"NOT support to change APIC_BASE MSR from %#lx to %#lx",
			vlapic->msr_apicbase, new);
		return (-1);
	}

	return 0;
}

void
vlapic_deliver_intr(struct vm *vm, bool level, uint32_t dest, bool phys,
		int delmode, int vec)
{
	bool lowprio;
	int vcpu_id;
	uint64_t dmask;
	struct vcpu *target_vcpu;

	if (delmode != IOAPIC_RTE_DELFIXED &&
			delmode != IOAPIC_RTE_DELLOPRI &&
			delmode != IOAPIC_RTE_DELEXINT) {
		dev_dbg(ACRN_DBG_LAPIC,
			"vlapic intr invalid delmode %#x", delmode);
		return;
	}
	lowprio = (delmode == IOAPIC_RTE_DELLOPRI);

	/*
	 * We don't provide any virtual interrupt redirection hardware so
	 * all interrupts originating from the ioapic or MSI specify the
	 * 'dest' in the legacy xAPIC format.
	 */
	vlapic_calcdest(vm, &dmask, dest, phys, lowprio);

	while ((vcpu_id = bitmap_ffs(&dmask)) >= 0) {
		bitmap_clr(vcpu_id, &dmask);
		target_vcpu = vcpu_from_vid(vm, vcpu_id);
		if (target_vcpu == NULL)
			return;

		/* only make request when vlapic enabled */
		if (vlapic_enabled(target_vcpu->arch_vcpu.vlapic)) {
			if (delmode == IOAPIC_RTE_DELEXINT)
				vcpu_inject_extint(target_vcpu);
			else
				vlapic_set_intr(target_vcpu, vec, level);
		}
	}
}

bool
vlapic_enabled(struct vlapic *vlapic)
{
	struct lapic *lapic = vlapic->apic_page;

	if ((vlapic->msr_apicbase & APICBASE_ENABLED) &&
			(lapic->svr & APIC_SVR_ENABLE))
		return true;
	else
		return false;
}

void
vlapic_set_tmr(struct vlapic *vlapic, int vector, bool level)
{
	struct lapic *lapic;
	struct lapic_reg *tmrptr;
	uint32_t mask;
	int idx;

	lapic = vlapic->apic_page;
	tmrptr = &lapic->tmr[0];
	idx = vector / 32;
	mask = 1 << (vector % 32);
	if (level)
		tmrptr[idx].val |= mask;
	else
		tmrptr[idx].val &= ~mask;
}

/*
 * APICv batch set tmr will try to set multi vec at the same time
 * to avoid unnecessary VMCS read/update.
 */
void
vlapic_apicv_batch_set_tmr(struct vlapic *vlapic)
{
	if (vlapic->ops.apicv_batch_set_tmr != NULL)
		(*vlapic->ops.apicv_batch_set_tmr)(vlapic);
}

void
vlapic_apicv_set_tmr(struct vlapic *vlapic, int vector, bool level)
{
	if (vlapic->ops.apicv_set_tmr != NULL)
		(*vlapic->ops.apicv_set_tmr)(vlapic, vector, level);
}

void
vlapic_reset_tmr(struct vlapic *vlapic)
{
	int vector;

	dev_dbg(ACRN_DBG_LAPIC,
			"vlapic resetting all vectors to edge-triggered");

	for (vector = 0; vector <= 255; vector++)
		vlapic_set_tmr(vlapic, vector, false);

	vcpu_make_request(vlapic->vcpu, ACRN_REQUEST_TMR_UPDATE);
}

void
vlapic_set_tmr_one_vec(struct vlapic *vlapic, __unused int delmode,
	int vector, bool level)
{
	ASSERT((vector >= 0) && (vector <= NR_MAX_VECTOR),
		"invalid vector %d", vector);

	/*
	 * A level trigger is valid only for fixed and lowprio delivery modes.
	 */
	if (delmode != APIC_DELMODE_FIXED && delmode != APIC_DELMODE_LOWPRIO) {
		dev_dbg(ACRN_DBG_LAPIC,
			"Ignoring level trigger-mode for delivery-mode %d",
			delmode);
		return;
	}

	/* NOTE
	 * We don't check whether the vcpu is in the dest here. That means
	 * all vcpus of vm will do tmr update.
	 *
	 * If there is new caller to this function, need to refine this
	 * part of work.
	 */
	dev_dbg(ACRN_DBG_LAPIC, "vector %d set to level-triggered", vector);
	vlapic_set_tmr(vlapic, vector, level);
}

int
vlapic_set_intr(struct vcpu *vcpu, int vector, bool level)
{
	struct vlapic *vlapic;
	int ret = 0;

	if (vcpu == NULL)
		return -EINVAL;

	/*
	 * According to section "Maskable Hardware Interrupts" in Intel SDM
	 * vectors 16 through 255 can be delivered through the local APIC.
	 */
	if (vector < 16 || vector > 255)
		return -EINVAL;

	vlapic = vcpu->arch_vcpu.vlapic;
	if (vlapic_set_intr_ready(vlapic, vector, level))
		vcpu_make_request(vcpu, ACRN_REQUEST_EVENT);
	else
		ret = -ENODEV;

	return ret;
}

int
vlapic_set_local_intr(struct vm *vm, int cpu_id, int vector)
{
	struct vlapic *vlapic;
	uint64_t dmask;
	int error;

	if (cpu_id < -1 || cpu_id >= phy_cpu_num)
		return -EINVAL;

	if (cpu_id == -1)
		dmask = vm_active_cpus(vm);
	else
		bitmap_setof(cpu_id, &dmask);
	error = 0;
	while ((cpu_id = bitmap_ffs(&dmask)) >= 0) {
		bitmap_clr(cpu_id, &dmask);
		vlapic = vm_lapic_from_vcpu_id(vm, cpu_id);
		error = vlapic_trigger_lvt(vlapic, vector);
		if (error)
			break;
	}

	return error;
}

int
vlapic_intr_msi(struct vm *vm, uint64_t addr, uint64_t msg)
{
	int delmode, vec;
	uint32_t dest;
	bool phys;

	dev_dbg(ACRN_DBG_LAPIC, "lapic MSI addr: %#lx msg: %#lx", addr, msg);

	if ((addr & MSI_ADDR_MASK) != MSI_ADDR_BASE) {
		dev_dbg(ACRN_DBG_LAPIC, "lapic MSI invalid addr %#lx", addr);
		return -1;
	}

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
	dest = (addr >> 12) & 0xff;
	phys = ((addr & (MSI_ADDR_RH | MSI_ADDR_LOG)) !=
			(MSI_ADDR_RH | MSI_ADDR_LOG));
	delmode = msg & APIC_DELMODE_MASK;
	vec = msg & 0xff;

	dev_dbg(ACRN_DBG_LAPIC, "lapic MSI %s dest %#x, vec %d",
		phys ? "physical" : "logical", dest, vec);

	vlapic_deliver_intr(vm, LAPIC_TRIG_EDGE, dest, phys, delmode, vec);
	return 0;
}

static bool
x2apic_msr(uint32_t msr)
{
	if (msr >= 0x800 && msr <= 0xBFF)
		return true;
	else
		return false;
}

static uint32_t
x2apic_msr_to_regoff(uint32_t msr)
{

	return (msr - 0x800) << 4;
}

bool
vlapic_msr(uint32_t msr)
{

	if (x2apic_msr(msr) || (msr == MSR_IA32_APIC_BASE))
		return true;
	else
		return false;
}

/* interrupt context */
static int vlapic_timer_expired(void *data)
{
	struct vcpu *vcpu = (struct vcpu *)data;
	struct vlapic *vlapic;
	struct lapic *lapic;

	vlapic = vcpu->arch_vcpu.vlapic;
	lapic = vlapic->apic_page;

	/* inject vcpu timer interrupt if not masked */
	if (!vlapic_lvtt_masked(vlapic))
		vlapic_intr_edge(vcpu, lapic->lvt_timer & APIC_LVTT_VECTOR);

	if (!vlapic_lvtt_period(vlapic))
		vlapic->vlapic_timer.timer.fire_tsc = 0;

	return 0;
}

int
vlapic_rdmsr(struct vcpu *vcpu, uint32_t msr, uint64_t *rval)
{
	int error = 0;
	uint32_t offset;
	struct vlapic *vlapic;

	dev_dbg(ACRN_DBG_LAPIC, "cpu[%d] rdmsr: %x", vcpu->vcpu_id, msr);
	vlapic = vcpu->arch_vcpu.vlapic;

	switch (msr) {
	case MSR_IA32_APIC_BASE:
		*rval = vlapic_get_apicbase(vlapic);
		break;

	case MSR_IA32_TSC_DEADLINE:
		*rval = vlapic_get_tsc_deadline_msr(vlapic);
		break;

	default:
		offset = x2apic_msr_to_regoff(msr);
		error = vlapic_read(vlapic, 0, offset, rval);
		break;
	}

	return error;
}

int
vlapic_wrmsr(struct vcpu *vcpu, uint32_t msr, uint64_t val)
{
	int error = 0;
	uint32_t offset;
	struct vlapic *vlapic;

	vlapic = vcpu->arch_vcpu.vlapic;

	switch (msr) {
	case MSR_IA32_APIC_BASE:
		error = vlapic_set_apicbase(vlapic, val);
		break;

	case MSR_IA32_TSC_DEADLINE:
		vlapic_set_tsc_deadline_msr(vlapic, val);
		break;

	default:
		offset = x2apic_msr_to_regoff(msr);
		error = vlapic_write(vlapic, 0, offset, val);
		break;
	}

	dev_dbg(ACRN_DBG_LAPIC, "cpu[%d] wrmsr: %x val=%#x",
		vcpu->vcpu_id, msr, val);
	return error;
}

int
vlapic_mmio_write(struct vcpu *vcpu, uint64_t gpa, uint64_t wval, int size)
{
	int error;
	uint64_t off;
	struct vlapic *vlapic;

	off = gpa - DEFAULT_APIC_BASE;

	/*
	 * Memory mapped local apic accesses must be 4 bytes wide and
	 * aligned on a 16-byte boundary.
	 */
	if (size != 4 || off & 0xf)
		return -EINVAL;

	vlapic = vcpu->arch_vcpu.vlapic;
	error = vlapic_write(vlapic, 1, off, wval);
	return error;
}

int
vlapic_mmio_read(struct vcpu *vcpu, uint64_t gpa, uint64_t *rval,
			__unused int size)
{
	int error;
	uint64_t off;
	struct vlapic *vlapic;

	off = gpa - DEFAULT_APIC_BASE;

	/*
	 * Memory mapped local apic accesses should be aligned on a
	 * 16-byte boundary. They are also suggested to be 4 bytes
	 * wide, alas not all OSes follow suggestions.
	 */
	off &= ~3;
	if (off & 0xf)
		return -EINVAL;

	vlapic = vcpu->arch_vcpu.vlapic;
	error = vlapic_read(vlapic, 1, off, rval);
	return error;
}

int vlapic_mmio_access_handler(struct vcpu *vcpu, struct mem_io *mmio,
		__unused void *handler_private_data)
{
	uint64_t gpa = mmio->paddr;
	int ret = 0;

	/* Note all RW to LAPIC are 32-Bit in size */
	ASSERT(mmio->access_size == 4,
			"All RW to LAPIC must be 32-bits in size");

	if (mmio->read_write == HV_MEM_IO_READ) {
		ret = vlapic_mmio_read(vcpu,
				gpa,
				&mmio->value,
				mmio->access_size);
		mmio->mmio_status = MMIO_TRANS_VALID;

	} else if (mmio->read_write == HV_MEM_IO_WRITE) {
		ret = vlapic_mmio_write(vcpu,
				gpa,
				mmio->value,
				mmio->access_size);

		mmio->mmio_status = MMIO_TRANS_VALID;
	}

	return ret;
}

int vlapic_create(struct vcpu *vcpu)
{
	void *apic_page = alloc_page();
	struct vlapic *vlapic = calloc(1, sizeof(struct vlapic));

	ASSERT(vlapic != NULL, "vlapic allocate failed");
	ASSERT(apic_page != NULL, "apic reg page allocate failed");

	memset((void *)apic_page, 0, CPU_PAGE_SIZE);
	vlapic->vm = vcpu->vm;
	vlapic->vcpu = vcpu;
	vlapic->apic_page = (struct lapic *) apic_page;

	if (is_vapic_supported()) {
		if (is_vapic_intr_delivery_supported()) {
			vlapic->ops.apicv_set_intr_ready =
					apicv_set_intr_ready;

			vlapic->ops.apicv_pending_intr =
					apicv_pending_intr;

			vlapic->ops.apicv_set_tmr = apicv_set_tmr;
			vlapic->ops.apicv_batch_set_tmr =
					apicv_batch_set_tmr;

			vlapic->pir_desc = (struct pir_desc *)(&(vlapic->pir));
		}

		if (is_vcpu_bsp(vcpu)) {
			ept_mmap(vcpu->vm,
				apicv_get_apic_access_addr(vcpu->vm),
				DEFAULT_APIC_BASE, CPU_PAGE_SIZE, MAP_MMIO,
				MMU_MEM_ATTR_WRITE | MMU_MEM_ATTR_READ |
				MMU_MEM_ATTR_UNCACHED);
		}
	} else {
		/*No APICv support*/
		if (register_mmio_emulation_handler(vcpu->vm,
				vlapic_mmio_access_handler,
				(uint64_t)DEFAULT_APIC_BASE,
				(uint64_t)DEFAULT_APIC_BASE +
				CPU_PAGE_SIZE,
				(void *) 0))
			return -1;
	}

	vcpu->arch_vcpu.vlapic = vlapic;
	vlapic_init(vlapic);

	return 0;
}

void vlapic_free(struct vcpu *vcpu)
{
	struct vlapic *vlapic = NULL;
	void *apic_page = NULL;

	if (vcpu == NULL)
		return;

	vlapic = vcpu->arch_vcpu.vlapic;
	if (vlapic == NULL)
		return;

	del_timer(&vlapic->vlapic_timer.timer);

	if (!is_vapic_supported()) {
		unregister_mmio_emulation_handler(vcpu->vm,
			(uint64_t)DEFAULT_APIC_BASE,
			(uint64_t)DEFAULT_APIC_BASE + CPU_PAGE_SIZE);
	}

	apic_page = vlapic->apic_page;
	if (apic_page == NULL) {
		free(vlapic);
		return;
	}

	free(apic_page);
	free(vlapic);
}

/**
 * APIC-v functions
 * **/
static int
apicv_set_intr_ready(struct vlapic *vlapic, int vector, __unused bool level)
{
	struct pir_desc *pir_desc;
	uint64_t mask;
	int idx, notify;

	pir_desc = vlapic->pir_desc;

	idx = vector / 64;
	mask = 1UL << (vector % 64);

	atomic_set_long(&pir_desc->pir[idx], mask);
	notify = (atomic_cmpxchg64((long *)&pir_desc->pending, 0, 1) == 0);
	return notify;
}

static int
apicv_pending_intr(struct vlapic *vlapic, __unused int *vecptr)
{
	struct pir_desc *pir_desc;
	struct lapic *lapic;
	uint64_t pending, pirval;
	uint32_t ppr, vpr;
	int i;

	pir_desc = vlapic->pir_desc;

	pending = atomic_load64((long *)&pir_desc->pending);
	if (!pending)
		return 0;

	lapic = vlapic->apic_page;
	ppr = lapic->ppr & 0xF0;

	if (ppr == 0)
		return 1;

	for (i = 3; i >= 0; i--) {
		pirval = pir_desc->pir[i];
		if (pirval != 0) {
			vpr = (i * 64 + flsl(pirval)) & 0xF0;
			return (vpr > ppr);
		}
	}
	return 0;
}

static void
apicv_set_tmr(__unused struct vlapic *vlapic, int vector, bool level)
{
	uint64_t mask, val;

	mask = 1UL << (vector % 64);

	val = exec_vmread(VMX_EOI_EXIT(vector));
	if (level)
		val |= mask;
	else
		val &= ~mask;

	exec_vmwrite(VMX_EOI_EXIT(vector), val);
}

/* Update the VMX_EOI_EXIT according to related tmr */
#define	EOI_STEP_LEN	(64)
#define	TMR_STEP_LEN	(32)
static void
apicv_batch_set_tmr(struct vlapic *vlapic)
{
	struct lapic *lapic = vlapic->apic_page;
	uint64_t val;
	struct lapic_reg *ptr;
	unsigned int s, e;

	ptr = &lapic->tmr[0];
	s = 0;
	e = 256;

	while (s < e) {
		val = ptr[s/TMR_STEP_LEN + 1].val;
		val <<= TMR_STEP_LEN;
		val |= ptr[s/TMR_STEP_LEN].val;
		exec_vmwrite64(VMX_EOI_EXIT(s), val);

		s += EOI_STEP_LEN;
	}
}

/**
 *APIC-v: Get the HPA to APIC-access page
 * **/
uint64_t
apicv_get_apic_access_addr(__unused struct vm *vm)
{
	if (apicv_apic_access_addr == NULL) {
		apicv_apic_access_addr = alloc_page();
		ASSERT(apicv_apic_access_addr != NULL,
					"apicv allocate failed.");

		memset((void *)apicv_apic_access_addr, 0, CPU_PAGE_SIZE);
	}
	return HVA2HPA(apicv_apic_access_addr);
}

/**
 *APIC-v: Get the HPA to virtualized APIC registers page
 * **/
uint64_t
apicv_get_apic_page_addr(struct vlapic *vlapic)
{
	return HVA2HPA(vlapic->apic_page);
}

/*
 * Transfer the pending interrupts in the PIR descriptor to the IRR
 * in the virtual APIC page.
 */

void
apicv_inject_pir(struct vlapic *vlapic)
{
	struct pir_desc *pir_desc;
	struct lapic *lapic;
	uint64_t val, pirval;
	int rvi, pirbase = -1, i;
	uint16_t intr_status_old, intr_status_new;
	struct lapic_reg *irr = NULL;

	pir_desc = vlapic->pir_desc;
	if (atomic_cmpxchg64((long *)&pir_desc->pending, 1, 0) != 1)
		return;

	pirval = 0;
	pirbase = -1;
	lapic = vlapic->apic_page;
	irr = &lapic->irr[0];

	for (i = 0; i < 4; i++) {
		val = atomic_readandclear64((long *)&pir_desc->pir[i]);
		if (val != 0) {
			irr[i * 2].val |= val;
			irr[(i * 2) + 1].val |= val >> 32;

			pirbase = 64*i;
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
	if (pirval != 0) {
		rvi = pirbase + flsl(pirval);

		intr_status_old = (uint16_t)
				(0xFFFF &
				exec_vmread(VMX_GUEST_INTR_STATUS));

		intr_status_new = (intr_status_old & 0xFF00) | rvi;
		if (intr_status_new > intr_status_old)
			exec_vmwrite(VMX_GUEST_INTR_STATUS,
					intr_status_new);
	}
}

int apic_access_vmexit_handler(struct vcpu *vcpu)
{
	int access_type, offset;
	uint64_t qual;
	struct vlapic *vlapic;

	qual = vcpu->arch_vcpu.exit_qualification;
	access_type = APIC_ACCESS_TYPE(qual);

	/*parse offset if linear access*/
	if (access_type <= 3)
		offset = APIC_ACCESS_OFFSET(qual);

	vlapic = vcpu->arch_vcpu.vlapic;

	analyze_instruction(vcpu, &vcpu->mmio);
	if (access_type == 1) {
		if (!emulate_instruction(vcpu, &vcpu->mmio))
			vlapic_write(vlapic, 1, offset, vcpu->mmio.value);
	} else if (access_type == 0) {
		vlapic_read(vlapic, 1, offset, &vcpu->mmio.value);
		emulate_instruction(vcpu, &vcpu->mmio);
	}

	TRACE_2L(TRC_VMEXIT_APICV_ACCESS, qual, (uint64_t)vlapic);
	return 0;
}

int veoi_vmexit_handler(struct vcpu *vcpu)
{
	struct vlapic *vlapic = NULL;

	int vector;
	struct lapic *lapic;
	struct lapic_reg *tmrptr;
	uint32_t idx, mask;

	VCPU_RETAIN_RIP(vcpu);

	vlapic = vcpu->arch_vcpu.vlapic;
	lapic = vlapic->apic_page;
	vector = (vcpu->arch_vcpu.exit_qualification) & 0xFF;

	tmrptr = &lapic->tmr[0];
	idx = vector / 32;
	mask = 1 << (vector % 32);

	if ((tmrptr[idx].val & mask) != 0) {
		/* hook to vIOAPIC */
		vioapic_process_eoi(vlapic->vm, vector);
	}

	TRACE_2L(TRC_VMEXIT_APICV_VIRT_EOI, vector, 0);

	return 0;
}

int apic_write_vmexit_handler(struct vcpu *vcpu)
{
	uint64_t qual;
	int error, handled, offset;
	struct vlapic *vlapic = NULL;

	qual = vcpu->arch_vcpu.exit_qualification;
	offset = (qual & 0xFFF);

	handled = 1;
	VCPU_RETAIN_RIP(vcpu);
	vlapic = vcpu->arch_vcpu.vlapic;

	switch (offset) {
	case APIC_OFFSET_ID:
		vlapic_id_write_handler(vlapic);
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
		if (error != 0)
			handled = 0;
		break;
	case APIC_OFFSET_CMCI_LVT:
	case APIC_OFFSET_TIMER_LVT ... APIC_OFFSET_ERROR_LVT:
		vlapic_lvt_write_handler(vlapic, offset);
		break;
	case APIC_OFFSET_TIMER_ICR:
		vlapic_icrtmr_write_handler(vlapic);
		break;
	case APIC_OFFSET_TIMER_DCR:
		vlapic_dcr_write_handler(vlapic);
		break;
	default:
		handled = 0;
		pr_err("Unhandled APIC-Write, offset:0x%x", offset);
		break;
	}

	TRACE_2L(TRC_VMEXIT_APICV_WRITE, offset, 0);

	return handled;
}

int tpr_below_threshold_vmexit_handler(__unused struct vcpu *vcpu)
{
	pr_err("Unhandled %s.", __func__);
	return 0;
}
