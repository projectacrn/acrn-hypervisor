/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bits.h>
#include <vm_config.h>
#include <logmsg.h>
#include <rdt.h>
#include <pgtable.h>
#include <vuart.h>

static uint8_t rtvm_uuid1[16] = POST_RTVM_UUID1;
static uint8_t safety_vm_uuid1[16] = SAFETY_VM_UUID1;

/*
 * @pre vm_id < CONFIG_MAX_VM_NUM
 * @post return != NULL
 */
struct acrn_vm_config *get_vm_config(uint16_t vm_id)
{
	return &vm_configs[vm_id];
}

/*
 * @pre vm_id < CONFIG_MAX_VM_NUM
 */
uint8_t get_vm_severity(uint16_t vm_id)
{
	return vm_configs[vm_id].severity;
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
 * return true if the input uuid is for RTVM
 *
 * @pre vmid < CONFIG_MAX_VM_NUM
 */
static bool is_safety_vm_uuid(const uint8_t *uuid)
{
	/* TODO: Extend to check more safety VM uuid if we have more than one safety VM. */
	return uuid_is_equal(uuid, safety_vm_uuid1);
}

/**
 * return true if the input uuid is for RTVM
 *
 * @pre vmid < CONFIG_MAX_VM_NUM
 */
static bool is_rtvm_uuid(const uint8_t *uuid)
{
	/* TODO: Extend to check more rtvm uuid if we have more than one RTVM. */
	return uuid_is_equal(uuid, rtvm_uuid1);
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

static bool check_vm_clos_config(uint16_t vm_id)
{
	uint16_t i;
	bool ret = true;
	struct acrn_vm_config *vm_config = get_vm_config(vm_id);
	uint16_t vcpu_num = bitmap_weight(vm_config->cpu_affinity);

	for (i = 0U; i < vcpu_num; i++) {
		if (vm_config->clos[i] >= valid_clos_num) {
			pr_err("vm%u: vcpu%u clos(%u) exceed the max clos(%u).",
				vm_id, i, vm_config->clos[i], valid_clos_num);
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
	uint16_t vm_id, vuart_idx;
	uint64_t pre_launch_pcpu_bitmap = 0UL;
	struct acrn_vm_config *vm_config;

	/* All physical CPUs except ocuppied by Pre-launched VMs are all
	 * belong to SOS_VM. i.e. The cpu_affinity of a SOS_VM is decided
	 * by cpu_affinity status in PRE_LAUNCHED_VMs.
	 * We need to setup a rule, that the vm_configs[] array should follow
	 * the order of PRE_LAUNCHED_VM first, and then SOS_VM.
	 */
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);

		if ((vm_config->cpu_affinity & ~ALL_CPUS_MASK) != 0UL) {
			pr_err("%s: vm%u assigns invalid PCPU (0x%llx)", __func__, vm_id, vm_config->cpu_affinity);
			ret = false;
		}

		switch (vm_config->load_order) {
		case PRE_LAUNCHED_VM:
			if (vm_config->cpu_affinity == 0UL) {
				ret = false;
			/* GUEST_FLAG_RT must be set if we have GUEST_FLAG_LAPIC_PASSTHROUGH set in guest_flags */
			} else if (((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U)
					&& ((vm_config->guest_flags & GUEST_FLAG_RT) == 0U)) {
				ret = false;
			} else if (vm_config->epc.size != 0UL) {
				ret = false;
			} else if (is_safety_vm_uuid(vm_config->uuid) && (vm_config->severity != (uint8_t)SEVERITY_SAFETY_VM)) {
				ret = false;
			} else {
				pre_launch_pcpu_bitmap |= vm_config->cpu_affinity;
			}
			break;
		case SOS_VM:
			/* Deduct pcpus of PRE_LAUNCHED_VMs */
			vm_config->cpu_affinity = ALL_CPUS_MASK ^ pre_launch_pcpu_bitmap;
			if ((vm_config->cpu_affinity == 0UL) || (vm_config->severity != (uint8_t)SEVERITY_SOS) ||
				((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U)) {
				ret = false;
			}
			break;
		case POST_LAUNCHED_VM:
			if ((vm_config->cpu_affinity == 0UL) ||
				((vm_config->cpu_affinity & pre_launch_pcpu_bitmap) != 0UL)) {
				pr_err("%s: Post-launch VM has no pcpus or share pcpu with Pre-launch VM!", __func__);
				ret = false;
			}

			if ((vm_config->severity == (uint8_t)SEVERITY_SAFETY_VM) ||
					(vm_config->severity == (uint8_t)SEVERITY_SOS)) {
				ret = false;
			}

			/* VM with RTVM uuid must have RTVM severity */
			if (is_rtvm_uuid(vm_config->uuid) && (vm_config->severity != (uint8_t)SEVERITY_RTVM)) {
				ret = false;
			}

			/* VM WITHOUT RTVM uuid must NOT have RTVM severity */
			if (!is_rtvm_uuid(vm_config->uuid) && (vm_config->severity == (uint8_t)SEVERITY_RTVM)) {
				ret = false;
			}

			break;
		default:
			/* Nothing to do for a unknown VM, break directly. */
			break;
		}

		if (ret && is_platform_rdt_capable()) {
			ret = check_vm_clos_config(vm_id);
		}

		if (ret &&
		    (((vm_config->epc.size | vm_config->epc.base) & ~PAGE_MASK) != 0UL)) {
			ret = false;
		}

		if (ret) {
			/* make sure no identical UUID in following VM configurations */
			ret = check_vm_uuid_collision(vm_id);
		}

		if (ret) {
			/* vuart[1+] are used for VM communications */
			for (vuart_idx = 1U; vuart_idx < MAX_VUART_NUM_PER_VM; vuart_idx++) {
				const struct vuart_config *vu_config = &vm_config->vuart[vuart_idx];

				if (!(vu_config->type == VUART_LEGACY_PIO) &&
					(vu_config->addr.port_base == INVALID_COM_BASE)) {
					if ((vu_config->t_vuart.vm_id >= CONFIG_MAX_VM_NUM) ||
						(vu_config->t_vuart.vm_id == vm_id)) {
						pr_err("%s invalid vuart configuration for VM %d\n", __func__, vm_id);
						ret = false;
					}
				}
			}
		}

		if (!ret) {
			break;
		}
	}
	return ret;
}
