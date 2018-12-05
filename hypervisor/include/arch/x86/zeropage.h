/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
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
	struct e820_entry entries[0x80];	/* 0x2d0 */
	uint8_t pad4[0x330];	/* 0xcd0 */
} __packed;

#endif
