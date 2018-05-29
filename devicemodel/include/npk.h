/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NPK_H_
#define _NPK_H_

#define NPK_DRV_SYSFS_PATH		"/sys/bus/pci/drivers/intel_th_pci"

#define NPK_CSR_MTB_BAR_SZ		0x100000

#define NPK_CSR_GTH_BASE		0
#define NPK_CSR_GTH_SZ			0xF0
#define NPK_CSR_GTHOPT0			0x0
#define NPK_CSR_SWDEST_0		0x8
#define NPK_CSR_GSWDEST			0x88
#define NPK_CSR_GTHSTAT			0xD4
#define NPK_CSR_GTHSTAT_PLE		0xFF

#define NPK_CSR_STH_BASE		0x4000
#define NPK_CSR_STH_SZ			0x80
#define NPK_CSR_STHCAP0			0x0
#define NPK_CSR_STHCAP1			0x4

#define NPK_CSR_MSC0_BASE		0xA0100
#define NPK_CSR_MSC0_SZ			0x20
#define NPK_CSR_MSCxCTL			0x0
#define NPK_CSR_MSCxSTS			0x4
#define NPK_CSR_MSCxSTS_PLE		0x4

#define NPK_CSR_MSC1_BASE		0xA0200
#define NPK_CSR_MSC1_SZ			0x20

#define NPK_CSR_PTI_BASE		0x1C00
#define NPK_CSR_PTI_SZ			0x4

#define NPK_SW_MSTR_STRT		256
#define NPK_SW_MSTR_STP			1024
#define NPK_SW_MSTR_NUM			(NPK_SW_MSTR_STP - NPK_SW_MSTR_STRT)
#define NPK_CHANNELS_PER_MSTR		128
#define NPK_MSTR_TO_MEM_SZ(x)		((x) * NPK_CHANNELS_PER_MSTR * 64)

enum npk_regs_name {
	NPK_CSR_FIRST,
	NPK_CSR_GTH = NPK_CSR_FIRST,
	NPK_CSR_STH,
	NPK_CSR_MSC0,
	NPK_CSR_MSC1,
	NPK_CSR_PTI,
	NPK_CSR_LAST,
};

struct npk_regs {
	uint32_t base;
	uint32_t size;
	union {
		uint8_t *u8;
		uint32_t *u32;
	} data;
} __packed;

struct npk_reg_default_val {
	enum npk_regs_name csr;
	int offset;
	uint32_t default_val;
};

#endif /* _NPK_H_ */
