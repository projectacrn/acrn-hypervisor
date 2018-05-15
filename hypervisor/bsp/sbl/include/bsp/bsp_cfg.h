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

#ifndef BSP_CFG_H
#define BSP_CFG_H
#define NR_IOAPICS	1
#define STACK_SIZE	8192
#define LOG_BUF_SIZE	0x100000
#define LOG_DESTINATION	3
#define CPU_UP_TIMEOUT	100
#define CONFIG_SERIAL_MMIO_BASE	0xfc000000
#define MALLOC_ALIGN	16
#define NUM_ALLOC_PAGES	4096
#define HEAP_SIZE	0x100000
#define CONSOLE_LOGLEVEL_DEFAULT	2
#define MEM_LOGLEVEL_DEFAULT		4
#define	CONFIG_LOW_RAM_START	0x00001000
#define	CONFIG_LOW_RAM_SIZE	0x000CF000
#define	CONFIG_RAM_START	0x6E000000
#define	CONFIG_RAM_SIZE		0x02000000	/* 32M */
#endif /* BSP_CFG_H */
