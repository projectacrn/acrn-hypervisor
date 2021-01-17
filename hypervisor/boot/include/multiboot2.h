/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*   multiboot2.h - Multiboot 2 header file. */
/*   Copyright (C) 1999,2003,2007,2008,2009,2010  Free Software Foundation, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL ANY
 *  DEVELOPER OR DISTRIBUTOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#define MULTIBOOT2_HEADER_ALIGN				8

#define MULTIBOOT2_HEADER_MAGIC				0xe85250d6U

/*  This should be in %eax. */
#define MULTIBOOT2_INFO_MAGIC				0x36d76289U

/*  Alignment of the multiboot info structure. */
#define MULTIBOOT2_INFO_ALIGN				0x00000008U

/*  Flags set in the 'flags' member of the multiboot header. */

#define MULTIBOOT2_TAG_ALIGN				8U
#define MULTIBOOT2_TAG_TYPE_END				0U
#define MULTIBOOT2_TAG_TYPE_CMDLINE			1U
#define MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME		2U
#define MULTIBOOT2_TAG_TYPE_MODULE			3U
#define MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO		4U
#define MULTIBOOT2_TAG_TYPE_BOOTDEV			5U
#define MULTIBOOT2_TAG_TYPE_MMAP			6U
#define MULTIBOOT2_TAG_TYPE_VBE				7U
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER			8U
#define MULTIBOOT2_TAG_TYPE_ELF_SECTIONS		9U
#define MULTIBOOT2_TAG_TYPE_APM				10U
#define MULTIBOOT2_TAG_TYPE_EFI32			11U
#define MULTIBOOT2_TAG_TYPE_EFI64			12U
#define MULTIBOOT2_TAG_TYPE_SMBIOS			13U
#define MULTIBOOT2_TAG_TYPE_ACPI_OLD			14U
#define MULTIBOOT2_TAG_TYPE_ACPI_NEW			15U
#define MULTIBOOT2_TAG_TYPE_NETWORK			16U
#define MULTIBOOT2_TAG_TYPE_EFI_MMAP			17U
#define MULTIBOOT2_TAG_TYPE_EFI_BS			18U
#define MULTIBOOT2_TAG_TYPE_EFI32_IH			19U
#define MULTIBOOT2_TAG_TYPE_EFI64_IH			20U
#define MULTIBOOT2_TAG_TYPE_LOAD_BASE_ADDR		21U

#define MULTIBOOT2_HEADER_TAG_END			0
#define MULTIBOOT2_HEADER_TAG_INFORMATION_REQUEST	1
#define MULTIBOOT2_HEADER_TAG_ADDRESS			2
#define MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS		3
#define MULTIBOOT2_HEADER_TAG_CONSOLE_FLAGS		4
#define MULTIBOOT2_HEADER_TAG_FRAMEBUFFER		5
#define MULTIBOOT2_HEADER_TAG_MODULE_ALIGN		6
#define MULTIBOOT2_HEADER_TAG_EFI_BS			7
#define MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS_EFI32	8
#define MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS_EFI64	9
#define MULTIBOOT2_HEADER_TAG_RELOCATABLE		10

#define MULTIBOOT2_ARCHITECTURE_I386			0
#define MULTIBOOT2_ARCHITECTURE_MIPS32			4

#endif /*  MULTIBOOT2_H */
