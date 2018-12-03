/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <bsp_extern.h>
#include <multiboot.h>
#include <reloc.h>

#define ACRN_DBG_GUEST	6U

/* for VM0 e820 */
uint32_t e820_entries;
struct e820_entry e820[E820_MAX_ENTRIES];
struct e820_mem_params e820_mem;

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

uint64_t vcpumask2pcpumask(struct acrn_vm *vm, uint64_t vdmask)
{
	uint16_t vcpu_id;
	uint64_t dmask = 0UL;
	struct acrn_vcpu *vcpu;

	for (vcpu_id = 0U; vcpu_id < vm->hw.created_vcpus; vcpu_id++) {
		if ((vdmask & (1UL << vcpu_id)) != 0UL) {
			vcpu = vcpu_from_vid(vm, vcpu_id);
			bitmap_set_lock(vcpu->pcpu_id, &dmask);
		}
	}

	return dmask;
}

enum vm_paging_mode get_vcpu_paging_mode(struct acrn_vcpu *vcpu)
{
	enum vm_cpu_mode cpu_mode;

	cpu_mode = get_vcpu_mode(vcpu);

	if (cpu_mode == CPU_MODE_REAL) {
		return PAGING_MODE_0_LEVEL;
	}
	else if (cpu_mode == CPU_MODE_PROTECTED) {
		if (is_pae(vcpu)) {
			return PAGING_MODE_3_LEVEL;
		}
		else if (is_paging_enabled(vcpu)) {
			return PAGING_MODE_2_LEVEL;
		}
		return PAGING_MODE_0_LEVEL;
	} else {	/* compatibility or 64bit mode */
		return PAGING_MODE_4_LEVEL;
	}
}

/* TODO: Add code to check for Revserved bits, SMAP and PKE when do translation
 * during page walk */
static int local_gva2gpa_common(struct acrn_vcpu *vcpu, const struct page_walk_info *pw_info,
	uint64_t gva, uint64_t *gpa, uint32_t *err_code)
{
	uint32_t i;
	uint64_t index;
	uint32_t shift;
	void *base;
	uint64_t entry;
	uint64_t addr, page_size;
	int ret = 0;
	int fault = 0;
	bool is_user_mode_addr = true;
	bool is_page_rw_flags_on = true;

	if (pw_info->level < 1U) {
		return -EINVAL;
	}

	addr = pw_info->top_entry;
	i = pw_info->level;
	while (i != 0U) {
		i--;

		addr = addr & IA32E_REF_MASK;
		base = gpa2hva(vcpu->vm, addr);
		if (base == NULL) {
			ret = -EFAULT;
			goto out;
		}

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
			ret = -EFAULT;
			goto out;
		}

		/* check for R/W */
		if ((entry & PAGE_RW) == 0U) {
			if (pw_info->is_write_access) {
				/* Case1: Supermode and wp is 1
				 * Case2: Usermode */
				if (pw_info->is_user_mode_access ||
						pw_info->wp) {
					fault = 1;
					goto out;
				}
			}
			is_page_rw_flags_on = false;
		}

		/* check for nx, since for 32-bit paing, the XD bit is
		 * reserved(0), use the same logic as PAE/4-level paging */
		if (pw_info->is_inst_fetch && pw_info->nxe &&
		    ((entry & PAGE_NX) != 0U)) {
			fault = 1;
			goto out;
		}

		/* check for U/S */
		if ((entry & PAGE_USER) == 0U) {
			is_user_mode_addr = false;

			if (pw_info->is_user_mode_access) {
				fault = 1;
				goto out;
			}
		}

		if (pw_info->pse && ((i > 0U) && ((entry & PAGE_PSE) != 0U))) {
			break;
		}
		addr = entry;
	}

	/* When SMAP/SMEP is on, we only need to apply check when address is
	 * user-mode address.
	 * Also SMAP/SMEP only impact the supervisor-mode access.
	 */
	/* if smap is enabled and supervisor-mode access */
	if (pw_info->is_smap_on && !pw_info->is_user_mode_access &&
			is_user_mode_addr) {
		bool rflags_ac = ((vcpu_get_rflags(vcpu) & RFLAGS_AC) != 0UL);

		/* read from user mode address, eflags.ac = 0 */
		if (!pw_info->is_write_access && !rflags_ac) {
			fault = 1;
			goto out;
		}

		/* write to user mode address */
		if (pw_info->is_write_access) {
			/* cr0.wp = 0, eflags.ac = 0 */
			if (!pw_info->wp && !rflags_ac) {
				fault = 1;
				goto out;
			}

			/* cr0.wp = 1, eflags.ac = 1, r/w flag is 0
			 * on any paging structure entry
			 */
			if (pw_info->wp && rflags_ac && !is_page_rw_flags_on) {
				fault = 1;
				goto out;
			}

			/* cr0.wp = 1, eflags.ac = 0 */
			if (pw_info->wp && !rflags_ac) {
				fault = 1;
				goto out;
			}
		}
	}

	/* instruction fetch from user-mode address, smep on */
	if (pw_info->is_smep_on && !pw_info->is_user_mode_access &&
			is_user_mode_addr && pw_info->is_inst_fetch) {
		fault = 1;
		goto out;
	}

	entry >>= shift;
	/* shift left 12bit more and back to clear XD/Prot Key/Ignored bits */
	entry <<= (shift + 12U);
	entry >>= 12U;
	*gpa = entry | (gva & (page_size - 1UL));
out:

	if (fault != 0) {
		ret = -EFAULT;
		*err_code |= PAGE_FAULT_P_FLAG;
	}
	return ret;
}

static int local_gva2gpa_pae(struct acrn_vcpu *vcpu, struct page_walk_info *pw_info,
	uint64_t gva, uint64_t *gpa, uint32_t *err_code)
{
	int index;
	uint64_t *base;
	uint64_t entry;
	uint64_t addr;
	int ret;

	addr = pw_info->top_entry & 0xFFFFFFF0U;
	base = gpa2hva(vcpu->vm, addr);
	if (base == NULL) {
		ret = -EFAULT;
		goto out;
	}

	index = (gva >> 30U) & 0x3UL;
	entry = base[index];

	if ((entry & PAGE_PRESENT) == 0U) {
		ret = -EFAULT;
		goto out;
	}

	pw_info->level = 2U;
	pw_info->top_entry = entry;
	ret = local_gva2gpa_common(vcpu, pw_info, gva, gpa, err_code);

out:
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
int gva2gpa(struct acrn_vcpu *vcpu, uint64_t gva, uint64_t *gpa,
	uint32_t *err_code)
{
	enum vm_paging_mode pm = get_vcpu_paging_mode(vcpu);
	struct page_walk_info pw_info;
	int ret = 0;

	if ((gpa == NULL) || (err_code == NULL)) {
		return -EINVAL;
	}
	*gpa = 0UL;

	pw_info.top_entry = exec_vmread(VMX_GUEST_CR3);
	pw_info.level = pm;
	pw_info.is_write_access = ((*err_code & PAGE_FAULT_WR_FLAG) != 0U);
	pw_info.is_inst_fetch = ((*err_code & PAGE_FAULT_ID_FLAG) != 0U);

	/* SDM vol3 27.3.2
	 * If the segment register was unusable, the base, select and some
	 * bits of access rights are undefined. With the exception of
	 * DPL of SS
	 * and others.
	 * So we use DPL of SS access rights field for guest DPL.
	 */
	pw_info.is_user_mode_access =
		(((exec_vmread32(VMX_GUEST_SS_ATTR) >> 5U) & 0x3U) == 3U);
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
		pr_err("%s,vm[%hu] gpa 0x%llx,GPA is unmapping",
			__func__, vm->vm_id, gpa);
		return 0U;
	}

	if (fix_pg_size != 0U) {
		pg_size = fix_pg_size;
	}

	offset_in_pg = (uint32_t)gpa & (pg_size - 1U);
	len = (size > (pg_size - offset_in_pg)) ?
		(pg_size - offset_in_pg) : size;

	g_ptr = hpa2hva(hpa);

	if (cp_from_vm) {
		(void)memcpy_s(h_ptr, len, g_ptr, len);
	} else {
		(void)memcpy_s(g_ptr, len, h_ptr, len);
	}

	return len;
}

static inline int copy_gpa(struct acrn_vm *vm, void *h_ptr_arg, uint64_t gpa_arg,
	uint32_t size_arg, bool cp_from_vm)
{
	void *h_ptr = h_ptr_arg;
	uint32_t len;
	uint64_t gpa = gpa_arg;
	uint32_t size = size_arg;

	while (size > 0U) {
		len = local_copy_gpa(vm, h_ptr, gpa, size, 0U, cp_from_vm);
		if (len == 0U) {
			return -EINVAL;
		}

		gpa += len;
		h_ptr += len;
		size -= len;
	}

	return 0;
}

/*
 * @pre vcpu != NULL && err_code != NULL
 */
static inline int copy_gva(struct acrn_vcpu *vcpu, void *h_ptr_arg, uint64_t gva_arg,
	uint32_t size_arg, uint32_t *err_code, uint64_t *fault_addr,
	bool cp_from_vm)
{
	void *h_ptr = h_ptr_arg;
	uint64_t gpa = 0UL;
	int32_t ret;
	uint32_t len;
	uint64_t gva = gva_arg;
	uint32_t size = size_arg;

	while (size > 0U) {
		ret = gva2gpa(vcpu, gva, &gpa, err_code);
		if (ret < 0) {
			*fault_addr = gva;
			pr_err("error[%d] in GVA2GPA, err_code=0x%x",
					ret, *err_code);
			return ret;
		}

		len = local_copy_gpa(vcpu->vm, h_ptr, gpa, size,
			PAGE_SIZE_4K, cp_from_vm);

		if (len == 0U) {
			return -EINVAL;
		}

		gva += len;
		h_ptr += len;
		size -= len;
	}

	return 0;
}

/* @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int copy_from_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	return copy_gpa(vm, h_ptr, gpa, size, 1);
}
/* @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	return copy_gpa(vm, h_ptr, gpa, size, 0);
}

int copy_from_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr)
{
	return copy_gva(vcpu, h_ptr, gva, size, err_code, fault_addr, 1);
}

int copy_to_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr)
{
	return copy_gva(vcpu, h_ptr, gva, size, err_code, fault_addr, 0);
}

void init_e820(void)
{
	uint32_t i;

	if (boot_regs[0] == MULTIBOOT_INFO_MAGIC) {
		struct multiboot_info *mbi = (struct multiboot_info *)
			(hpa2hva((uint64_t)boot_regs[1]));

		pr_info("Multiboot info detected\n");
		if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_MMAP) != 0U) {
			struct multiboot_mmap *mmap =
				(struct multiboot_mmap *)
				hpa2hva((uint64_t)mbi->mi_mmap_addr);
			e820_entries = mbi->mi_mmap_length/
				sizeof(struct multiboot_mmap);
			if (e820_entries > E820_MAX_ENTRIES) {
				pr_err("Too many E820 entries %d\n",
					e820_entries);
				e820_entries = E820_MAX_ENTRIES;
			}
			dev_dbg(ACRN_DBG_GUEST,
				"mmap length 0x%x addr 0x%x entries %d\n",
				mbi->mi_mmap_length, mbi->mi_mmap_addr,
				e820_entries);
			for (i = 0U; i < e820_entries; i++) {
				e820[i].baseaddr = mmap[i].baseaddr;
				e820[i].length = mmap[i].length;
				e820[i].type = mmap[i].type;

				dev_dbg(ACRN_DBG_GUEST,
					"mmap table: %d type: 0x%x\n",
					i, mmap[i].type);
				dev_dbg(ACRN_DBG_GUEST,
					"Base: 0x%016llx length: 0x%016llx",
					mmap[i].baseaddr, mmap[i].length);
			}
		}
	} else {
		ASSERT(false, "no multiboot info found");
	}
}


void obtain_e820_mem_info(void)
{
	uint32_t i;
	struct e820_entry *entry;

	e820_mem.mem_bottom = UINT64_MAX;
	e820_mem.mem_top = 0x0UL;
	e820_mem.total_mem_size = 0UL;
	e820_mem.max_ram_blk_base = 0UL;
	e820_mem.max_ram_blk_size = 0UL;

	for (i = 0U; i < e820_entries; i++) {
		entry = &e820[i];
		if (e820_mem.mem_bottom > entry->baseaddr) {
			e820_mem.mem_bottom = entry->baseaddr;
		}

		if ((entry->baseaddr + entry->length)
				> e820_mem.mem_top) {
			e820_mem.mem_top = entry->baseaddr
				+ entry->length;
		}

		if (entry->type == E820_TYPE_RAM) {
			e820_mem.total_mem_size += entry->length;
			if (entry->baseaddr == UOS_DEFAULT_START_ADDR) {
				e820_mem.max_ram_blk_base =
					entry->baseaddr;
				e820_mem.max_ram_blk_size = entry->length;
			}
		}
	}
}

static void rebuild_vm0_e820(void)
{
	uint32_t i;
	uint64_t entry_start;
	uint64_t entry_end;
	uint64_t hv_start_pa = get_hv_image_base();
	uint64_t hv_end_pa  = hv_start_pa + CONFIG_HV_RAM_SIZE;
	struct e820_entry *entry, new_entry = {0};

	/* hypervisor mem need be filter out from e820 table
	 * it's hv itself + other hv reserved mem like vgt etc
	 */
	for (i = 0U; i < e820_entries; i++) {
		entry = &e820[i];
		entry_start = entry->baseaddr;
		entry_end = entry->baseaddr + entry->length;

		/* No need handle in these cases*/
		if ((entry->type != E820_TYPE_RAM) || (entry_end <= hv_start_pa)
				|| (entry_start >= hv_end_pa)) {
			continue;
		}

		/* filter out hv mem and adjust length of this entry*/
		if ((entry_start < hv_start_pa) && (entry_end <= hv_end_pa)) {
			entry->length = hv_start_pa - entry_start;
			continue;
		}
		/* filter out hv mem and need to create a new entry*/
		if ((entry_start < hv_start_pa) && (entry_end > hv_end_pa)) {
			entry->length = hv_start_pa - entry_start;
			new_entry.baseaddr = hv_end_pa;
			new_entry.length = entry_end - hv_end_pa;
			new_entry.type = E820_TYPE_RAM;
			continue;
		}
		/* This entry is within the range of hv mem
		 * change to E820_TYPE_RESERVED
		 */
		if ((entry_start >= hv_start_pa) && (entry_end <= hv_end_pa)) {
			entry->type = E820_TYPE_RESERVED;
			continue;
		}

		if ((entry_start >= hv_start_pa) && (entry_start < hv_end_pa)
				&& (entry_end > hv_end_pa)) {
			entry->baseaddr = hv_end_pa;
			entry->length = entry_end - hv_end_pa;
			continue;
		}

	}

	if (new_entry.length > 0UL) {
		e820_entries++;
		ASSERT(e820_entries <= E820_MAX_ENTRIES,
				"e820 entry overflow");
		entry = &e820[e820_entries - 1];
		entry->baseaddr = new_entry.baseaddr;
		entry->length = new_entry.length;
		entry->type = new_entry.type;
	}

	e820_mem.total_mem_size -= CONFIG_HV_RAM_SIZE;
}

/**
 * @param[inout] vm pointer to a vm descriptor
 *
 * @retval 0 on success
 *
 * @pre vm != NULL
 * @pre is_vm0(vm) == true
 */
int prepare_vm0_memmap_and_e820(struct acrn_vm *vm)
{
	uint32_t i;
	uint64_t attr_uc = (EPT_RWX | EPT_UNCACHED);
	struct e820_entry *entry;
	uint64_t hv_hpa;
	uint64_t *pml4_page = (uint64_t *)vm->arch_vm.nworld_eptp;

	rebuild_vm0_e820();
	dev_dbg(ACRN_DBG_GUEST,
		"vm0: bottom memory - 0x%llx, top memory - 0x%llx\n",
		e820_mem.mem_bottom, e820_mem.mem_top);

	if (e820_mem.mem_top > EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE)) {
		panic("Please configure VM0_ADDRESS_SPACE correctly!\n");
	}

	/* create real ept map for all ranges with UC */
	ept_mr_add(vm, pml4_page,
			e820_mem.mem_bottom, e820_mem.mem_bottom,
			(e820_mem.mem_top - e820_mem.mem_bottom),
			attr_uc);

	/* update ram entries to WB attr */
	for (i = 0U; i < e820_entries; i++) {
		entry = &e820[i];
		if (entry->type == E820_TYPE_RAM) {
			ept_mr_modify(vm, pml4_page,
					entry->baseaddr, entry->length,
					EPT_WB, EPT_MT_MASK);
		}
	}

	dev_dbg(ACRN_DBG_GUEST, "VM0 e820 layout:\n");
	for (i = 0U; i < e820_entries; i++) {
		entry = &e820[i];
		dev_dbg(ACRN_DBG_GUEST,
			"e820 table: %d type: 0x%x", i, entry->type);
		dev_dbg(ACRN_DBG_GUEST,
			"BaseAddress: 0x%016llx length: 0x%016llx\n",
			entry->baseaddr, entry->length);
	}

	/* unmap hypervisor itself for safety
	 * will cause EPT violation if sos accesses hv memory
	 */
	hv_hpa = get_hv_image_base();
	ept_mr_del(vm, pml4_page, hv_hpa, CONFIG_HV_RAM_SIZE);
	return 0;
}

uint64_t e820_alloc_low_memory(uint32_t size_arg)
{
	uint32_t i;
	uint32_t size = size_arg;
	struct e820_entry *entry, *new_entry;

	/* We want memory in page boundary and integral multiple of pages */
	size = (((size + PAGE_SIZE) - 1U) >> CPU_PAGE_SHIFT)
		<< CPU_PAGE_SHIFT;

	for (i = 0U; i < e820_entries; i++) {
		entry = &e820[i];
		uint64_t start, end, length;

		start = round_page_up(entry->baseaddr);
		end = round_page_down(entry->baseaddr + entry->length);
		length = end - start;
		length = (end > start) ? (end - start) : 0;

		/* Search for available low memory */
		if ((entry->type != E820_TYPE_RAM)
			|| (length < size)
			|| ((start + size) > MEM_1M)) {
			continue;
		}

		/* found exact size of e820 entry */
		if (length == size) {
			entry->type = E820_TYPE_RESERVED;
			e820_mem.total_mem_size -= size;
			return start;
		}

		/*
		 * found entry with available memory larger than requested
		 * alocate memory from the end of this entry at page boundary
		 */
		new_entry = &e820[e820_entries];
		new_entry->type = E820_TYPE_RESERVED;
		new_entry->baseaddr = end - size;
		new_entry->length = (entry->baseaddr +
			entry->length) - new_entry->baseaddr;

		/* Shrink the existing entry and total available memory */
		entry->length -= new_entry->length;
		e820_mem.total_mem_size -= new_entry->length;
		e820_entries++;

		return new_entry->baseaddr;
	}

	pr_fatal("Can't allocate memory under 1M from E820\n");
	return ACRN_INVALID_HPA;
}
