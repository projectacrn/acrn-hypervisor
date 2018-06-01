/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <zeropage.h>

static uint32_t create_e820_table(struct e820_entry *_e820)
{
	uint32_t i;

	ASSERT(e820_entries > 0,
			"e820 should be inited");

	for (i = 0; i < e820_entries; i++) {
		_e820[i].baseaddr = e820[i].baseaddr;
		_e820[i].length = e820[i].length;
		_e820[i].type = e820[i].type;
	}

	return e820_entries;
}

static uint64_t create_zero_page(struct vm *vm)
{
	struct zero_page *zeropage;
	struct sw_linux *sw_linux = &(vm->sw.linux_info);
	struct zero_page *hva;
	uint64_t gpa;

	/* Set zeropage in Linux Guest RAM region just past boot args */
	hva = GPA2HVA(vm, (uint64_t)sw_linux->bootargs_load_addr);
	zeropage = (struct zero_page *)((char *)hva + MEM_4K);

	/* clear the zeropage */
	memset(zeropage, 0, MEM_2K);

	/* copy part of the header into the zero page */
	hva = GPA2HVA(vm, (uint64_t)vm->sw.kernel_info.kernel_load_addr);
	memcpy_s(&(zeropage->hdr), sizeof(zeropage->hdr),
				&(hva->hdr), sizeof(hva->hdr));

	/* See if kernel has a RAM disk */
	if (sw_linux->ramdisk_src_addr) {
		/* Copy ramdisk load_addr and size in zeropage header structure
		 */
		zeropage->hdr.ramdisk_addr =
			(uint32_t)(uint64_t)sw_linux->ramdisk_load_addr;
		zeropage->hdr.ramdisk_size = (uint32_t)sw_linux->ramdisk_size;
	}

	/* Copy bootargs load_addr in zeropage header structure */
	zeropage->hdr.bootargs_addr =
		(uint32_t)(uint64_t)sw_linux->bootargs_load_addr;

	/* set constant arguments in zero page */
	zeropage->hdr.loader_type = 0xff;
	zeropage->hdr.load_flags |= (1 << 5);	/* quiet */

	/* Create/add e820 table entries in zeropage */
	zeropage->e820_nentries = create_e820_table(zeropage->e820);

	/* Get the host physical address of the zeropage */
	gpa = hpa2gpa(vm, HVA2HPA((uint64_t)zeropage));

	/* Return Physical Base Address of zeropage */
	return gpa;
}

int load_guest(struct vm *vm, struct vcpu *vcpu)
{
	int ret = 0;
	void *hva;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	uint64_t  lowmem_gpa_top;

	hva  = GPA2HVA(vm, GUEST_CFG_OFFSET);
	lowmem_gpa_top = *(uint64_t *)hva;

	/* hardcode vcpu entry addr(kernel entry) & rsi (zeropage)*/
	memset(cur_context->guest_cpu_regs.longs,
			0, sizeof(uint64_t)*NUM_GPRS);

	hva  = GPA2HVA(vm, lowmem_gpa_top -
			MEM_4K - MEM_2K);
	vcpu->entry_addr = (void *)(*((uint64_t *)hva));
	cur_context->guest_cpu_regs.regs.rsi =
		lowmem_gpa_top - MEM_4K;

	pr_info("%s, Set config according to predefined offset:",
			__func__);
	pr_info("VCPU%d Entry: 0x%llx, RSI: 0x%016llx, cr3: 0x%016llx",
			vcpu->vcpu_id, vcpu->entry_addr,
			cur_context->guest_cpu_regs.regs.rsi,
			vm->arch_vm.guest_init_pml4);

	return ret;
}

int general_sw_loader(struct vm *vm, struct vcpu *vcpu)
{
	int ret = 0;
	void *hva;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	char  dyn_bootargs[100] = {0};
	uint32_t kernel_entry_offset;
	struct zero_page *zeropage;

	ASSERT(vm != NULL, "Incorrect argument");

	pr_dbg("Loading guest to run-time location");

	/* FIXME: set config according to predefined offset */
	if (!is_vm0(vm))
		return load_guest(vm, vcpu);

	/* calculate the kernel entry point */
	zeropage = (struct zero_page *)
			vm->sw.kernel_info.kernel_src_addr;
	kernel_entry_offset = (zeropage->hdr.setup_sects + 1) * 512;
	if (vcpu->arch_vcpu.cpu_mode == CPU_MODE_64BIT)
		/* 64bit entry is the 512bytes after the start */
		kernel_entry_offset += 512;

	vm->sw.kernel_info.kernel_entry_addr =
		(void *)((unsigned long)vm->sw.kernel_info.kernel_load_addr
			+ kernel_entry_offset);
	if (is_vcpu_bsp(vcpu)) {
		/* Set VCPU entry point to kernel entry */
		vcpu->entry_addr = vm->sw.kernel_info.kernel_entry_addr;
		pr_info("%s, VM *d VCPU %d Entry: 0x%016llx ",
			__func__, vm->attr.id, vcpu->vcpu_id, vcpu->entry_addr);
	}

	/* Calculate the host-physical address where the guest will be loaded */
	hva = GPA2HVA(vm, (uint64_t)vm->sw.kernel_info.kernel_load_addr);

	/* Copy the guest kernel image to its run-time location */
	memcpy_s((void *)hva, vm->sw.kernel_info.kernel_size,
				vm->sw.kernel_info.kernel_src_addr,
				vm->sw.kernel_info.kernel_size);

	/* See if guest is a Linux guest */
	if (vm->sw.kernel_type == VM_LINUX_GUEST) {
		/* Documentation states: ebx=0, edi=0, ebp=0, esi=ptr to
		 * zeropage
		 */
		memset(cur_context->guest_cpu_regs.longs,
			0, sizeof(uint64_t) * NUM_GPRS);

		/* Get host-physical address for guest bootargs */
		hva = GPA2HVA(vm,
			(uint64_t)vm->sw.linux_info.bootargs_load_addr);

		/* Copy Guest OS bootargs to its load location */
		strcpy_s((char *)hva, MEM_2K,
				vm->sw.linux_info.bootargs_src_addr);

#ifdef CONFIG_CMA
		/* add "cma=XXXXM@0xXXXXXXXX" to cmdline*/
		if (is_vm0(vm) && (e820_mem.max_ram_blk_size > 0)) {
			snprintf(dyn_bootargs, 100, " cma=%dM@0x%llx",
					(e820_mem.max_ram_blk_size >> 20),
					e820_mem.max_ram_blk_base);
			strcpy_s((char *)hva
					+vm->sw.linux_info.bootargs_size,
					100, dyn_bootargs);
		}
#else
		/* add "hugepagesz=1G hugepages=x" to cmdline for 1G hugepage
		 * reserving. Current strategy is "total_mem_size in Giga -
		 * remained 1G pages" for reserving.
		 */
		if (is_vm0(vm) && check_mmu_1gb_support(PTT_HOST)) {
			int reserving_1g_pages;

#ifdef CONFIG_REMAIN_1G_PAGES
			reserving_1g_pages = (e820_mem.total_mem_size >> 30) -
						CONFIG_REMAIN_1G_PAGES;
#else
			reserving_1g_pages = (e820_mem.total_mem_size >> 30) -
						3;
#endif
			if (reserving_1g_pages > 0) {
				snprintf(dyn_bootargs, 100,
					" hugepagesz=1G hugepages=%d",
					reserving_1g_pages);
				strcpy_s((char *)hva
					+vm->sw.linux_info.bootargs_size,
					100, dyn_bootargs);
			}
		}
#endif

		/* Check if a RAM disk is present with Linux guest */
		if (vm->sw.linux_info.ramdisk_src_addr) {
			/* Get host-physical address for guest RAM disk */
			hva = GPA2HVA(vm,
				(uint64_t)vm->sw.linux_info.ramdisk_load_addr);

			/* Copy RAM disk to its load location */
			memcpy_s((void *)hva, vm->sw.linux_info.ramdisk_size,
					vm->sw.linux_info.ramdisk_src_addr,
					vm->sw.linux_info.ramdisk_size);

		}

		/* Create Zeropage and copy Physical Base Address of Zeropage
		 * in RSI
		 */
		cur_context->guest_cpu_regs.regs.rsi = create_zero_page(vm);

		pr_info("%s, RSI pointing to zero page for VM %d at GPA %X",
				__func__, vm->attr.id,
				cur_context->guest_cpu_regs.regs.rsi);

	} else {
		pr_err("%s, Loading VM SW failed", __func__);
		ret = -EINVAL;
	}

	return ret;
}
