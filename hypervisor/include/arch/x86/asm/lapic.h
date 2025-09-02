/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_X86_LAPIC_H
#define ARCH_X86_LAPIC_H

#include <types.h>

/* x2APIC Interrupt Command Register (ICR) structure */
union apic_icr {
	uint64_t value;
	struct {
		uint32_t lo_32;
		uint32_t hi_32;
	} value_32;
	struct {
		uint32_t vector:8;
		uint32_t delivery_mode:3;
		uint32_t destination_mode:1;
		uint32_t rsvd_1:2;
		uint32_t level:1;
		uint32_t trigger_mode:1;
		uint32_t rsvd_2:2;
		uint32_t shorthand:2;
		uint32_t rsvd_3:12;
		uint32_t dest_field:32;
	} bits;
};

/**
 * @defgroup lapic_ext_apis LAPIC External Interfaces
 *
 * This is a group that includes LAPIC External Interfaces.
 *
 * @{
 */

/**
 * @brief Enable LAPIC in x2APIC mode
 *
 * Enable LAPIC in x2APIC mode via MSR writes.
 *
 */
void early_init_lapic(void);

/**
 * @brief Suspend LAPIC
 *
 * Suspend LAPIC by getting the APIC base addr and saving the registers.
 */
void suspend_lapic(void);

/**
 * @brief Resume LAPIC
 *
 * Resume LAPIC by setting the APIC base addr and restoring the registers.
 */
void resume_lapic(void);

/**
 * @brief Get the LAPIC ID
 *
 * Get the LAPIC ID via MSR read.
 *
 * @return LAPIC ID
 */
uint32_t get_cur_lapic_id(void);

/**
 * @}
 */
/* End of lapic_ext_apis */

void init_lapic(uint16_t pcpu_id);
void send_lapic_eoi(void);

/**
 * @defgroup ipi_ext_apis IPI External Interfaces
 *
 * This is a group that includes IPI External Interfaces.
 *
 * @{
 */

/**
 * @brief Send an SIPI to a specific cpu
 *
 * Send an Startup IPI to a specific cpu, to notify the cpu to start booting.
 *
 * @param[in]	dest_pcpu_id The id of destination physical cpu
 * @param[in]	cpu_startup_start_address The address for the dest pCPU to start running
 *
 */
void send_startup_ipi(uint16_t dest_pcpu_id, uint64_t cpu_startup_start_address);

/**
 * @brief Send an IPI to multiple pCPUs
 *
 * @param[in]	dest_mask The mask of destination physical cpus
 * @param[in]	vector The vector of interrupt
 */
void send_dest_ipi_mask(uint64_t dest_mask, uint32_t vector);

/**
 * @brief Send an IPI to a single pCPU
 *
 * @param[in]	pcpu_id The id of destination physical cpu
 * @param[in]	vector The vector of interrupt
 */
void send_single_ipi(uint16_t pcpu_id, uint32_t vector);

/**
 * @}
 */
/* End of ipi_ext_apis */

/**
 * @brief Send an INIT signal to a single pCPU
 *
 * @param[in] pcpu_id The id of destination physical cpu
 */
void send_single_init(uint16_t pcpu_id);

void kick_pcpu(uint16_t pcpu_id);

#endif /* ARCH_X86_LAPIC_H */
