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
#include <asm/cpu.h>
#include <logmsg.h>
#include <acpi.h>

#define CAL_MS	10U

#define HPET_PERIOD	0x004U
#define HPET_CFG	0x010U
#define HPET_COUNTER	0x0F0U

#define HPET_CFG_ENABLE	0x001UL

static uint32_t tsc_khz;
static void *hpet_hva;

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

void hpet_init(void)
{
	uint64_t cfg;

	hpet_hva = parse_hpet();
	if (hpet_hva != NULL) {
		cfg = mmio_read64(hpet_hva + HPET_CFG);
		if ((cfg & HPET_CFG_ENABLE) == 0UL) {
			cfg |= HPET_CFG_ENABLE;
			mmio_write64(cfg, hpet_hva + HPET_CFG);
		}
	}
}

static inline bool is_hpet_capable(void)
{
	return (hpet_hva != NULL);
}

static inline uint32_t hpet_read(uint32_t offset)
{
	return mmio_read32(hpet_hva + offset);
}

static inline uint64_t tsc_read_hpet(uint64_t *p)
{
	uint64_t current_tsc;

	/* read hpet first */
	*p = hpet_read(HPET_COUNTER);
	current_tsc = rdtsc();

	return current_tsc;
}

static uint64_t hpet_calibrate_tsc(uint32_t cal_ms_arg)
{
	uint64_t tsc1, tsc2, hpet1, hpet2;
	uint64_t delta_tsc, delta_fs;
	uint64_t rflags, tsc_khz;

	CPU_INT_ALL_DISABLE(&rflags);
	tsc1 = tsc_read_hpet(&hpet1);
	pit_calibrate_tsc(cal_ms_arg);
	tsc2 = tsc_read_hpet(&hpet2);
	CPU_INT_ALL_RESTORE(rflags);

	/* in case counter wrap happened in the low 32 bits */
	if (hpet2 <= hpet1) {
		hpet2 |= (1UL << 32U);
	}
	delta_fs = (hpet2 - hpet1) * hpet_read(HPET_PERIOD);
	delta_tsc = tsc2 - tsc1;
	/*
	 * FS_PER_S = 10 ^ 15
	 *
	 * tsc_khz = delta_tsc / (delta_fs / FS_PER_S) / 1000UL;
	 *         = delta_tsc / delta_fs * (10 ^ 12)
	 *         = (delta_tsc * (10 ^ 6)) / (delta_fs / (10 ^ 6))
	 */
	tsc_khz = (delta_tsc * 1000000UL) / (delta_fs / 1000000UL);
	return tsc_khz * 1000U;
}

static uint64_t pit_hpet_calibrate_tsc(uint32_t cal_ms_arg, uint64_t tsc_ref_hz)
{
	uint64_t tsc_hz, delta;

	if (is_hpet_capable()) {
		tsc_hz = hpet_calibrate_tsc(cal_ms_arg);
	} else {
		tsc_hz = pit_calibrate_tsc(cal_ms_arg);
	}

	if (tsc_ref_hz != 0UL) {
		delta = (tsc_hz * 100UL) / tsc_ref_hz;
		if ((delta < 95UL) || (delta > 105UL)) {
			tsc_hz = tsc_ref_hz;
		}
	}

	return tsc_hz;
}

/*
 * Determine TSC frequency via CPUID 0x15.
 */
static uint64_t native_calculate_tsc_cpuid_0x15(void)
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

	return tsc_hz;
}

/*
 * Determine TSC frequency via CPUID 0x16.
 */
static uint64_t native_calculate_tsc_cpuid_0x16(void)
{
	uint64_t tsc_hz = 0UL;
	const struct cpuinfo_x86 *cpu_info = get_pcpu_info();

	if (cpu_info->cpuid_level >= 0x16U) {
		uint32_t eax_base_mhz, ebx_max_mhz, ecx_bus_mhz, edx;

		cpuid_subleaf(0x16U, 0x0U, &eax_base_mhz, &ebx_max_mhz, &ecx_bus_mhz, &edx);
		tsc_hz = (uint64_t) eax_base_mhz * 1000000U;
	}

	return tsc_hz;
}

void calibrate_tsc(void)
{
	uint64_t tsc_hz;

	tsc_hz = native_calculate_tsc_cpuid_0x15();
	if (tsc_hz == 0UL) {
		tsc_hz = pit_hpet_calibrate_tsc(CAL_MS, native_calculate_tsc_cpuid_0x16());
	}
	tsc_khz = (uint32_t)(tsc_hz / 1000UL);
	pr_acrnlog("%s: tsc_khz = %ld", __func__, tsc_khz);
}

uint32_t get_tsc_khz(void)
{
	return tsc_khz;
}

/* external API */

uint64_t arch_cpu_ticks(void)
{
	return rdtsc();
}

uint32_t arch_cpu_tickrate(void)
{
	return tsc_khz;
}
