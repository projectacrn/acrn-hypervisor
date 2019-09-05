/*-
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_CORE_SW_LOAD_
#define _CORE_SW_LOAD_

#define STR_LEN 1024
#define BOOT_ARG_LEN 2048

/* E820 memory types */
#define E820_TYPE_RAM           1U   /* EFI 1, 2, 3, 4, 5, 6, 7 */
/* EFI 0, 11, 12, 13 (everything not used elsewhere) */
#define E820_TYPE_RESERVED      2U
#define E820_TYPE_ACPI_RECLAIM  3U   /* EFI 9 */
#define E820_TYPE_ACPI_NVS      4U   /* EFI 10 */
#define E820_TYPE_UNUSABLE      5U   /* EFI 8 */

#define NUM_E820_ENTRIES        9
#define LOWRAM_E820_ENTRY       2
#define HIGHRAM_E820_ENTRY      8

/* Defines a single entry in an E820 memory map. */
struct e820_entry {
	/** The base address of the memory range. */
	uint64_t baseaddr;
	/** The length of the memory range. */
	uint64_t length;
	/** The type of memory region. */
	uint32_t type;
} __attribute__((packed));

extern const struct e820_entry e820_default_entries[NUM_E820_ENTRIES];
extern int with_bootargs;
extern bool writeback_nv_storage;

size_t ovmf_image_size(void);

int acrn_parse_kernel(char *arg);
int acrn_parse_ramdisk(char *arg);
int acrn_parse_bootargs(char *arg);
int acrn_parse_gvtargs(char *arg);
int acrn_parse_vsbl(char *arg);
int acrn_parse_ovmf(char *arg);
int acrn_parse_elf(char *arg);
int acrn_parse_guest_part_info(char *arg);
char *get_bootargs(void);
void vsbl_set_bdf(int bnum, int snum, int fnum);

int check_image(char *path, size_t size_limit, size_t *size);
uint32_t acrn_create_e820_table(struct vmctx *ctx, struct e820_entry *e820);
int add_e820_entry(struct e820_entry *e820, int len, uint64_t start,
	uint64_t size, uint32_t type);

int acrn_sw_load_bzimage(struct vmctx *ctx);
int acrn_sw_load_elf(struct vmctx *ctx);
int acrn_sw_load_vsbl(struct vmctx *ctx);
int acrn_sw_load_ovmf(struct vmctx *ctx);
int acrn_writeback_ovmf_nvstorage(struct vmctx *ctx);
int acrn_sw_load(struct vmctx *ctx);
#endif

