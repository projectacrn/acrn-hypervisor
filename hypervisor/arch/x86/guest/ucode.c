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
	uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

	msr_write(MSR_IA32_BIOS_SIGN_ID, 0);
	cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	val = msr_read(MSR_IA32_BIOS_SIGN_ID);

	return val;
}

/*
 * According to SDM vol 3 Table 9-7. If data_size field of uCode
 * header is zero, the ucode length is 2000
 */
#define	GET_DATA_SIZE(hdptr)	\
	((hdptr)->data_size ? ((hdptr)->data_size) : 2000)
void acrn_update_ucode(struct vcpu *vcpu, uint64_t v)
{
	uint64_t hva, gpa, gva;
	struct ucode_header *uhdr;
	int data_size, data_page_num;
	uint8_t *ucode_ptr, *ptr;
	int chunk_size;
	int error = 0;
	uint32_t err_code;

	gva = v - sizeof(struct ucode_header);

	err_code = 0;
	error = vm_gva2gpa(vcpu, gva, &gpa, &err_code);
	if (error)
		return;

	uhdr = (struct ucode_header *)GPA2HVA(vcpu->vm, gpa);

	data_size = GET_DATA_SIZE(uhdr) + sizeof(struct ucode_header);
	data_page_num =
		(data_size + CPU_PAGE_SIZE - 1) >> CPU_PAGE_SHIFT;

	ptr = ucode_ptr = alloc_pages(data_page_num);
	if (ptr == NULL)
		return;

	hva = (uint64_t)uhdr;
	while (true) {
		chunk_size = CPU_PAGE_SIZE - (hva & (CPU_PAGE_SIZE - 1));
		chunk_size = (chunk_size < data_size) ? chunk_size : data_size;

		memcpy_s(ucode_ptr, chunk_size, (uint8_t *)hva, chunk_size);

		data_size -= chunk_size;
		if (data_size <= 0)
			break;

		ucode_ptr += chunk_size;
		gva += chunk_size;

		err_code = 0;
		error = vm_gva2gpa(vcpu, gva, &gpa, &err_code);
		if (error) {
			free(ucode_ptr);
			return;
		}
		hva = (uint64_t)GPA2HVA(vcpu->vm, gpa);
	}

	msr_write(MSR_IA32_BIOS_UPDT_TRIG,
			(uint64_t)ptr + sizeof(struct ucode_header));
	get_microcode_version();

	free(ucode_ptr);
}
