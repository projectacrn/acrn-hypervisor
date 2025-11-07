/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <reloc.h>
#include <vm_config.h>
#include <libfdt.h>
#include <vfdt.h>
#include <fdt_api.h>

void init_vm_vfdt_common(struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	void *fdt = vm_get_vfdt(vm);

	if (vm_config->os_config.bootargs[0] != '\0') {
		fdt_set_kernel_bootargs(fdt, vm_config->os_config.bootargs);
	}

	vm->sw.fdt_info.src_addr = fdt;
	vm->sw.fdt_info.size = fdt_totalsize(fdt);
	/* load addr is initialized in image loader */
}

void init_service_vm_vfdt(struct acrn_vm *vm)
{
	void *fdt = vm_get_vfdt(vm);
	struct acrn_vm_config *vm_config;
	uint16_t vm_id;
	uint64_t i, addr, size;

	/* Re-use host FDT */
	fdt_move(get_host_fdt(), fdt, MAX_FDT_SIZE);

	/* Reserve hypervisor mem range */
	fdt_add_rsvd_node(fdt, get_hv_image_base(), get_hv_image_size());

	/* Remove hv owned devices */
	/* TODO: to be implemented */

	/* Reserve pre-launched VM mem range */
	for (vm_id = 0; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if (vm_config->load_order == PRE_LAUNCHED_VM) {
			for (i = 0; i < vm_config->memory.region_num; i++) {
				addr = vm_config->memory.host_regions[i].start_hpa;
				size = vm_config->memory.host_regions[i].size_hpa;
				fdt_add_rsvd_node(fdt, addr, size);
			}
		}
	}

	/* Remove pre-launch owned devices */
	/* TODO: to be implemented */

	arch_init_service_vm_vfdt(vm);

	init_vm_vfdt_common(vm);
}
