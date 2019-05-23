/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bits.h>
#include <vm_config.h>
#include <logmsg.h>
#include <cat.h>
#include <pgtable.h>

/*
 * @pre vm_id < CONFIG_MAX_VM_NUM
 * @post return != NULL
 */
struct acrn_vm_config *get_vm_config(uint16_t vm_id)
{
	return &vm_configs[vm_id];
}

static inline bool uuid_is_equal(const uint8_t *uuid1, const uint8_t *uuid2)
{
	uint64_t uuid1_h = *(const uint64_t *)uuid1;
	uint64_t uuid1_l = *(const uint64_t *)(uuid1 + 8);
	uint64_t uuid2_h = *(const uint64_t *)uuid2;
	uint64_t uuid2_l = *(const uint64_t *)(uuid2 + 8);

	return ((uuid1_h == uuid2_h) && (uuid1_l == uuid2_l));
}

/**
 * return true if the input uuid is configured in VM
 *
 * @pre vmid < CONFIG_MAX_VM_NUM
 */
bool vm_has_matched_uuid(uint16_t vmid, const uint8_t *uuid)
{
	struct acrn_vm_config *vm_config = get_vm_config(vmid);

	return (uuid_is_equal(vm_config->uuid, uuid));
}

/**
 * return true if no UUID collision is found in vm configs array start from vm_configs[vm_id]
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 */
static bool check_vm_uuid_collision(uint16_t vm_id)
{
	uint16_t i;
	bool ret = true;
	struct acrn_vm_config *start_config = get_vm_config(vm_id);
	struct acrn_vm_config *following_config;

	for (i = vm_id + 1U; i < CONFIG_MAX_VM_NUM; i++) {
		following_config = get_vm_config(i);
		if (uuid_is_equal(&start_config->uuid[0], &following_config->uuid[0])) {
			ret = false;
			break;
		}
	}
	return ret;
}

/**
 * @pre vm_config != NULL
 */
bool sanitize_vm_config(void)
{
	bool ret = true;
	uint16_t vm_id, vcpu_id, nr;
	uint64_t sos_pcpu_bitmap, pre_launch_pcpu_bitmap = 0U, vm_pcpu_bitmap;
	struct acrn_vm_config *vm_config;

	sos_pcpu_bitmap = (uint64_t)((((uint64_t)1U) << get_pcpu_nums()) - 1U);
	/* All physical CPUs except ocuppied by Pre-launched VMs are all
	 * belong to SOS_VM. i.e. The vm_pcpu_bitmap of a SOS_VM is decided
	 * by vm_pcpu_bitmap status in PRE_LAUNCHED_VMs.
	 * We need to setup a rule, that the vm_configs[] array should follow
	 * the order of PRE_LAUNCHED_VM first, and then SOS_VM.
	 */
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		vm_pcpu_bitmap = 0U;
		if (vm_config->load_order != SOS_VM) {
			for (vcpu_id = 0U; vcpu_id < vm_config->vcpu_num; vcpu_id++) {
				if (bitmap_weight(vm_config->vcpu_affinity[vcpu_id]) != 1U) {
					pr_err("%s: vm%u vcpu%u should have only one prefer pcpu!",
							__func__, vm_id, vcpu_id);
					ret = false;
					break;
				}
				vm_pcpu_bitmap |= vm_config->vcpu_affinity[vcpu_id];
			}

			if ((bitmap_weight(vm_pcpu_bitmap) != vm_config->vcpu_num)) {
				pr_err("%s: One VM cannot have multiple vcpus share one pcpu!", __func__);
				ret = false;
			}
		}

		switch (vm_config->load_order) {
		case PRE_LAUNCHED_VM:
			if (vm_config->vcpu_num == 0U) {
				ret = false;
			/* GUEST_FLAG_RT must be set if we have GUEST_FLAG_LAPIC_PASSTHROUGH set in guest_flags */
			} else if (((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U)
					&& ((vm_config->guest_flags & GUEST_FLAG_RT) == 0U)) {
				ret = false;
			}else if (vm_config->epc.size != 0UL) {
				ret = false;
			} else {
				pre_launch_pcpu_bitmap |= vm_pcpu_bitmap;
			}
			break;
		case SOS_VM:
			/* Deduct pcpus of PRE_LAUNCHED_VMs */
			sos_pcpu_bitmap ^= pre_launch_pcpu_bitmap;
			if ((sos_pcpu_bitmap == 0U) || ((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U)) {
				ret = false;
			} else {
				vm_config->vcpu_num = bitmap_weight(sos_pcpu_bitmap);
				for (vcpu_id = 0U; vcpu_id < vm_config->vcpu_num; vcpu_id++) {
					nr = ffs64(sos_pcpu_bitmap);
					vm_config->vcpu_affinity[vcpu_id] = 1U << nr;
					bitmap_clear_nolock(nr, &sos_pcpu_bitmap);
				}
			}
			break;
		case POST_LAUNCHED_VM:
			if ((vm_pcpu_bitmap == 0U) || ((vm_pcpu_bitmap & pre_launch_pcpu_bitmap) != 0U)) {
				pr_err("%s: Post-launch VM has no pcpus or share pcpu with Pre-launch VM!", __func__);
				ret = false;
			}
			break;
		default:
			/* Nothing to do for a unknown VM, break directly. */
			break;
		}

		if ((vm_config->guest_flags & GUEST_FLAG_CLOS_REQUIRED) != 0U) {
			if (cat_cap_info.support && (vm_config->clos <= cat_cap_info.clos_max)) {
					cat_cap_info.enabled = true;
			} else {
				pr_err("%s set wrong CLOS or CAT is not supported\n", __func__);
				ret = false;
			}
		}

		if (((vm_config->epc.size | vm_config->epc.base) & ~PAGE_MASK) != 0UL) {
			ret = false;
		}

		if (ret) {
			/* make sure no identical UUID in following VM configurations */
			ret = check_vm_uuid_collision(vm_id);
		}
		if (!ret) {
			break;
		}
	}
	return ret;
}
