/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
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
#define CONSOLE_LOGLEVEL_DEFAULT	3
#define MEM_LOGLEVEL_DEFAULT		5
#define	CONFIG_LOW_RAM_START	0x00001000
#define	CONFIG_LOW_RAM_SIZE	0x000CF000
#define	CONFIG_RAM_START	0x6E000000
#define	CONFIG_RAM_SIZE		0x02000000	/* 32M */
#endif /* BSP_CFG_H */
