/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <ucode.h>

#define MICRO_CODE_SIZE_MAX    0x40000U
static uint8_t micro_code[MICRO_CODE_SIZE_MAX];
static spinlock_t micro_code_lock = { .head = 0U, .tail = 0U };

uint64_t get_microcode_version(void)
{
	uint64_t val;
	uint32_t eax, ebx, ecx, edx;

	msr_write(MSR_IA32_BIOS_SIGN_ID, 0U);
	cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	val = msr_read(MSR_IA32_BIOS_SIGN_ID);

	return val;
}

/*
 * According to SDM vol 3 Table 9-7. If data_size field of uCode
 * header is zero, the ucode length is 2000
 */
static inline size_t get_ucode_data_size(const struct ucode_header *uhdr)
{
	return ((uhdr->data_size != 0U) ? uhdr->data_size : 2000U);
}

void acrn_update_ucode(struct acrn_vcpu *vcpu, uint64_t v)
{
	uint64_t gva, fault_addr;
	struct ucode_header uhdr;
	size_t data_size;
	int32_t err;
	uint32_t err_code;

	spinlock_obtain(&micro_code_lock);
	gva = v - sizeof(struct ucode_header);

	err_code = 0U;
	err = copy_from_gva(vcpu, &uhdr, gva, sizeof(uhdr), &err_code,
			&fault_addr);
	if (err < 0) {
		if (err == -EFAULT) {
			vcpu_inject_pf(vcpu, fault_addr, err_code);
		}
		spinlock_release(&micro_code_lock);
		return;
	}

	data_size = get_ucode_data_size(&uhdr) + sizeof(struct ucode_header);
	if (data_size > MICRO_CODE_SIZE_MAX) {
		pr_err("The size of microcode is greater than 0x%x",
				MICRO_CODE_SIZE_MAX);
		spinlock_release(&micro_code_lock);
		return;
	}

	err_code = 0U;
	err = copy_from_gva(vcpu, micro_code, gva, data_size, &err_code,
			&fault_addr);
	if (err < 0) {
		if (err == -EFAULT) {
			vcpu_inject_pf(vcpu, fault_addr, err_code);
		}
		spinlock_release(&micro_code_lock);
		return;
	}

	msr_write(MSR_IA32_BIOS_UPDT_TRIG,
			(uint64_t)micro_code + sizeof(struct ucode_header));
	(void)get_microcode_version();
	spinlock_release(&micro_code_lock);
}
