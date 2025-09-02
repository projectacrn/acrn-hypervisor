/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <types.h>
#include <asm/sbi.h>
#include <debug/logmsg.h>

/**
 * An ECALL is used as the control transfer instruction between the
 * supervisor and the SEE.
 *
 * a7 encodes the SBI extension ID (EID).
 *
 * a6 encodes the SBI function ID (FID) for a given extension ID
 * encoded in a7 for any SBI extension defined in or after SBI v0.2.
 *
 * a0 through a5 contain the arguments for the SBI function call.
 * Registers that are not defined in the SBI function call are not
 * reserved.
 *
 * All registers except a0 & a1 must be preserved across an SBI call
 * by the callee.
 *
 * SBI functions must return a pair of values in a0 and a1, with a0
 * returning an error code. This is analogous to returning the C
 * structure.
 */
static sbiret sbi_ecall(uint64_t arg0, uint64_t arg1, uint64_t arg2,
				uint64_t arg3, uint64_t arg4, uint64_t arg5,
				uint64_t func, uint64_t ext)
{
	sbiret ret;

	register uint64_t a0 asm ("a0") = arg0;
	register uint64_t a1 asm ("a1") = arg1;
	register uint64_t a2 asm ("a2") = arg2;
	register uint64_t a3 asm ("a3") = arg3;
	register uint64_t a4 asm ("a4") = arg4;
	register uint64_t a5 asm ("a5") = arg5;
	register uint64_t a6 asm ("a6") = func;
	register uint64_t a7 asm ("a7") = ext;

	asm volatile (
		"ecall \n\t"
		:"+r" (a0), "+r" (a1)
		:"r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		: "memory"
	);

	ret.error = a0;
	ret.value = a1;

	return ret;
}

/**
 * Implemented IPI functionality using the SBI IPI Extension (EID #0x735049).
 * Legacy SBI extensions are not supported in ACRN.
 */
static int64_t sbi_send_ipi(uint64_t mask, uint64_t mask_base)
{
	sbiret ret = sbi_ecall(mask, mask_base, 0UL, 0UL, 0UL, 0UL, SBI_IPI_FID_SEND_IPI, SBI_EID_IPI);

	if (ret.error != SBI_SUCCESS) {
		pr_err("%s: Failed to send IPI by SBI, error code: %lx", __func__, ret.error);
	}

	return ret.error;
}

/**
 * msg_type is currently unused.
 *
 * At present, only IPI_NOTIFY_CPU is supported, covering two use cases:
 *  - SMP call
 *  - Kick pCPU out of non-root mode
 *
 * Callers should invoke this function with:
 *      arch_send_single_ipi(pcpu_id, IPI_NOTIFY_CPU);
 *
 * msg_type is retained for future extensions and to stay aligned with
 * the function prototype used on other architectures (e.g. x86).
 */
void arch_send_single_ipi(uint16_t pcpu_id, __unused uint32_t msg_type)
{
	sbi_send_ipi((1UL << pcpu_id), 0UL);
}

/**
 * Similar to arch_send_single_ipi() regards to msg_type.
 *
 * Callers should invoke this function with:
 *      arch_send_dest_ipi_mask(dest_mask, IPI_NOTIFY_CPU);
 */
void arch_send_dest_ipi_mask(uint64_t dest_mask, __unused uint32_t msg_type)
{
	sbi_send_ipi(dest_mask, 0UL);
}
