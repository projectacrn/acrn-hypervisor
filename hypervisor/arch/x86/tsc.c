/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <util.h>
#include <asm/cpuid.h>
#include <asm/cpu_caps.h>
#include <asm/io.h>
#include <asm/tsc.h>

#define CAL_MS	10U

static uint32_t tsc_khz;

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
	const struct cpuinfo_x86 *cpu_info = get_pcpu_info();

	if (cpu_info->cpuid_level >= 0x15U) {
		uint32_t eax_denominator, ebx_numerator, ecx_hz, reserved;

		cpuid_subleaf(0x15U, 0x0U, &eax_denominator, &ebx_numerator,
			&ecx_hz, &reserved);

		if ((eax_denominator != 0U) && (ebx_numerator != 0U)) {
			tsc_hz = ((uint64_t) ecx_hz *
				ebx_numerator) / eax_denominator;
		}
	}

	if ((tsc_hz == 0UL) && (cpu_info->cpuid_level >= 0x16U)) {
		uint32_t eax_base_mhz, ebx_max_mhz, ecx_bus_mhz, edx;

		cpuid_subleaf(0x16U, 0x0U, &eax_base_mhz, &ebx_max_mhz, &ecx_bus_mhz, &edx);
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
}

uint32_t get_tsc_khz(void)
{
	return tsc_khz;
}

/* external API */

uint64_t cpu_ticks(void)
{
	return rdtsc();
}

uint32_t cpu_tickrate(void)
{
	return tsc_khz;
}
