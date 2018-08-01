/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <softirq.h>

#define MAX_TIMER_ACTIONS	32U
#define TIMER_IRQ		(NR_IRQS - 1U)
#define CAL_MS			10U
#define MIN_TIMER_PERIOD_US	500U

uint32_t tsc_khz = 0U;

static void run_timer(struct hv_timer *timer)
{
	/* deadline = 0 means stop timer, we should skip */
	if ((timer->func != NULL) && timer->fire_tsc != 0UL) {
		timer->func(timer->priv_data);
	}

	TRACE_2L(TRACE_TIMER_ACTION_PCKUP, timer->fire_tsc, 0UL);
}

/* run in interrupt context */
static int tsc_deadline_handler(__unused int irq, __unused void *data)
{
	fire_softirq(SOFTIRQ_TIMER);
	return 0;
}

static inline void update_physical_timer(struct per_cpu_timers *cpu_timer)
{
	struct hv_timer *timer = NULL;

	/* find the next event timer */
	if (!list_empty(&cpu_timer->timer_list)) {
		timer = list_entry((&cpu_timer->timer_list)->next,
			struct hv_timer, node);

		/* it is okay to program a expired time */
		msr_write(MSR_IA32_TSC_DEADLINE, timer->fire_tsc);
	}
}

static void local_add_timer(struct per_cpu_timers *cpu_timer,
			struct hv_timer *timer,
			bool *need_update)
{
	struct list_head *pos, *prev;
	struct hv_timer *tmp;
	uint64_t tsc = timer->fire_tsc;

	prev = &cpu_timer->timer_list;
	list_for_each(pos, &cpu_timer->timer_list) {
		tmp = list_entry(pos, struct hv_timer, node);
		if (tmp->fire_tsc < tsc) {
			prev = &tmp->node;
		}
		else {
			break;
		}
	}

	list_add(&timer->node, prev);

	if (need_update != NULL) {
		/* update the physical timer if we're on the timer_list head */
		*need_update = (prev == &cpu_timer->timer_list);
	}
}

int add_timer(struct hv_timer *timer)
{
	struct per_cpu_timers *cpu_timer;
	uint16_t pcpu_id;
	bool need_update;

	if (timer == NULL || timer->func == NULL || timer->fire_tsc == 0UL) {
		return -EINVAL;
	}

	/* limit minimal periodic timer cycle period */
	if (timer->mode == TICK_MODE_PERIODIC) {
		timer->period_in_cycle = max(timer->period_in_cycle,
				us_to_ticks(MIN_TIMER_PERIOD_US));
	}

	pcpu_id  = get_cpu_id();
	cpu_timer = &per_cpu(cpu_timers, pcpu_id);
	local_add_timer(cpu_timer, timer, &need_update);

	if (need_update) {
		update_physical_timer(cpu_timer);
	}

	TRACE_2L(TRACE_TIMER_ACTION_ADDED, timer->fire_tsc, 0UL);
	return 0;

}

void del_timer(struct hv_timer *timer)
{
	if ((timer != NULL) && !list_empty(&timer->node)) {
		list_del_init(&timer->node);
	}
}

static int request_timer_irq(uint16_t pcpu_id,
			dev_handler_t func, void *data,
			const char *name)
{
	struct dev_handler_node *node = NULL;

	if (per_cpu(timer_node, pcpu_id) != NULL) {
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

static void init_percpu_timer(uint16_t pcpu_id)
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

static void timer_softirq(uint16_t pcpu_id)
{
	struct per_cpu_timers *cpu_timer;
	struct hv_timer *timer;
	struct list_head *pos, *n;
	int tries = MAX_TIMER_ACTIONS;
	uint64_t current_tsc = rdtsc();

	/* handle passed timer */
	cpu_timer = &per_cpu(cpu_timers, pcpu_id);

	/* This is to make sure we are not blocked due to delay inside func()
	 * force to exit irq handler after we serviced >31 timers
	 * caller used to local_add_timer() for periodic timer, if there is a delay
	 * inside func(), it will infinitely loop here, because new added timer
	 * already passed due to previously func()'s delay.
	 */
	list_for_each_safe(pos, n, &cpu_timer->timer_list) {
		timer = list_entry(pos, struct hv_timer, node);
		/* timer expried */
		tries--;
		if (timer->fire_tsc <= current_tsc && tries > 0) {
			del_timer(timer);

			run_timer(timer);

			if (timer->mode == TICK_MODE_PERIODIC) {
				/* update periodic timer fire tsc */
				timer->fire_tsc += timer->period_in_cycle;
				local_add_timer(cpu_timer, timer, NULL);
			}
		} else {
			break;
		}
	}

	/* update nearest timer */
	update_physical_timer(cpu_timer);
}

void timer_init(void)
{
	char name[32] = {0};
	uint16_t pcpu_id = get_cpu_id();

	init_percpu_timer(pcpu_id);
	register_softirq(SOFTIRQ_TIMER, timer_softirq);

	snprintf(name, 32, "timer_tick[%hu]", pcpu_id);
	if (request_timer_irq(pcpu_id, tsc_deadline_handler, NULL, name) < 0) {
		pr_err("Timer setup failed");
		return;
	}

	init_tsc_deadline_timer();
}

void timer_cleanup(void)
{
	uint16_t pcpu_id = get_cpu_id();

	if (per_cpu(timer_node, pcpu_id) != NULL) {
		unregister_handler_common(per_cpu(timer_node, pcpu_id));
	}

	per_cpu(timer_node, pcpu_id) = NULL;
}

void check_tsc(void)
{
	uint64_t temp64;

	/* Ensure time-stamp timer is turned on for each CPU */
	CPU_CR_READ(cr4, &temp64);
	CPU_CR_WRITE(cr4, (temp64 & ~CR4_TSD));
}

static uint64_t pit_calibrate_tsc(uint16_t cal_ms_arg)
{
#define PIT_TICK_RATE	1193182U
#define PIT_TARGET	0x3FFFU
#define PIT_MAX_COUNT	0xFFFFU

	uint16_t cal_ms = cal_ms_arg;
	uint32_t initial_pit;
	uint16_t current_pit;
	uint16_t max_cal_ms;
	uint64_t current_tsc;
	uint8_t initial_pit_high, initial_pit_low;

	max_cal_ms = ((PIT_MAX_COUNT - PIT_TARGET) * 1000U) / PIT_TICK_RATE;
	cal_ms = min(cal_ms, max_cal_ms);

	/* Assume the 8254 delivers 18.2 ticks per second when 16 bits fully
	 * wrap.  This is about 1.193MHz or a clock period of 0.8384uSec
	 */
	initial_pit = ((uint32_t)cal_ms * PIT_TICK_RATE) / 1000U;
	initial_pit += PIT_TARGET;
	initial_pit_high = (uint8_t)(initial_pit >> 8U);
	initial_pit_low = (uint8_t)initial_pit;

	/* Port 0x43 ==> Control word write; Data 0x30 ==> Select Counter 0,
	 * Read/Write least significant byte first, mode 0, 16 bits.
	 */

	pio_write8(0x30U, 0x43U);
	pio_write8(initial_pit_low, 0x40U);	/* Write LSB */
	pio_write8(initial_pit_high, 0x40U);		/* Write MSB */

	current_tsc = rdtsc();

	do {
		/* Port 0x43 ==> Control word write; 0x00 ==> Select
		 * Counter 0, Counter Latch Command, Mode 0; 16 bits
		 */
		pio_write8(0x00U, 0x43U);

		current_pit = pio_read8(0x40U);	/* Read LSB */
		current_pit |= pio_read8(0x40U) << 8U;	/* Read MSB */
		/* Let the counter count down to PIT_TARGET */
	} while (current_pit > PIT_TARGET);

	current_tsc = rdtsc() - current_tsc;

	return (current_tsc / cal_ms) * 1000U;
}

/*
 * Determine TSC frequency via CPUID 0x15
 */
static uint64_t native_calibrate_tsc(void)
{
	if (boot_cpu_data.cpuid_level >= 0x15U) {
		uint32_t eax_denominator, ebx_numerator, ecx_hz, reserved;

		cpuid(0x15U, &eax_denominator, &ebx_numerator,
			&ecx_hz, &reserved);

		if (eax_denominator != 0U && ebx_numerator != 0U) {
			return ((uint64_t) ecx_hz *
				ebx_numerator) / eax_denominator;
		}
	}

	return 0;
}

void calibrate_tsc(void)
{
	uint64_t tsc_hz;
	tsc_hz = native_calibrate_tsc();
	if (tsc_hz == 0U) {
		tsc_hz = pit_calibrate_tsc(CAL_MS);
	}
	tsc_khz = (uint32_t)(tsc_hz / 1000UL);
	printf("%s, tsc_khz=%lu\n", __func__, tsc_khz);
}
