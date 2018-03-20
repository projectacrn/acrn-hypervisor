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

#ifndef ZEROPAGE_H
#define ZEROPAGE_H

struct zero_page {
	uint8_t pad1[0x1e8];	/* 0x000 */
	uint8_t e820_nentries;	/* 0x1e8 */
	uint8_t pad2[0x8];	/* 0x1e9 */

	struct {
		uint8_t setup_sects;	/* 0x1f1 */
		uint8_t hdr_pad1[0x1e];	/* 0x1f2 */
		uint8_t loader_type;	/* 0x210 */
		uint8_t load_flags;	/* 0x211 */
		uint8_t hdr_pad2[0x6];	/* 0x212 */
		uint32_t ramdisk_addr;	/* 0x218 */
		uint32_t ramdisk_size;	/* 0x21c */
		uint8_t hdr_pad3[0x8];	/* 0x220 */
		uint32_t bootargs_addr;	/* 0x228 */
		uint8_t hdr_pad4[0x8];	/* 0x22c */
		uint8_t relocatable_kernel; /* 0x234 */
		uint8_t hdr_pad5[0x13];    /* 0x235 */
		uint32_t payload_offset;/* 0x248 */
		uint32_t payload_length;/* 0x24c */
		uint8_t hdr_pad6[0x8];	/* 0x250 */
		uint64_t pref_addr;     /* 0x258 */
		uint8_t hdr_pad7[8];    /* 0x260 */
	} __packed hdr;

	uint8_t pad3[0x68];	/* 0x268 */
	struct e820_entry e820[0x80];	/* 0x2d0 */
	uint8_t pad4[0x330];	/* 0xcd0 */
} __packed;

#endif
