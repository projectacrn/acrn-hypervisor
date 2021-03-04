/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/lib/bits.h>
#include <x86/page.h>
#include <x86/vm_config.h>
#include <x86/rdt.h>
#include <vuart.h>
#include <ivshmem.h>
#include <vmcs9900.h>
#include <vpci.h>
#include <hv_prebuild.h>

static uint8_t rtvm_uuids[][16] = {
	PRE_RTVM_UUID1,
	POST_RTVM_UUID1,
};
static uint8_t safety_vm_uuid1[16] = SAFETY_VM_UUID1;

/* sanity check for below structs is not needed, so use a empty struct instead */
const struct pci_vdev_ops vhostbridge_ops;
const struct pci_vdev_ops vpci_ivshmem_ops;
const struct pci_vdev_ops vmcs9900_ops;

#define PLATFORM_CPUS_MASK             ((1UL << MAX_PCPU_NUM) - 1UL)

/**
 * return true if the input uuid is for safety VM
 */
static bool is_safety_vm_uuid(const uint8_t *uuid)
{
	/* TODO: Extend to check more safety VM uuid if we have more than one safety VM. */
	return uuid_is_equal(uuid, safety_vm_uuid1);
}

/**
 * return true if the input uuid is for RTVM
 */
static bool is_rtvm_uuid(const uint8_t *uuid)
{
	bool ret = false;
	uint16_t i;
	uint8_t *rtvm_uuid;

	for (i = 0U; i < ARRAY_SIZE(rtvm_uuids); i++) {
		rtvm_uuid = rtvm_uuids[i];
		if (uuid_is_equal(uuid, rtvm_uuid)) {
			ret = true;
			break;
		}
	}
	return ret;
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

#ifdef CONFIG_RDT_ENABLED
static bool check_vm_clos_config(uint16_t vm_id)
{
	uint16_t i;
	uint16_t platform_clos_num = HV_SUPPORTED_MAX_CLOS;
	bool ret = true;
	struct acrn_vm_config *vm_config = get_vm_config(vm_id);
	uint16_t vcpu_num = bitmap_weight(vm_config->cpu_affinity);

	for (i = 0U; i < vcpu_num; i++) {
		if (((platform_clos_num != 0U) && (vm_config->clos[i] == platform_clos_num))
				|| (vm_config->clos[i] > platform_clos_num)) {
			printf("vm%u: vcpu%u clos(%u) exceed the max clos(%u).",
				vm_id, i, vm_config->clos[i], platform_clos_num);
			ret = false;
			break;
		}
	}
	return ret;
}
#endif

/**
 * @pre vm_config != NULL
 */
bool sanitize_vm_config(void)
{
	bool ret = true;
	uint16_t vm_id, vuart_idx;
	struct acrn_vm_config *vm_config;

	/* We need to setup a rule, that the vm_configs[] array should follow
	 * the order of PRE_LAUNCHED_VM first, and then SOS_VM.
	 */
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);

		if ((vm_config->cpu_affinity == 0UL) || ((vm_config->cpu_affinity & ~PLATFORM_CPUS_MASK) != 0UL)) {
			printf("%s: vm%u assigns invalid PCPU (affinity: 0x%016x)\n", __func__, vm_id, vm_config->cpu_affinity);
			ret = false;
		}

		switch (vm_config->load_order) {
		case PRE_LAUNCHED_VM:
			/* GUEST_FLAG_RT must be set if we have GUEST_FLAG_LAPIC_PASSTHROUGH set in guest_flags */
			if (((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U)
					&& ((vm_config->guest_flags & GUEST_FLAG_RT) == 0U)) {
				ret = false;
			} else if (vm_config->epc.size != 0UL) {
				ret = false;
			} else if (is_safety_vm_uuid(vm_config->uuid) && (vm_config->severity != (uint8_t)SEVERITY_SAFETY_VM)) {
				ret = false;
			} else {
#if (SOS_VM_NUM == 1U)
				if (vm_config->severity <= SEVERITY_SOS) {
				/* If there are both SOS and Pre-launched VM, make sure pre-launched VM has higher severity than SOS */
					printf("%s: pre-launched vm doesn't has higher severity than SOS \n", __func__);
					ret = false;
				}
#endif
			}
			break;
		case SOS_VM:
			if ((vm_config->severity != (uint8_t)SEVERITY_SOS) || ((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U)) {
				ret = false;
			}
			break;
		case POST_LAUNCHED_VM:
			if ((vm_config->severity == (uint8_t)SEVERITY_SAFETY_VM) || (vm_config->severity == (uint8_t)SEVERITY_SOS)) {
				ret = false;
			}
			break;
		default:
			/* Nothing to do for a unknown VM, break directly. */
			break;
		}

		if (ret) {
			/* VM with RTVM uuid must have RTVM severity */
			if (is_rtvm_uuid(vm_config->uuid) && (vm_config->severity != (uint8_t)SEVERITY_RTVM)) {
				ret = false;
			}

			/* VM WITHOUT RTVM uuid must NOT have RTVM severity */
			if (!is_rtvm_uuid(vm_config->uuid) && (vm_config->severity == (uint8_t)SEVERITY_RTVM)) {
				ret = false;
			}
		}

#ifdef CONFIG_RDT_ENABLED
		if (ret) {
			ret = check_vm_clos_config(vm_id);
		}
#endif

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
						printf("%s invalid vuart configuration for VM %d\n", __func__, vm_id);
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
