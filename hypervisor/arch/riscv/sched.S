/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

	.text

	.align 8
	.global arch_switch_to
arch_switch_to:

/* refer to struct stack_frame */
	addi sp, sp, -0x80
	/*
	 * 0x0=STACK_FRAME_OFFSET_RA
	 * 0x8=STACK_FRAME_OFFSET_S0
	 * 0x10=STACK_FRAME_OFFSET_S1
	 * 0x18=STACK_FRAME_OFFSET_S2
	 * 0x20=STACK_FRAME_OFFSET_S3
	 * 0x28=STACK_FRAME_OFFSET_S4
	 * 0x30=STACK_FRAME_OFFSET_S5
	 * 0x38=STACK_FRAME_OFFSET_S6
	 * 0x40=STACK_FRAME_OFFSET_S7
	 * 0x48=STACK_FRAME_OFFSET_S8
	 * 0x50=STACK_FRAME_OFFSET_S9
	 * 0x58=STACK_FRAME_OFFSET_S10
	 * 0x60=STACK_FRAME_OFFSET_S11
	 * 0x68=STACK_FRAME_OFFSET_A0
	 */
	sd ra, 0x0(sp)
	sd s0, 0x8(sp)
	sd s1, 0x10(sp)
	sd s2, 0x18(sp)
	sd s3, 0x20(sp)
	sd s4, 0x28(sp)
	sd s5, 0x30(sp)
	sd s6, 0x38(sp)
	sd s7, 0x40(sp)
	sd s8, 0x48(sp)
	sd s9, 0x50(sp)
	sd s10, 0x58(sp)
	sd s11, 0x60(sp)
	sd a0, 0x68(sp)
	sd sp, 0(a0)

	ld sp, 0(a1)
	/*
	 * 0x0=STACK_FRAME_OFFSET_RA
	 * 0x8=STACK_FRAME_OFFSET_S0
	 * 0x10=STACK_FRAME_OFFSET_S1
	 * 0x18=STACK_FRAME_OFFSET_S2
	 * 0x20=STACK_FRAME_OFFSET_S3
	 * 0x28=STACK_FRAME_OFFSET_S4
	 * 0x30=STACK_FRAME_OFFSET_S5
	 * 0x38=STACK_FRAME_OFFSET_S6
	 * 0x40=STACK_FRAME_OFFSET_S7
	 * 0x48=STACK_FRAME_OFFSET_S8
	 * 0x50=STACK_FRAME_OFFSET_S9
	 * 0x58=STACK_FRAME_OFFSET_S10
	 * 0x60=STACK_FRAME_OFFSET_S11
	 * 0x68=STACK_FRAME_OFFSET_A0
	 */
	ld ra, 0x0(sp)
	ld s0, 0x8(sp)
	ld s1, 0x10(sp)
	ld s2, 0x18(sp)
	ld s3, 0x20(sp)
	ld s4, 0x28(sp)
	ld s5, 0x30(sp)
	ld s6, 0x38(sp)
	ld s7, 0x40(sp)
	ld s8, 0x48(sp)
	ld s9, 0x50(sp)
	ld s10, 0x58(sp)
	ld s11, 0x60(sp)
	ld a0, 0x68(sp)
	addi sp, sp, 0x80

	ret
