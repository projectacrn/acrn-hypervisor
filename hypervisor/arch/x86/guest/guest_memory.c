/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <x86/vmx.h>
#include <x86/guest/guest_memory.h>
#include <x86/guest/vcpu.h>
#include <x86/guest/vm.h>
#include <x86/guest/vmcs.h>
#include <x86/mmu.h>
#include <x86/guest/ept.h>
#include <logmsg.h>

struct page_walk_info {
	uint64_t top_entry;	/* Top level paging structure entry */
	uint32_t level;
	uint32_t width;
	bool is_user_mode_access;
	bool is_write_access;
	bool is_inst_fetch;
	bool pse;		/* CR4.PSE for 32bit paing,
				 * true for PAE/4-level paing */
	bool wp;		/* CR0.WP */
	bool nxe;		/* MSR_IA32_EFER_NXE_BIT */

	bool is_smap_on;
	bool is_smep_on;
};

enum vm_paging_mode get_vcpu_paging_mode(struct acrn_vcpu *vcpu)
{
	enum vm_paging_mode ret = PAGING_MODE_0_LEVEL;	/* non-paging */

	if (is_paging_enabled(vcpu)) {
		if (is_pae(vcpu)) {
			if (is_long_mode(vcpu)) {
				ret = PAGING_MODE_4_LEVEL;	/* 4-level paging */
			} else {
				ret = PAGING_MODE_3_LEVEL;	/* PAE paging */
			}
		} else {
			ret = PAGING_MODE_2_LEVEL;	/* 32-bit paging */
		}
	}

	return ret;
}

/* TODO: Add code to check for Revserved bits, SMAP and PKE when do translation
 * during page walk */
static int32_t local_gva2gpa_common(struct acrn_vcpu *vcpu, const struct page_walk_info *pw_info,
	uint64_t gva, uint64_t *gpa, uint32_t *err_code)
{
	uint32_t i;
	uint64_t index;
	uint32_t shift;
	void *base;
	uint64_t entry = 0U;
	uint64_t addr;
	uint64_t page_size = PAGE_SIZE_4K;
	int32_t ret = 0;
	int32_t fault = 0;
	bool is_user_mode_addr = true;
	bool is_page_rw_flags_on = true;

	if (pw_info->level < 1U) {
		ret = -EINVAL;
	} else {
		addr = pw_info->top_entry;
		i = pw_info->level;
		stac();

		while ((i != 0U) && (fault == 0)) {
			i--;

			addr = addr & IA32E_REF_MASK;
			base = gpa2hva(vcpu->vm, addr);
			if (base == NULL) {
				fault = 1;
			} else {
				shift = (i * pw_info->width) + 12U;
				index = (gva >> shift) & ((1UL << pw_info->width) - 1UL);
				page_size = 1UL << shift;

				if (pw_info->width == 10U) {
					uint32_t *base32 = (uint32_t *)base;
					/* 32bit entry */
					entry = (uint64_t)(*(base32 + index));
				} else {
					uint64_t *base64 = (uint64_t *)base;
					entry = *(base64 + index);
				}

				/* check if the entry present */
				if ((entry & PAGE_PRESENT) == 0U) {
					fault = 1;
				}

					/* check for R/W */
				if ((fault == 0) && ((entry & PAGE_RW) == 0U)) {
					if (pw_info->is_write_access  &&
					    (pw_info->is_user_mode_access || pw_info->wp)) {
						/* Case1: Supermode and wp is 1
						 * Case2: Usermode */
						fault = 1;
					}
					is_page_rw_flags_on = false;
				}
			}

			/* check for nx, since for 32-bit paing, the XD bit is
			 * reserved(0), use the same logic as PAE/4-level paging */
			if ((fault == 0) && pw_info->is_inst_fetch && pw_info->nxe &&
				    ((entry & PAGE_NX) != 0U)) {
				fault = 1;
			}

			/* check for U/S */
			if ((fault == 0) && ((entry & PAGE_USER) == 0U)) {
				is_user_mode_addr = false;

				if (pw_info->is_user_mode_access) {
					fault = 1;
				}
			}


			if ((fault == 0) && pw_info->pse &&
				((i > 0U) && ((entry & PAGE_PSE) != 0U))) {
					break;
			}
			addr = entry;
		}

		/* When SMAP/SMEP is on, we only need to apply check when address is
		 * user-mode address.
		 * Also SMAP/SMEP only impact the supervisor-mode access.
		 */
		/* if smap is enabled and supervisor-mode access */
		if ((fault == 0) && pw_info->is_smap_on && (!pw_info->is_user_mode_access) &&
			is_user_mode_addr) {
			bool acflag = ((vcpu_get_rflags(vcpu) & RFLAGS_AC) != 0UL);

			/* read from user mode address, eflags.ac = 0 */
			if ((!pw_info->is_write_access) && (!acflag)) {
				fault = 1;
			} else if (pw_info->is_write_access) {
				/* write to user mode address */

				/* cr0.wp = 0, eflags.ac = 0 */
				if ((!pw_info->wp) && (!acflag)) {
					fault = 1;
				}

				/* cr0.wp = 1, eflags.ac = 1, r/w flag is 0
				 * on any paging structure entry
				 */
				if (pw_info->wp && acflag && (!is_page_rw_flags_on)) {
					fault = 1;
				}

				/* cr0.wp = 1, eflags.ac = 0 */
				if (pw_info->wp && (!acflag)) {
					fault = 1;
				}
			} else {
				/* do nothing */
			}
		}

		/* instruction fetch from user-mode address, smep on */
		if ((fault == 0) && pw_info->is_smep_on && (!pw_info->is_user_mode_access) &&
			is_user_mode_addr && pw_info->is_inst_fetch) {
			fault = 1;
		}

		if (fault == 0) {
			entry >>= shift;
			/* shift left 12bit more and back to clear XD/Prot Key/Ignored bits */
			entry <<= (shift + 12U);
			entry >>= 12U;
			*gpa = entry | (gva & (page_size - 1UL));
		}

		clac();
		if (fault != 0) {
			ret = -EFAULT;
			*err_code |= PAGE_FAULT_P_FLAG;
		}
	}
	return ret;
}

static int32_t local_gva2gpa_pae(struct acrn_vcpu *vcpu, struct page_walk_info *pw_info,
	uint64_t gva, uint64_t *gpa, uint32_t *err_code)
{
	int32_t index;
	uint64_t *base;
	uint64_t entry;
	uint64_t addr;
	int32_t ret = -EFAULT;

	addr = get_pae_pdpt_addr(pw_info->top_entry);
	base = (uint64_t *)gpa2hva(vcpu->vm, addr);
	if (base != NULL) {
		index = (gva >> 30U) & 0x3UL;
		stac();
		entry = base[index];
		clac();

		if ((entry & PAGE_PRESENT) != 0U) {
			pw_info->level = 2U;
			pw_info->top_entry = entry;
			ret = local_gva2gpa_common(vcpu, pw_info, gva, gpa, err_code);
		}
	}

	return ret;
}

/* Refer to SDM Vol.3A 6-39 section 6.15 for the format of paging fault error
 * code.
 *
 * Caller should set the contect of err_code properly according to the address
 * usage when calling this function:
 * - If it is an address for write, set PAGE_FAULT_WR_FLAG in err_code.
 * - If it is an address for instruction featch, set PAGE_FAULT_ID_FLAG in
 *   err_code.
 * Caller should check the return value to confirm if the function success or
 * not.
 * If a protection volation detected during page walk, this function still will
 * give the gpa translated, it is up to caller to decide if it need to inject a
 * #PF or not.
 * - Return 0 for success.
 * - Return -EINVAL for invalid parameter.
 * - Return -EFAULT for paging fault, and refer to err_code for paging fault
 *   error code.
 */
int32_t gva2gpa(struct acrn_vcpu *vcpu, uint64_t gva, uint64_t *gpa,
	uint32_t *err_code)
{
	enum vm_paging_mode pm = get_vcpu_paging_mode(vcpu);
	struct page_walk_info pw_info;
	int32_t ret = 0;

	if ((gpa == NULL) || (err_code == NULL)) {
		ret = -EINVAL;
	} else {
		*gpa = 0UL;

		pw_info.top_entry = exec_vmread(VMX_GUEST_CR3);
		pw_info.level = (uint32_t)pm;
		pw_info.is_write_access = ((*err_code & PAGE_FAULT_WR_FLAG) != 0U);
		pw_info.is_inst_fetch = ((*err_code & PAGE_FAULT_ID_FLAG) != 0U);

		/* SDM vol3 27.3.2
		 * If the segment register was unusable, the base, select and some
		 * bits of access rights are undefined. With the exception of
		 * DPL of SS
		 * and others.
		 * So we use DPL of SS access rights field for guest DPL.
		 */
		pw_info.is_user_mode_access = (((exec_vmread32(VMX_GUEST_SS_ATTR) >> 5U) & 0x3U) == 3U);
		pw_info.pse = true;
		pw_info.nxe = ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_NXE_BIT) != 0UL);
		pw_info.wp = ((vcpu_get_cr0(vcpu) & CR0_WP) != 0UL);
		pw_info.is_smap_on = ((vcpu_get_cr4(vcpu) & CR4_SMAP) != 0UL);
		pw_info.is_smep_on = ((vcpu_get_cr4(vcpu) & CR4_SMEP) != 0UL);

		*err_code &=  ~PAGE_FAULT_P_FLAG;

		if (pm == PAGING_MODE_4_LEVEL) {
			pw_info.width = 9U;
			ret = local_gva2gpa_common(vcpu, &pw_info, gva, gpa, err_code);
		} else if (pm == PAGING_MODE_3_LEVEL) {
			pw_info.width = 9U;
			ret = local_gva2gpa_pae(vcpu, &pw_info, gva, gpa, err_code);
		} else if (pm == PAGING_MODE_2_LEVEL) {
			pw_info.width = 10U;
			pw_info.pse = ((vcpu_get_cr4(vcpu) & CR4_PSE) != 0UL);
			pw_info.nxe = false;
			ret = local_gva2gpa_common(vcpu, &pw_info, gva, gpa, err_code);
		} else {
			*gpa = gva;
		}

		if (ret == -EFAULT) {
			if (pw_info.is_user_mode_access) {
				*err_code |= PAGE_FAULT_US_FLAG;
			}
		}
	}

	return ret;
}

static inline uint32_t local_copy_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa,
	uint32_t size, uint32_t fix_pg_size, bool cp_from_vm)
{
	uint64_t hpa;
	uint32_t offset_in_pg, len, pg_size;
	void *g_ptr;

	hpa = local_gpa2hpa(vm, gpa, &pg_size);
	if (hpa == INVALID_HPA) {
		pr_err("%s,vm[%hu] gpa 0x%lx,GPA is unmapping",
			__func__, vm->vm_id, gpa);
		len = 0U;
	} else {

		if (fix_pg_size != 0U) {
			pg_size = fix_pg_size;
		}

		offset_in_pg = (uint32_t)gpa & (pg_size - 1U);
		len = (size > (pg_size - offset_in_pg)) ? (pg_size - offset_in_pg) : size;

		g_ptr = hpa2hva(hpa);

		stac();
		if (cp_from_vm) {
			(void)memcpy_s(h_ptr, len, g_ptr, len);
		} else {
			(void)memcpy_s(g_ptr, len, h_ptr, len);
		}
		clac();
	}

	return len;
}

static inline int32_t copy_gpa(struct acrn_vm *vm, void *h_ptr_arg, uint64_t gpa_arg,
	uint32_t size_arg, bool cp_from_vm)
{
	void *h_ptr = h_ptr_arg;
	uint32_t len;
	uint64_t gpa = gpa_arg;
	uint32_t size = size_arg;
	int32_t err = 0;

	while (size > 0U) {
		len = local_copy_gpa(vm, h_ptr, gpa, size, 0U, cp_from_vm);
		if (len == 0U) {
			err = -EINVAL;
			break;
		}
		gpa += len;
		h_ptr += len;
		size -= len;
	}

	return err;
}

/*
 * @pre vcpu != NULL && err_code != NULL && h_ptr_arg != NULL
 */
static inline int32_t copy_gva(struct acrn_vcpu *vcpu, void *h_ptr_arg, uint64_t gva_arg,
	uint32_t size_arg, uint32_t *err_code, uint64_t *fault_addr,
	bool cp_from_vm)
{
	void *h_ptr = h_ptr_arg;
	uint64_t gpa = 0UL;
	int32_t ret = 0;
	uint32_t len;
	uint64_t gva = gva_arg;
	uint32_t size = size_arg;

	while ((size > 0U) && (ret == 0)) {
		ret = gva2gpa(vcpu, gva, &gpa, err_code);
		if (ret >= 0) {
			len = local_copy_gpa(vcpu->vm, h_ptr, gpa, size, PAGE_SIZE_4K, cp_from_vm);
			if (len != 0U) {
				gva += len;
				h_ptr += len;
				size -= len;
			} else {
				ret =  -EINVAL;
			}
		} else {
			*fault_addr = gva;
			pr_err("error[%d] in GVA2GPA, err_code=0x%x", ret, *err_code);
		}
	}

	return ret;
}

/* @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int32_t copy_from_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	int32_t ret = 0;

	ret = copy_gpa(vm, h_ptr, gpa, size, 1);
	if (ret != 0) {
		pr_err("Unable to copy GPA 0x%llx from VM%d to HPA 0x%llx\n", gpa, vm->vm_id, (uint64_t)h_ptr);
	}

	return ret;
}

/* @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int32_t copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	int32_t ret = 0;

	ret = copy_gpa(vm, h_ptr, gpa, size, 0);
	if (ret != 0) {
		pr_err("Unable to copy HPA 0x%llx to GPA 0x%llx in VM%d\n", (uint64_t)h_ptr, gpa, vm->vm_id);
	}

	return ret;
}

int32_t copy_from_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr)
{
	return copy_gva(vcpu, h_ptr, gva, size, err_code, fault_addr, 1);
}

int32_t copy_to_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr)
{
	return copy_gva(vcpu, h_ptr, gva, size, err_code, fault_addr, false);
}

/* gpa --> hpa -->hva */
void *gpa2hva(struct acrn_vm *vm, uint64_t x)
{
	uint64_t hpa = gpa2hpa(vm, x);
	return (hpa == INVALID_HPA) ? NULL : hpa2hva(hpa);
}
