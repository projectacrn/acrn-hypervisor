/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IDT_H
#define IDT_H

/*
 * IDT is defined in assembly so we handle exceptions as early as possible.
 */

#ifndef ASSEMBLER

/* Number of the HOST IDT entries */
#define     HOST_IDT_ENTRIES    (0x100U)

/*
 * Definition of an 16 byte IDT selector.
 */
union idt_64_descriptor {
	uint64_t value;
	struct {
		union {
			uint32_t value;
			struct {
				uint32_t offset_15_0:16;
				uint32_t seg_sel:16;
			} bits;
		} low32;
		union {
			uint32_t value;
			struct {
				uint32_t ist:3;
				uint32_t bit_3_clr:1;
				uint32_t bit_4_clr:1;
				uint32_t bits_5_7_clr:3;
				uint32_t type:4;
				uint32_t bit_12_clr:1;
				uint32_t dpl:2;
				uint32_t present:1;
				uint32_t offset_31_16:16;
			} bits;
		} high32;
		uint32_t offset_63_32;
		uint32_t rsvd;
	} fields;
} __aligned(8);

/*****************************************************************************
 *
 * Definition of the IDT.
 *
 *****************************************************************************/
struct host_idt {
	union idt_64_descriptor host_idt_descriptors[HOST_IDT_ENTRIES];
} __aligned(8);

/*
 * Definition of the IDT descriptor.
 */
struct host_idt_descriptor {
	uint16_t len;
	struct host_idt *idt;
} __packed;

extern struct host_idt HOST_IDT;
extern struct host_idt_descriptor HOST_IDTR;

#else /* ASSEMBLER */

/* Interrupt Descriptor Table (LDT) selectors are 16 bytes on x86-64 instead of 8 bytes. */
#define     X64_IDT_DESC_SIZE   (0x10)
/* Number of the HOST IDT entries */
#define     HOST_IDT_ENTRIES    (0x100)
/* Size of the IDT */
#define     HOST_IDT_SIZE       (HOST_IDT_ENTRIES * X64_IDT_DESC_SIZE)

#endif /* end #ifndef ASSEMBLER */

#endif /* IDT_H */
