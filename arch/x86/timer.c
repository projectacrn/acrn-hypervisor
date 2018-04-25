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
#define TIMER_IRQ		(NR_MAX_IRQS - 1)
#define CAL_MS			10

uint64_t tsc_hz = 1000000000;

struct per_cpu_timers {
	struct list_head timer_list;	/* it's for runtime active timer list */
};

static DEFINE_CPU_DATA(struct per_cpu_timers, cpu_timers);
static DEFINE_CPU_DATA(struct dev_handler_node *, timer_node);

static struct timer *find_expired_timer(
			struct per_cpu_timers *cpu_timer,
			uint64_t tsc_now)
{
	struct timer *timer = NULL, *tmp;
	struct list_head *pos;

	list_for_each(pos, &cpu_timer->timer_list) {
		tmp = list_entry(pos, struct timer, node);
		if (tmp->fire_tsc <= tsc_now) {
			timer = tmp;
			break;
		}
	}

	return timer;
}

static struct timer *_search_nearest_timer(
			struct per_cpu_timers *cpu_timer)
{
	struct timer *timer;
	struct timer *target = NULL;
	struct list_head *pos;

	list_for_each(pos, &cpu_timer->timer_list) {
		timer = list_entry(pos, struct timer, node);
		if (target == NULL)
			target = timer;
		else if (timer->fire_tsc < target->fire_tsc)
			target = timer;
	}

	return target;
}

static void run_timer(struct timer *timer)
{
	/* deadline = 0 means stop timer, we should skip */
	if (timer->func && timer->fire_tsc != 0UL)
		timer->func(timer->priv_data);

	TRACE_2L(TRACE_TIMER_ACTION_PCKUP, timer->fire_tsc, 0);
}

/* run in interrupt context */
static int tsc_deadline_handler(__unused int irq, __unused void *data)
{
	raise_softirq(SOFTIRQ_TIMER);
	return 0;
}

static inline void schedule_next_timer(struct per_cpu_timers *cpu_timer)
{
	struct timer *timer;

	timer = _search_nearest_timer(cpu_timer);
	if (timer) {
		/* it is okay to program a expired time */
		msr_write(MSR_IA32_TSC_DEADLINE, timer->fire_tsc);
	}
}

int add_timer(struct timer *timer)
{
	struct per_cpu_timers *cpu_timer;
	int pcpu_id;

	if (timer == NULL || timer->func == NULL || timer->fire_tsc == 0)
		return -EINVAL;

	pcpu_id  = get_cpu_id();
	cpu_timer = &per_cpu(cpu_timers, pcpu_id);
	list_add_tail(&timer->node, &cpu_timer->timer_list);

	schedule_next_timer(cpu_timer);

	TRACE_2L(TRACE_TIMER_ACTION_ADDED, timer->fire_tsc, 0);
	return 0;

}

void del_timer(struct timer *timer)
{
	if (timer && !list_empty(&timer->node))
		list_del(&timer->node);
}

static int request_timer_irq(int pcpu_id,
			dev_handler_t func, void *data,
			const char *name)
{
	struct dev_handler_node *node = NULL;

	if (pcpu_id >= phy_cpu_num)
		return -EINVAL;

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
		return -ENODEV;
	}

	return 0;
}

static void init_percpu_timer(int pcpu_id)
{
	struct per_cpu_timers *cpu_timer;

	cpu_timer = &per_cpu(cpu_timers, pcpu_id);
	INIT_LIST_HEAD(&cpu_timer->timer_list);
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
	init_percpu_timer(pcpu_id);
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
		del_timer(timer);

		run_timer(timer);

		/* search next one */
		timer = find_expired_timer(cpu_timer, rdtsc());
	}

	/* update nearest timer */
	schedule_next_timer(cpu_timer);
	return 0;
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
