/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

	.section .text.entry, "ax", %progbits

	/*
	 * main entry point
	 *  - a0 = hart ID
	 *  - a1 = dtb address
	 */
	.globl _start
_start:
	/* Mask all interrupts */
	csrw	sie, zero

	/* Disable FPU bit[14:13] */
	li	t0, 0x00006000
	csrc	sstatus, t0

	/* Set trap vector to spin forever for debug */
	lla	a3, _start_hang
	csrw	stvec, a3

	/* Clear BSS */
	lla	t3, _bss_start
	lla	t4, _bss_end
_clear_bss:
	sd	zero, (t3)
	add	t3, t3, __SIZEOF_POINTER__
	bltu	t3, t4, _clear_bss

	/* Setup cpu0 boot stack (full descending) */
	lla	sp, _boot_stack_end

	/* a0 = hart_id, a1 = dtb_address */
	tail	init_primary_pcpu

	.align 2
	.global _start_secondary_sbi
_start_secondary_sbi:
	/* Mask all interrupts */
	csrw	sie, zero

	/* Disable FPU bit[14:13] */
	li	t0, 0x00006000
	csrc	sstatus, t0

	/* Set trap vector to spin forever for debug */
	lla	a3, _start_hang
	csrw	stvec, a3

	/* a0 contains the hartid & a1 contains stack physical addr */
	mv	sp, a1

	tail	init_secondary_pcpu

	.align 2
_start_hang:
	wfi
	j	_start_hang

