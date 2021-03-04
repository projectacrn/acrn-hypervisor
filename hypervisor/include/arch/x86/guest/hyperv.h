/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HYPERV_H
#define HYPERV_H

#include <x86/guest/vcpuid.h>

/* Hyper-V MSR numbers */
#define HV_X64_MSR_GUEST_OS_ID		0x40000000U
#define HV_X64_MSR_HYPERCALL		0x40000001U
#define HV_X64_MSR_VP_INDEX		0x40000002U

#define HV_X64_MSR_TIME_REF_COUNT	0x40000020U
#define HV_X64_MSR_REFERENCE_TSC	0x40000021U

union hyperv_ref_tsc_page_msr {
	uint64_t val64;
	struct {
		uint64_t enabled:1;
		uint64_t rsvdp:11;
		uint64_t gpfn:52;
	};
};

union hyperv_hypercall_msr {
	uint64_t val64;
	struct {
		uint64_t enabled:1;
		uint64_t locked:1;
		uint64_t rsvdp:10;
		uint64_t gpfn:52;
	};
};

union hyperv_guest_os_id_msr {
	uint64_t val64;
	struct {
		uint64_t build_number:16;
		uint64_t service_version:8;
		uint64_t minor_version:8;
		uint64_t major_version:8;
		uint64_t os_id:8;
		uint64_t vendor_id:15;
		uint64_t os_type:1;
	};
};

struct acrn_hyperv {
	union hyperv_hypercall_msr	hypercall_page;
	union hyperv_guest_os_id_msr	guest_os_id;
	union hyperv_ref_tsc_page_msr	ref_tsc_page;
	uint64_t			tsc_scale;
	uint64_t			tsc_offset;
};

int32_t hyperv_wrmsr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t wval);
int32_t hyperv_rdmsr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *rval);
void hyperv_init_vcpuid_entry(uint32_t leaf, uint32_t subleaf, uint32_t flags,
	struct vcpuid_entry *entry);

#endif
