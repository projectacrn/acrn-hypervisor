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

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>

#define MAX_TIMER_ACTIONS	32
#define CAL_MS			10

uint64_t tsc_hz = 1000000000;

struct timer {
	timer_handle_t	func;		/* callback if time reached */
	uint64_t	priv_data;	/* func private data */
	uint64_t	deadline;	/* tsc deadline to interrupt */
	long		handle;		/* unique handle for user */
	int		pcpu_id;	/* armed on which CPU */
	int		id;		/* timer ID, used by release */
	struct list_head node;		/* link all timers */
};

struct per_cpu_timers {
	struct timer *timers_pool; 	/* it's timers pool for allocation */
	uint64_t free_bitmap;
	struct list_head timer_list;	/* it's for runtime active timer list */
	int pcpu_id;
	uint64_t used_handler;
};

static DEFINE_CPU_DATA(struct per_cpu_timers, cpu_timers);

#define TIMER_IRQ (NR_MAX_IRQS - 1)

DEFINE_CPU_DATA(struct dev_handler_node *, timer_node);

static struct timer*
find_expired_timer(struct per_cpu_timers *cpu_timer, uint64_t tsc_now);

static struct timer *alloc_timer(int pcpu_id)
{
	int idx;
	struct per_cpu_timers *cpu_timer;
	struct timer *timer;

	cpu_timer = &per_cpu(cpu_timers, pcpu_id);
	idx = bitmap_ffs(&cpu_timer->free_bitmap);
	if (idx < 0) {
		return NULL;
	}

	bitmap_clr(idx, &cpu_timer->free_bitmap);
	cpu_timer->used_handler++;

	/* assign unique handle and never duplicate */
	timer = cpu_timer->timers_pool + idx;
	timer->handle = cpu_timer->used_handler;

	ASSERT((cpu_timer->timers_pool[pcpu_id].pcpu_id == pcpu_id),
		"timer pcpu_id did not match");
	return timer;
}

static void release_timer(struct timer *timer)
{
	struct per_cpu_timers *cpu_timer;

	cpu_timer = &per_cpu(cpu_timers, timer->pcpu_id);
	timer->priv_data = 0;
	timer->func = NULL;
	timer->deadline = 0;
	bitmap_set(timer->id, &cpu_timer->free_bitmap);
}

static int get_target_cpu(void)
{
	/* we should search idle CPU to balance timer service */
	return get_cpu_id();
}

static struct timer*
find_expired_timer(struct per_cpu_timers *cpu_timer, uint64_t tsc_now)
{
	struct timer *timer;
	struct list_head *pos;

	list_for_each(pos, &cpu_timer->timer_list) {
		timer = list_entry(pos, struct timer, node);
		if (timer->deadline <= tsc_now)
			goto UNLOCK;
	}
	timer = NULL;
UNLOCK:
	return timer;
}

/* need lock protect outside */
static struct timer*
_search_nearest_timer(struct per_cpu_timers *cpu_timer)
{
	struct timer *timer;
	struct timer *target = NULL;
	struct list_head *pos;

	list_for_each(pos, &cpu_timer->timer_list) {
		timer = list_entry(pos, struct timer, node);
		if (target == NULL)
			target = timer;
		else if (timer->deadline < target->deadline)
			target = timer;
	}

	return target;
}

/* need lock protect outside */
static struct timer*
_search_timer_by_handle(struct per_cpu_timers *cpu_timer, long handle)
{
	struct timer *timer = NULL, *tmp;
	struct list_head *pos;

	list_for_each(pos, &cpu_timer->timer_list) {
		tmp = list_entry(pos, struct timer, node);
		if (tmp->handle == handle) {
			timer = tmp;
			break;
		}
	}

	return timer;
}

static void run_timer(struct timer *timer)
{

	/* remove from list first */
	list_del(&timer->node);

	/* deadline = 0 means stop timer, we should skip */
	if (timer->func && timer->deadline != 0UL)
		timer->func(timer->priv_data);

	TRACE_4I(TRACE_TIMER_ACTION_PCKUP, timer->id, timer->deadline,
		timer->deadline >> 32, 0);
}

/* run in interrupt context */
static int tsc_deadline_handler(__unused int irq, __unused void *data)
{
	raise_softirq(SOFTIRQ_TIMER);
	return 0;
}

static inline void schedule_next_timer(int pcpu_id)
{
	struct timer *timer;
	struct per_cpu_timers *cpu_timer = &per_cpu(cpu_timers, pcpu_id);

	timer = _search_nearest_timer(cpu_timer);
	if (timer) {
		/* it is okay to program a expired time */
		msr_write(MSR_IA32_TSC_DEADLINE, timer->deadline);
	}
}

int request_timer_irq(int pcpu_id, dev_handler_t func,
			void *data, const char *name)
{
	struct dev_handler_node *node = NULL;

	if (pcpu_id >= phy_cpu_num)
		return -1;

	if (per_cpu(timer_node, pcpu_id)) {
		pr_err("CPU%d timer isr already added", pcpu_id);
		unregister_handler_common(per_cpu(timer_node, pcpu_id));
	}

	node = pri_register_handler(TIMER_IRQ, VECTOR_TIMER, func, data, name);
	if (node != NULL) {
		per_cpu(timer_node, pcpu_id) = node;
		update_irq_handler(TIMER_IRQ, quick_handler_nolock);
	} else {
		pr_err("Failed to add timer isr");
		return -1;
	}

	return 0;
}

/*TODO: init in separate cpu */
static void init_timer_pool(void)
{
	int i, j;
	struct per_cpu_timers *cpu_timer;
	struct timer *timers_pool;

	/* Make sure only init one time*/
	if (get_cpu_id() > 0)
		return;

	for (i = 0; i < phy_cpu_num; i++) {
		cpu_timer = &per_cpu(cpu_timers, i);
		cpu_timer->pcpu_id = i;
		timers_pool =
			calloc(MAX_TIMER_ACTIONS, sizeof(struct timer));
		ASSERT(timers_pool, "Create timers pool failed");

		cpu_timer->timers_pool = timers_pool;
		cpu_timer->free_bitmap = (1UL<<MAX_TIMER_ACTIONS)-1;

		INIT_LIST_HEAD(&cpu_timer->timer_list);
		for (j = 0; j < MAX_TIMER_ACTIONS; j++) {
			timers_pool[j].id = j;
			timers_pool[j].pcpu_id = i;
			timers_pool[j].priv_data = 0;
			timers_pool[j].func = NULL;
			timers_pool[j].deadline = 0;
			timers_pool[j].handle = -1UL;
		}
	}
}

static void init_tsc_deadline_timer(void)
{
	uint32_t val;

	val = VECTOR_TIMER;
	val |= APIC_LVTT_TM_TSCDLT; /* TSC deadline and unmask */
	write_lapic_reg32(LAPIC_LVT_TIMER_REGISTER, val);
	asm volatile("mfence" : : : "memory");

	/* disarm timer */
	msr_write(MSR_IA32_TSC_DEADLINE, 0UL);
}

void timer_init(void)
{
	char name[32] = {0};
	int pcpu_id = get_cpu_id();

	snprintf(name, 32, "timer_tick[%d]", pcpu_id);
	if (request_timer_irq(pcpu_id, tsc_deadline_handler, NULL, name) < 0) {
		pr_err("Timer setup failed");
		return;
	}

	init_tsc_deadline_timer();
	init_timer_pool();
}

void timer_cleanup(void)
{
	int pcpu_id = get_cpu_id();

	if (per_cpu(timer_node, pcpu_id))
		unregister_handler_common(per_cpu(timer_node, pcpu_id));

	per_cpu(timer_node, pcpu_id) = NULL;
}

int timer_softirq(int pcpu_id)
{
	struct per_cpu_timers *cpu_timer;
	struct timer *timer;
	int max = MAX_TIMER_ACTIONS;

	/* handle passed timer */
	cpu_timer = &per_cpu(cpu_timers, pcpu_id);

	/* This is to make sure we are not blocked due to delay inside func()
	 * force to exit irq handler after we serviced >31 timers
	 * caller used to add_timer() in timer->func(), if there is a delay
	 * inside func(), it will infinitely loop here, because new added timer
	 * already passed due to previously func()'s delay.
	 */
	timer = find_expired_timer(cpu_timer, rdtsc());
	while (timer && --max > 0) {
		run_timer(timer);
		/* put back to timer pool */
		release_timer(timer);
		/* search next one */
		timer = find_expired_timer(cpu_timer, rdtsc());
	}

	/* update nearest timer */
	schedule_next_timer(pcpu_id);
	return 0;
}

/*
 * add_timer is okay to add passed timer but not 0
 * return: handle, this handle is unique and can be used to find back
 *  this added timer. handle will be invalid after timer expired
 */
long add_timer(timer_handle_t func, uint64_t data, uint64_t deadline)
{
	struct timer *timer;
	struct per_cpu_timers *cpu_timer;
	int pcpu_id = get_target_cpu();

	if (deadline == 0 || func == NULL)
		return -1;

	/* possible interrupt context please avoid mem alloct here*/
	timer = alloc_timer(pcpu_id);
	if (timer == NULL)
		return -1;

	timer->func = func;
	timer->priv_data = data;
	timer->deadline = deadline;
	timer->pcpu_id = get_target_cpu();

	cpu_timer = &per_cpu(cpu_timers, timer->pcpu_id);

	/* We need irqsave here even softirq enabled to protect timer_list */
	list_add_tail(&timer->node, &cpu_timer->timer_list);
	TRACE_4I(TRACE_TIMER_ACTION_ADDED, timer->id, timer->deadline,
		timer->deadline >> 32, cpu_timer->used_handler);

	schedule_next_timer(pcpu_id);
	return timer->handle;
}

/*
 * update_timer existing timer. if not found, add new timer
 */
long
update_timer(long handle, timer_handle_t func, uint64_t data,
		uint64_t deadline)
{
	struct timer *timer;
	struct per_cpu_timers *cpu_timer;
	int pcpu_id = get_target_cpu();

	bool ret = false;

	if (deadline == 0)
		return -1;

	cpu_timer = &per_cpu(cpu_timers, pcpu_id);
	timer = _search_timer_by_handle(cpu_timer, handle);
	if (timer) {
		/* update deadline and re-sort */
		timer->deadline = deadline;
		timer->func = func;
		timer->priv_data = data;
		TRACE_4I(TRACE_TIMER_ACTION_UPDAT, timer->id,
			timer->deadline, timer->deadline >> 32,
			cpu_timer->used_handler);
		ret = true;
	}

	if (ret)
		schedule_next_timer(pcpu_id);
	else {
		/* if update failed, we add to new, and update handle */
		/* TODO: the correct behavior should be return failure here */
		handle = add_timer(func, data, deadline);
	}

	return handle;
}

/* NOTE: pcpu_id referred to physical cpu id here */
bool cancel_timer(long handle, int pcpu_id)
{
	struct timer *timer;
	struct per_cpu_timers *cpu_timer;

	bool ret = false;

	cpu_timer = &per_cpu(cpu_timers, pcpu_id);
	timer = _search_timer_by_handle(cpu_timer, handle);
	if (timer) {
		/* NOTE: we can not directly release timer here.
		 * Instead we set deadline to expired and clear func.
		 * This timer will be reclaim next timer
		 */
		timer->deadline = 0;
		timer->func = NULL;
		ret = true;
	}
	return ret;
}

void check_tsc(void)
{
	uint64_t temp64;

	/* Ensure time-stamp timer is turned on for each CPU */
	CPU_CR_READ(cr4, &temp64);
	CPU_CR_WRITE(cr4, (temp64 & ~CR4_TSD));
}

static uint64_t pit_calibrate_tsc(uint16_t cal_ms)
{
#define PIT_TICK_RATE	1193182UL
#define PIT_TARGET	0x3FFF
#define PIT_MAX_COUNT	0xFFFF

	uint16_t initial_pit;
	uint16_t current_pit;
	uint16_t max_cal_ms;
	uint64_t current_tsc;

	max_cal_ms = (PIT_MAX_COUNT - PIT_TARGET) * 1000 / PIT_TICK_RATE;
	cal_ms = min(cal_ms, max_cal_ms);

	/* Assume the 8254 delivers 18.2 ticks per second when 16 bits fully
	 * wrap.  This is about 1.193MHz or a clock period of 0.8384uSec
	 */
	initial_pit = (uint16_t)(cal_ms * PIT_TICK_RATE / 1000);
	initial_pit += PIT_TARGET;

	/* Port 0x43 ==> Control word write; Data 0x30 ==> Select Counter 0,
	 * Read/Write least significant byte first, mode 0, 16 bits.
	 */

	io_write_byte(0x30, 0x43);
	io_write_byte(initial_pit & 0x00ff, 0x40);	/* Write LSB */
	io_write_byte(initial_pit >> 8, 0x40);		/* Write MSB */

	current_tsc = rdtsc();

	do {
		/* Port 0x43 ==> Control word write; 0x00 ==> Select
		 * Counter 0, Counter Latch Command, Mode 0; 16 bits
		 */
		io_write_byte(0x00, 0x43);

		current_pit = io_read_byte(0x40);	/* Read LSB */
		current_pit |= io_read_byte(0x40) << 8;	/* Read MSB */
		/* Let the counter count down to PIT_TARGET */
	} while (current_pit > PIT_TARGET);

	current_tsc = rdtsc() - current_tsc;

	return current_tsc / cal_ms * 1000;
}

/*
 * Determine TSC frequency via CPUID 0x15
 */
static uint64_t native_calibrate_tsc(void)
{
	if (boot_cpu_data.cpuid_level >= 0x15) {
		uint32_t eax_denominator, ebx_numerator, ecx_hz, reserved;

		cpuid(0x15, &eax_denominator, &ebx_numerator,
			&ecx_hz, &reserved);

		if (eax_denominator != 0 && ebx_numerator != 0)
			return (uint64_t) ecx_hz *
				ebx_numerator / eax_denominator;
	}

	return 0;
}

void calibrate_tsc(void)
{
	tsc_hz = native_calibrate_tsc();
	if (!tsc_hz)
		tsc_hz = pit_calibrate_tsc(CAL_MS);
	printf("%s, tsc_hz=%lu\n", __func__, tsc_hz);
}
