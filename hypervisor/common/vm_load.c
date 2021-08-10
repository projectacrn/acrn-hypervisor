/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
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
	/* get primary vcpu */
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BSP_CPU_ID);
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
	default:
		ret = -EINVAL;
		break;
	}

	if (ret == 0) {
		/* Copy Guest OS ACPI to its load location */
		load_sw_module(vm, acpi_info);
		/* Set VCPU entry point to kernel entry */
		vcpu_set_rip(vcpu, (uint64_t)vm->sw.kernel_info.kernel_entry_addr);
		pr_info("%s, VM %hu VCPU %hu Entry: 0x%016lx ", __func__, vm->vm_id, vcpu->vcpu_id,
			vm->sw.kernel_info.kernel_entry_addr);
	}

	return ret;
}
