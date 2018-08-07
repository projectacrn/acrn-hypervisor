/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <ucode.h>

uint64_t get_microcode_version(void)
{
	uint64_t val;
	uint32_t eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;

	msr_write(MSR_IA32_BIOS_SIGN_ID, 0U);
	cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	val = msr_read(MSR_IA32_BIOS_SIGN_ID);

	return val;
}

/*
 * According to SDM vol 3 Table 9-7. If data_size field of uCode
 * header is zero, the ucode length is 2000
 */
#define	UCODE_GET_DATA_SIZE(uhdr)	\
	((uhdr.data_size != 0U) ? uhdr.data_size : 2000U)
void acrn_update_ucode(struct vcpu *vcpu, uint64_t v)
{
	uint64_t gva, fault_addr;
	struct ucode_header uhdr;
	uint32_t data_page_num;
	size_t data_size;
	uint8_t *ucode_ptr;
	int err;
	uint32_t err_code;

	gva = v - sizeof(struct ucode_header);

	err_code = 0U;
	err = copy_from_gva(vcpu, &uhdr, gva, sizeof(uhdr), &err_code,
			&fault_addr);
	if (err < 0) {
		if (err == -EFAULT) {
			vcpu_inject_pf(vcpu, fault_addr, err_code);
		}
		return;
	}

	data_size = UCODE_GET_DATA_SIZE(uhdr) + sizeof(struct ucode_header);
	data_page_num =
		((data_size + CPU_PAGE_SIZE) - 1U) >> CPU_PAGE_SHIFT;

	ucode_ptr = alloc_pages(data_page_num);
	if (ucode_ptr == NULL) {
		return;
	}

	err_code = 0U;
	err = copy_from_gva(vcpu, ucode_ptr, gva, data_size, &err_code,
			&fault_addr);
	if (err < 0) {
		if (err == -EFAULT) {
			vcpu_inject_pf(vcpu, fault_addr, err_code);
		}
		return;
	}

	msr_write(MSR_IA32_BIOS_UPDT_TRIG,
			(uint64_t)ucode_ptr + sizeof(struct ucode_header));
	get_microcode_version();

	free(ucode_ptr);
}
