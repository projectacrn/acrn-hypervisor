/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <vboot.h>
#include <errno.h>
#include <logmsg.h>

/**
 * @pre sw_module != NULL
 */
void load_sw_module(struct acrn_vm *vm, struct sw_module_info *sw_module)
{
	if ((sw_module->size != 0) && (sw_module->load_addr != NULL)) {
		(void)copy_to_gpa(vm, sw_module->src_addr, (uint64_t)sw_module->load_addr, sw_module->size);
	}
}

/**
 * @pre vm != NULL
 */
int32_t prepare_os_image(struct acrn_vm *vm)
{
	int32_t ret = -EINVAL;
	struct sw_module_info *acpi_info = &(vm->sw.acpi_info);

	switch (vm->sw.kernel_type) {
#ifdef CONFIG_GUEST_KERNEL_BZIMAGE
	case KERNEL_BZIMAGE:
		ret = bzimage_loader(vm);
		break;
#endif
#ifdef CONFIG_GUEST_KERNEL_RAWIMAGE
	case KERNEL_RAWIMAGE:
		ret = rawimage_loader(vm);
		break;
#endif
#ifdef CONFIG_GUEST_KERNEL_ELF
	case KERNEL_ELF:
		ret = elf_loader(vm);
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}

	if (ret == 0) {
		/* Copy Guest OS ACPI to its load location */
		load_sw_module(vm, acpi_info);
		pr_dbg("%s, VM%hu 0x%016lx", __func__, vm->vm_id,
			vm->sw.kernel_info.kernel_entry_addr);
	}

	return ret;
}
