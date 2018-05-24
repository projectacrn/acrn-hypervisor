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

#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
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
#define	GET_DATA_SIZE(hdptr)	((hdptr)->data_size ?\
			((hdptr)->data_size) : 2000)
void acrn_update_ucode(struct vcpu *vcpu, uint64_t v)
{
	uint64_t hva, gpa, gva;
	struct ucode_header *uhdr;
	int data_size, data_page_num;
	uint8_t *ucode_ptr, *ptr;
	int chunk_size;

	gva = v - sizeof(struct ucode_header);

	vm_gva2gpa(vcpu, gva, &gpa);
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

		vm_gva2gpa(vcpu, gva, &gpa);
		hva = (uint64_t)GPA2HVA(vcpu->vm, gpa);
	}

	msr_write(MSR_IA32_BIOS_UPDT_TRIG,
			(uint64_t)ptr + sizeof(struct ucode_header));
	get_microcode_version();

	free(ucode_ptr);
}
