/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <io.h>
#include <cpuid.h>
#include <cpu_caps.h>
#include <softirq.h>
#include <trace.h>

#define MAX_TIMER_ACTIONS	32U
#define CAL_MS			10U
#define MIN_TIMER_PERIOD_US	500U

static uint32_t tsc_khz = 0U;

uint64_t rdtsc(void)
{
	uint32_t lo, hi;

	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)hi << 32U) | lo;
}

static void run_timer(const struct hv_timer *timer)
{
	/* deadline = 0 means stop timer, we should skip */
	if ((timer->func != NULL) && (timer->fire_tsc != 0UL)) {
		timer->func(timer->priv_data);
	}

	TRACE_2L(TRACE_TIMER_ACTION_PCKUP, timer->fire_tsc, 0UL);
}

/* run in interrupt context */
static void tsc_deadline_handler(__unused uint32_t irq, __unused void *data)
{
	fire_softirq(SOFTIRQ_TIMER);
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

/*
 * return true if we add the timer on the timer_list head
 */
static bool local_add_timer(struct per_cpu_timers *cpu_timer,
			struct hv_timer *timer)
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

	return (prev == &cpu_timer->timer_list);
}

int32_t add_timer(struct hv_timer *timer)
{
	struct per_cpu_timers *cpu_timer;
	uint16_t pcpu_id;
	int32_t ret = 0;

	if ((timer == NULL) || (timer->func == NULL) || (timer->fire_tsc == 0UL)) {
		ret = -EINVAL;
	} else {
		ASSERT(list_empty(&timer->node), "add timer again!\n");

		/* limit minimal periodic timer cycle period */
		if (timer->mode == TICK_MODE_PERIODIC) {
			timer->period_in_cycle = max(timer->period_in_cycle, us_to_ticks(MIN_TIMER_PERIOD_US));
		}

		pcpu_id  = get_cpu_id();
		cpu_timer = &per_cpu(cpu_timers, pcpu_id);

		/* update the physical timer if we're on the timer_list head */
		if (local_add_timer(cpu_timer, timer)) {
			update_physical_timer(cpu_timer);
		}

		TRACE_2L(TRACE_TIMER_ACTION_ADDED, timer->fire_tsc, 0UL);
	}

	return ret;

}

void del_timer(struct hv_timer *timer)
{
	if ((timer != NULL) && !list_empty(&timer->node)) {
		list_del_init(&timer->node);
	}
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
	msr_write(MSR_IA32_EXT_APIC_LVT_TIMER, val);
	cpu_memory_barrier();

	/* disarm timer */
	msr_write(MSR_IA32_TSC_DEADLINE, 0UL);
}

static void timer_softirq(uint16_t pcpu_id)
{
	struct per_cpu_timers *cpu_timer;
	struct hv_timer *timer;
	struct list_head *pos, *n;
	uint32_t tries = MAX_TIMER_ACTIONS;
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
		if ((timer->fire_tsc <= current_tsc) && (tries != 0U)) {
			del_timer(timer);

			run_timer(timer);

			if (timer->mode == TICK_MODE_PERIODIC) {
				/* update periodic timer fire tsc */
				timer->fire_tsc += timer->period_in_cycle;
				(void)local_add_timer(cpu_timer, timer);
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
	uint16_t pcpu_id = get_cpu_id();
	int32_t retval = 0;

	init_percpu_timer(pcpu_id);

	if (pcpu_id == BOOT_CPU_ID) {
		register_softirq(SOFTIRQ_TIMER, timer_softirq);

		retval = request_irq(TIMER_IRQ, (irq_action_t)tsc_deadline_handler, NULL, IRQF_NONE);
		if (retval < 0) {
			pr_err("Timer setup failed");
		}
	}

	if (retval >= 0) {
		init_tsc_deadline_timer();
	}
}

static uint64_t pit_calibrate_tsc(uint32_t cal_ms_arg)
{
#define PIT_TICK_RATE	1193182U
#define PIT_TARGET	0x3FFFU
#define PIT_MAX_COUNT	0xFFFFU

	uint32_t cal_ms = cal_ms_arg;
	uint32_t initial_pit;
	uint16_t current_pit;
	uint32_t max_cal_ms;
	uint64_t current_tsc;
	uint8_t initial_pit_high, initial_pit_low;

	max_cal_ms = ((PIT_MAX_COUNT - PIT_TARGET) * 1000U) / PIT_TICK_RATE;
	cal_ms = min(cal_ms, max_cal_ms);

	/* Assume the 8254 delivers 18.2 ticks per second when 16 bits fully
	 * wrap.  This is about 1.193MHz or a clock period of 0.8384uSec
	 */
	initial_pit = (cal_ms * PIT_TICK_RATE) / 1000U;
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

		current_pit = (uint16_t)pio_read8(0x40U);	/* Read LSB */
		current_pit |= (uint16_t)pio_read8(0x40U) << 8U;	/* Read MSB */
		/* Let the counter count down to PIT_TARGET */
	} while (current_pit > PIT_TARGET);

	current_tsc = rdtsc() - current_tsc;

	return (current_tsc / cal_ms) * 1000U;
}

/*
 * Determine TSC frequency via CPUID 0x15 and 0x16.
 */
static uint64_t native_calibrate_tsc(void)
{
	uint64_t tsc_hz = 0UL;
	struct cpuinfo_x86 *cpu_info = get_cpu_info();

	if (cpu_info->cpuid_level >= 0x15U) {
		uint32_t eax_denominator, ebx_numerator, ecx_hz, reserved;

		cpuid(0x15U, &eax_denominator, &ebx_numerator,
			&ecx_hz, &reserved);

		if ((eax_denominator != 0U) && (ebx_numerator != 0U)) {
			tsc_hz = ((uint64_t) ecx_hz *
				ebx_numerator) / eax_denominator;
		}
	}

	if ((tsc_hz == 0UL) && (cpu_info->cpuid_level >= 0x16U)) {
		uint32_t eax_base_mhz, ebx_max_mhz, ecx_bus_mhz, edx;
		cpuid(0x16U, &eax_base_mhz, &ebx_max_mhz, &ecx_bus_mhz, &edx);
		tsc_hz = (uint64_t) eax_base_mhz * 1000000U;
	}

	return tsc_hz;
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

uint32_t get_tsc_khz(void)
{
	return tsc_khz;
}

/**
 * Frequency of TSC in KHz (where 1KHz = 1000Hz). Only valid after
 * calibrate_tsc() returns.
 */

uint64_t us_to_ticks(uint32_t us)
{
	return (((uint64_t)us * (uint64_t)tsc_khz) / 1000UL);
}

uint64_t ticks_to_us(uint64_t ticks)
{
	return (ticks * 1000UL) / (uint64_t)tsc_khz;
}

uint64_t ticks_to_ms(uint64_t ticks)
{
	return ticks / (uint64_t)tsc_khz;
}
