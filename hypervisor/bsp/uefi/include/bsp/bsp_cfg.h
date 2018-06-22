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
#define CONFIG_SERIAL_PIO_BASE	0x3f8
#define MALLOC_ALIGN	16
#define NUM_ALLOC_PAGES	4096
#define HEAP_SIZE	0x100000
#define CONSOLE_LOGLEVEL_DEFAULT	3
#define MEM_LOGLEVEL_DEFAULT		5
#define	CONFIG_LOW_RAM_SIZE	0x00010000

/*
 *  By default build the hypervisor in low address
 *  so that it can only relocate to higher address
 */
#define	CONFIG_RAM_START	0x00100000
#define	CONFIG_RAM_SIZE		0x02000000	/* 32M */
#define	CONFIG_DMAR_PARSE_ENABLED	1
#define	CONFIG_GPU_SBDF		0x00000010	/* 0000:00:02.0 */
#define CONFIG_EFI_STUB       1
#define CONFIG_UEFI_OS_LOADER_NAME  "\\EFI\\org.clearlinux\\bootloaderx64.efi"
#define CONFIG_MTRR_ENABLED		1
#endif /* BSP_CFG_H */
