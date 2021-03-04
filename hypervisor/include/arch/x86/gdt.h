/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GDT_H
#define GDT_H

/* GDT is defined in assembly so it can be used to switch modes before long mode
 * is established.
 * With 64-bit EFI this is not required since are already in long mode when EFI
 * transfers control to the hypervisor.  However, for any instantiation of the
 * ACRN Hypervisor that requires a boot from reset the GDT will be
 * used as mode transitions are being made to ultimately end up in long mode.
 * For this reason we establish the GDT in assembly.
 * This should not affect usage and convenience of interacting with the GDT in C
 * as the complete definition of the GDT is driven by the defines in this file.
 *
 * Unless it proves to be not viable we will use a single GDT for all hypervisor
 * CPUs, with space for per CPU LDT and TSS.
 */

/*
 * Segment selectors in x86-64 and i386 are the same size, 8 bytes.
 * Local Descriptor Table (LDT) selectors are 16 bytes on x86-64 instead of 8
 * bytes.
 * Task State Segment (TSS) selectors are 16 bytes on x86-64 instead of 8 bytes.
 */
#define X64_SEG_DESC_SIZE (0x8U)	/* In long mode SEG Descriptors are 8 bytes */
#define X64_LDT_DESC_SIZE (0x10U)/* In long mode LDT Descriptors are 16 bytes */
#define X64_TSS_DESC_SIZE (0x10U)/* In long mode TSS Descriptors are 16 bytes */

/*****************************************************************************
 *
 * BEGIN: Definition of the GDT.
 *
 * NOTE:
 * If you change the size of the GDT or rearrange the location of descriptors
 * within the GDT you must change both the defines and the C structure header.
 *
 *****************************************************************************/
/* Number of global 8 byte segments descriptor(s) */
#define    HOST_GDT_RING0_SEG_SELECTORS   (0x3U)	/* rsvd, code, data */
/* Offsets of global 8 byte segment descriptors */
#define    HOST_GDT_RING0_RSVD_SEL        (0x0000U)
#define    HOST_GDT_RING0_CODE_SEL        (0x0008U)
#define    HOST_GDT_RING0_DATA_SEL        (0x0010U)
/* Number of global 16 byte LDT descriptor(s) */
#define    HOST_GDT_RING0_TSS_SELECTORS   (0x1U)
/* One for each CPU in the hypervisor. */

/*****************************************************************************
 *
 * END: Definition of the GDT.
 *
 *****************************************************************************/

/* Offset to start of LDT Descriptors */
#define HOST_GDT_RING0_LDT_SEL		\
	(HOST_GDT_RING0_SEG_SELECTORS * X64_SEG_DESC_SIZE)
/* Offset to start of LDT Descriptors */
#define HOST_GDT_RING0_CPU_TSS_SEL (HOST_GDT_RING0_LDT_SEL)
/* Size of the GDT */
#define HOST_GDT_SIZE							\
	(HOST_GDT_RING0_CPU_TSS_SEL +					\
		(HOST_GDT_RING0_TSS_SELECTORS * X64_TSS_DESC_SIZE))

#ifndef ASSEMBLER

#include <types.h>
#include <x86/cpu.h>

#define TSS_AVAIL  (9U)

/*
 * Definition of 16 byte TSS and LDT selectors.
 */
struct tss_64_descriptor {
		uint32_t low32_value;
		uint32_t high32_value;
		uint32_t base_addr_63_32;
		uint32_t offset_12;
} __aligned(8);

/*****************************************************************************
 *
 * BEGIN: Definition of the GDT.
 *
 * NOTE:
 * If you change the size of the GDT or rearrange the location of descriptors
 * within the GDT you must change both the defines and the C structure header.
 *
 *****************************************************************************/
struct host_gdt {
	uint64_t rsvd;

	uint64_t code_segment_descriptor;
	uint64_t data_segment_descriptor;
	struct tss_64_descriptor host_gdt_tss_descriptors;
} __aligned(8);

/*****************************************************************************
 *
 * END: Definition of the GDT.
 *
 *****************************************************************************/

/*
 * x86-64 Task State Segment (TSS) definition.
 */
struct tss_64 {
	uint32_t rsvd1;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint32_t rsvd2;
	uint32_t rsvd3;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint32_t rsvd4;
	uint32_t rsvd5;
	uint16_t rsvd6;
	uint16_t io_map_base_addr;
} __packed __aligned(16);

/*
 * Definition of the GDT descriptor.
 */
struct host_gdt_descriptor {
	uint16_t len;
	struct host_gdt *gdt;
} __packed;

void load_gdtr_and_tr(void);

#endif /* end #ifndef ASSEMBLER */

#endif /* GDT_H */
