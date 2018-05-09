/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDT_H
#define IDT_H

/*
 * IDT is defined in assembly so we handle exceptions as early as possible.
 */

/* Interrupt Descriptor Table (LDT) selectors are 16 bytes on x86-64 instead of
 * 8 bytes.
 */
#define     X64_IDT_DESC_SIZE   (0x10)
/* Number of the HOST IDT entries */
#define     HOST_IDT_ENTRIES    (0x100)
/* Size of the IDT */
#define     HOST_IDT_SIZE       (HOST_IDT_ENTRIES * X64_IDT_DESC_SIZE)

#ifndef ASSEMBLER

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
				uint32_t segment_sel:16;
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
	};
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
	unsigned short len;
	struct host_idt *idt;
} __packed;

extern struct host_idt HOST_IDT;
extern struct host_idt_descriptor HOST_IDTR;

static inline void set_idt(struct host_idt_descriptor *idtd)
{

	asm volatile ("   lidtq %[idtd]\n" :	/* no output parameters */
		      :		/* input parameters */
		      [idtd] "m"(*idtd));
}

#endif /* end #ifndef ASSEMBLER */

#endif /* IDT_H */
