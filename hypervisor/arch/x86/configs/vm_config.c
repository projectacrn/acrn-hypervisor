/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <errno.h>
#include <acrn_common.h>
#include <page.h>
#include <logmsg.h>
#include <cat.h>

/*
 * @pre vm_id < CONFIG_MAX_VM_NUM
 * @post return != NULL 
 */
struct acrn_vm_config *get_vm_config(uint16_t vm_id)
{
	return &vm_configs[vm_id];
}

/**
 * @pre vm_config != NULL
 */
int32_t sanitize_vm_config(void)
{
	int32_t ret = 0;
	uint16_t vm_id;
	uint64_t sos_pcpu_bitmap, pre_launch_pcpu_bitmap;
	struct acrn_vm_config *vm_config;

	sos_pcpu_bitmap = (uint64_t)((((uint64_t)1U) << get_pcpu_nums()) - 1U);
	pre_launch_pcpu_bitmap = 0U;
	/* All physical CPUs except ocuppied by Pre-launched VMs are all
	 * belong to SOS_VM. i.e. The pcpu_bitmap of a SOS_VM is decided
	 * by pcpu_bitmap status in PRE_LAUNCHED_VMs.
	 * We need to setup a rule, that the vm_configs[] array should follow
	 * the order of PRE_LAUNCHED_VM first, and then SOS_VM.
	 */
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		switch (vm_config->type) {
		case PRE_LAUNCHED_VM:
			if (vm_config->pcpu_bitmap == 0U) {
				ret = -EINVAL;
			/* GUEST_FLAG_RT must be set if we have GUEST_FLAG_LAPIC_PASSTHROUGH set in guest_flags */
			} else if (((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U)
					&& ((vm_config->guest_flags & GUEST_FLAG_RT) == 0U)) {
				ret = -EINVAL;
			} else {
				pre_launch_pcpu_bitmap |= vm_config->pcpu_bitmap;
			}
			break;
		case SOS_VM:
			/* Deduct pcpus of PRE_LAUNCHED_VMs */
			sos_pcpu_bitmap ^= pre_launch_pcpu_bitmap;
			if ((sos_pcpu_bitmap == 0U) || ((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U)) {
				ret = -EINVAL;
			} else {
				vm_config->pcpu_bitmap = sos_pcpu_bitmap;
			}
			break;
		case NORMAL_VM:
			ret = -EINVAL;
			break;
		default:
			/* Nothing to do for a UNDEFINED_VM, break directly. */
			break;
		}

		if ((vm_config->guest_flags & GUEST_FLAG_CLOS_REQUIRED) != 0U) {
			if (cat_cap_info.support && (vm_config->clos <= cat_cap_info.clos_max)) {
					cat_cap_info.enabled = true;
			} else {
				pr_err("%s set wrong CLOS or CAT is not supported\n", __func__);
				ret = -EINVAL;
			}
		}

		if (ret != 0) {
			break;
		}
	}
	return ret;
}
