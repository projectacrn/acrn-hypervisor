/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_SBI_H
#define RISCV_SBI_H

#include <types.h>

enum sbi_eid  {
	SBI_EID_BASE = 0x10,
	SBI_EID_TIMER = 0x54494D45,
	SBI_EID_IPI = 0x735049,
	SBI_EID_RFENCE = 0x52464E43,
	SBI_EID_HSM = 0x48534D,
	SBI_EID_SRST = 0x53525354,
	SBI_EID_PMU = 0x504D55,
	SBI_EID_MPXY = 0x4D505859,

	/* Experimental extensions must lie within this range */
	SBI_EID_EXPER_START = 0x08000000,
	SBI_EID_EXPER_END = 0x08FFFFFF,

	/* Vendor extensions must lie within this range */
	SBI_EID_VENDOR_START = 0x09000000,
	SBI_EID_VENDOR_END = 0x09FFFFFF,
};

#define SBI_BASE_FID_GET_SPEC_VERSION		0x0
#define SBI_BASE_FID_GET_IMP_ID			0x1
#define SBI_BASE_FID_GET_IMP_VERSION		0x2
#define SBI_BASE_FID_PROBE_EXT			0x3
#define SBI_BASE_FID_GET_MVENDORID		0x4
#define SBI_BASE_FID_GET_MARCHID		0x5
#define SBI_BASE_FID_GET_MIMPID			0x6

/* SBI function IDs for TIME extension*/
#define SBI_TIMER_FID_SET_TIMER			0x0

/* SBI function IDs for IPI extension*/
#define SBI_IPI_FID_SEND_IPI			0x0

/* SBI function IDs for RFENCE extension*/
#define SBI_RFENCE_FID_FNECE_I			0x0
#define SBI_RFENCE_FID_SFNECE_VMA		0x1
#define SBI_RFENCE_FID_SFNECE_VMA_ASID		0x2
#define SBI_RFENCE_FID_HFNECE_GVMA		0x3
#define SBI_RFENCE_FID_HFNECE_GVMA_ASID		0x4
#define SBI_RFENCE_FID_HFNECE_VVMA		0x5
#define SBI_RFENCE_FID_HFNECE_VVMA_ASID		0x6

/* SBI function IDs for HSM extension*/
#define SBI_HSM_FID_HART_START			0x0
#define SBI_HSM_FID_HART_STOP			0x1
#define SBI_HSM_FID_HART_GET_STATUS		0x2
#define SBI_HSM_FID_HART_SUSPEND		0x3

/* SBI function IDs for MPXY extension*/
#define SBI_MPXY_FID_GET_SHM_SIZE		0x0
#define SBI_MPXY_FID_SET_SHM			0x1
#define SBI_MPXY_FID_GET_CHANNEL_IDS		0x2
#define SBI_MPXY_FID_READ_ATTRS			0x3
#define SBI_MPXY_FID_WRITE_ATTRS		0x4
#define SBI_MPXY_FID_SEND_MSG_WITH_RESP		0x5
#define SBI_MPXY_FID_SEND_MSG_WITHOUT_RESP	0x6
#define SBI_MPXY_FID_GET_NOTFICATION_EVENTS	0x7

/* SBI return error codes */
#define SBI_SUCCESS				0
#define SBI_ERR_FAILED				-1
#define SBI_ERR_NOT_SUPPORTED			-2
#define SBI_ERR_INVALID_PARAM			-3
#define SBI_ERR_DENIED				-4
#define SBI_ERR_INVALID_ADDRESS			-5
#define SBI_ERR_ALREADY_AVAILABLE		-6
#define SBI_ERR_ALREADY_STARTED			-7
#define SBI_ERR_ALREADY_STOPPED			-8
#define SBI_ERR_NO_SHMEM			-9
#define SBI_ERR_INVALID_STATE			-10
#define SBI_ERR_BAD_RANGE			-11
#define SBI_ERR_TIMEOUT				-12
#define SBI_ERR_IO				-13
#define SBI_ERR_DENIED_LOCKED			-14

#define SBI_RFENCE_FLUSH_ALL			((uint64_t)-1)

typedef struct {
	int64_t error;
	union {
		int64_t value;
		uint64_t uvalue;
	};
} sbiret;

void arch_send_single_ipi(uint16_t pcpu_id, __unused uint32_t msg_type);
void arch_send_dest_ipi_mask(uint64_t dest_mask, __unused uint32_t msg_type);

#endif /* RISCV_SBI_H */
