/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <zeropage.h>

#ifdef CONFIG_PARTITION_MODE
static uint32_t create_e820_table(struct e820_entry *param_e820)
{
	uint32_t i;

	for (i = 0U; i < NUM_E820_ENTRIES; i++) {
		param_e820[i].baseaddr = e820_default_entries[i].baseaddr;
		param_e820[i].length = e820_default_entries[i].length;
		param_e820[i].type = e820_default_entries[i].type;
	}

	return NUM_E820_ENTRIES;
}
#else
static uint32_t create_e820_table(struct e820_entry *param_e820)
{
	uint32_t i;

	ASSERT(e820_entries > 0U,
			"e820 should be inited");

	for (i = 0U; i < e820_entries; i++) {
		param_e820[i].baseaddr = e820[i].baseaddr;
		param_e820[i].length = e820[i].length;
		param_e820[i].type = e820[i].type;
	}

	return e820_entries;
}
#endif

static void prepare_bsp_gdt(struct vm *vm)
{
	size_t gdt_len;
	uint64_t gdt_base_hpa;
	void *gdt_base_hva;

	gdt_base_hpa = gpa2hpa(vm, vm0_boot_context.gdt.base);
	if (vm0_boot_context.gdt.base == gdt_base_hpa) {
		return;
	} else {
		gdt_base_hva = hpa2hva(gdt_base_hpa);
		gdt_len = ((size_t)vm0_boot_context.gdt.limit + 1U)/sizeof(uint8_t);

		(void )memcpy_s(gdt_base_hva, gdt_len, hpa2hva(vm0_boot_context.gdt.base), gdt_len);
	}

	return;
}

static uint64_t create_zero_page(struct vm *vm)
{
	struct zero_page *zeropage;
	struct sw_linux *sw_linux = &(vm->sw.linux_info);
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	struct zero_page *hva;
	uint64_t gpa, addr;

	/* Set zeropage in Linux Guest RAM region just past boot args */
	gpa = (uint64_t)sw_linux->bootargs_load_addr + MEM_4K;
	hva = gpa2hva(vm, gpa);
	zeropage = hva;

	/* clear the zeropage */
	(void)memset(zeropage, 0U, MEM_2K);

	/* copy part of the header into the zero page */
	hva = gpa2hva(vm, (uint64_t)sw_kernel->kernel_load_addr);
	(void)memcpy_s(&(zeropage->hdr), sizeof(zeropage->hdr),
				&(hva->hdr), sizeof(hva->hdr));

	/* See if kernel has a RAM disk */
	if (sw_linux->ramdisk_src_addr != NULL) {
		/* Copy ramdisk load_addr and size in zeropage header structure
		 */
		addr = (uint64_t)sw_linux->ramdisk_load_addr;
		zeropage->hdr.ramdisk_addr = (uint32_t)addr;
		zeropage->hdr.ramdisk_size = (uint32_t)sw_linux->ramdisk_size;
	}

	/* Copy bootargs load_addr in zeropage header structure */
	addr = (uint64_t)sw_linux->bootargs_load_addr;
	zeropage->hdr.bootargs_addr = (uint32_t)addr;

	/* set constant arguments in zero page */
	zeropage->hdr.loader_type = 0xffU;
	zeropage->hdr.load_flags |= (1U << 5U);	/* quiet */

	/* Create/add e820 table entries in zeropage */
	zeropage->e820_nentries = (uint8_t)create_e820_table(zeropage->e820);

	/* Return Physical Base Address of zeropage */
	return gpa;
}

int general_sw_loader(struct vm *vm)
{
	int32_t ret = 0;
	void *hva;
	char  dyn_bootargs[100] = {0};
	uint32_t kernel_entry_offset;
	struct zero_page *zeropage;
	struct sw_linux *sw_linux = &(vm->sw.linux_info);
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	struct acrn_vcpu *vcpu = get_primary_vcpu(vm);

	pr_dbg("Loading guest to run-time location");

	prepare_bsp_gdt(vm);
	set_vcpu_regs(vcpu, &vm0_boot_context);

	/* calculate the kernel entry point */
	zeropage = (struct zero_page *)sw_kernel->kernel_src_addr;
	kernel_entry_offset = (uint32_t)(zeropage->hdr.setup_sects + 1U) * 512U;
	if (vcpu->arch.cpu_mode == CPU_MODE_64BIT) {
		/* 64bit entry is the 512bytes after the start */
		kernel_entry_offset += 512U;
	}

	sw_kernel->kernel_entry_addr =
		(void *)((uint64_t)sw_kernel->kernel_load_addr
			+ kernel_entry_offset);
	if (is_vcpu_bsp(vcpu)) {
		/* Set VCPU entry point to kernel entry */
		vcpu_set_rip(vcpu, (uint64_t)sw_kernel->kernel_entry_addr);
		pr_info("%s, VM %hu VCPU %hu Entry: 0x%016llx ",
			__func__, vm->vm_id, vcpu->vcpu_id,
			sw_kernel->kernel_entry_addr);
	}

	/* Calculate the host-physical address where the guest will be loaded */
	hva = gpa2hva(vm, (uint64_t)sw_kernel->kernel_load_addr);

	/* Copy the guest kernel image to its run-time location */
	(void)memcpy_s((void *)hva, sw_kernel->kernel_size,
				sw_kernel->kernel_src_addr,
				sw_kernel->kernel_size);

	/* See if guest is a Linux guest */
	if (vm->sw.kernel_type == VM_LINUX_GUEST) {
		uint32_t i;

		/* Documentation states: ebx=0, edi=0, ebp=0, esi=ptr to
		 * zeropage
		 */
		for (i = 0U; i < NUM_GPRS; i++) {
			vcpu_set_gpreg(vcpu, i, 0UL);
		}

		/* Get host-physical address for guest bootargs */
		hva = gpa2hva(vm,
			(uint64_t)sw_linux->bootargs_load_addr);

		/* Copy Guest OS bootargs to its load location */
		(void)strcpy_s((char *)hva, MEM_2K,
				sw_linux->bootargs_src_addr);

#ifdef CONFIG_CMA
		/* add "cma=XXXXM@0xXXXXXXXX" to cmdline*/
		if (is_vm0(vm) && (e820_mem.max_ram_blk_size > 0)) {
			snprintf(dyn_bootargs, 100U, " cma=%dM@0x%llx",
					(e820_mem.max_ram_blk_size >> 20),
					e820_mem.max_ram_blk_base);
			(void)strcpy_s((char *)hva
					+ sw_linux->bootargs_size,
					100U, dyn_bootargs);
		}
#else
		/* add "hugepagesz=1G hugepages=x" to cmdline for 1G hugepage
		 * reserving. Current strategy is "total_mem_size in Giga -
		 * remained 1G pages" for reserving.
		 */
		if (is_vm0(vm)) {
			int32_t reserving_1g_pages;

#ifdef CONFIG_REMAIN_1G_PAGES
			reserving_1g_pages = (e820_mem.total_mem_size >> 30) -
						CONFIG_REMAIN_1G_PAGES;
#else
			reserving_1g_pages = (e820_mem.total_mem_size >> 30) -
						3;
#endif
			if (reserving_1g_pages > 0) {
				snprintf(dyn_bootargs, 100U,
					" hugepagesz=1G hugepages=%d",
					reserving_1g_pages);
				(void)strcpy_s((char *)hva
					+ sw_linux->bootargs_size,
					100U, dyn_bootargs);
			}
		}
#endif

		/* Check if a RAM disk is present with Linux guest */
		if (sw_linux->ramdisk_src_addr != NULL) {
			/* Get host-physical address for guest RAM disk */
			hva = gpa2hva(vm,
				(uint64_t)sw_linux->ramdisk_load_addr);

			/* Copy RAM disk to its load location */
			(void)memcpy_s((void *)hva,
					sw_linux->ramdisk_size,
					sw_linux->ramdisk_src_addr,
					sw_linux->ramdisk_size);

		}

		/* Create Zeropage and copy Physical Base Address of Zeropage
		 * in RSI
		 */
		vcpu_set_gpreg(vcpu, CPU_REG_RSI, create_zero_page(vm));

		pr_info("%s, RSI pointing to zero page for VM %d at GPA %X",
				__func__, vm->vm_id,
				vcpu_get_gpreg(vcpu, CPU_REG_RSI));

	} else {
		pr_err("%s, Loading VM SW failed", __func__);
		ret = -EINVAL;
	}

	return ret;
}
