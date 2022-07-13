/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/lib/bits.h>
#include <asm/page.h>
#include <asm/vm_config.h>
#include <asm/rdt.h>
#include <vuart.h>
#include <ivshmem.h>
#include <vmcs9900.h>
#include <vpci.h>
#include <hv_prebuild.h>


/* sanity check for below structs is not needed, so use a empty struct instead */
const struct pci_vdev_ops vhostbridge_ops;
const struct pci_vdev_ops vpci_ivshmem_ops;
const struct pci_vdev_ops vmcs9900_ops;

#define PLATFORM_CPUS_MASK             ((1UL << MAX_PCPU_NUM) - 1UL)


#ifdef CONFIG_RDT_ENABLED
static bool check_vm_clos_config(uint16_t vm_id)
{
	uint16_t i;
	uint16_t platform_clos_num = HV_SUPPORTED_MAX_CLOS;
	bool ret = true;
	struct acrn_vm_config *vm_config = get_vm_config(vm_id);

	for (i = 0U; i < vm_config->num_pclosids; i++) {
		if (((platform_clos_num != 0U) && (vm_config->pclosids[i] == platform_clos_num))
				|| (vm_config->pclosids[i] > platform_clos_num)) {
			printf("vm%u: vcpu%u clos(%u) exceed the max clos(%u).",
				vm_id, i, vm_config->pclosids[i], platform_clos_num);
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
	 * the order of PRE_LAUNCHED_VM first, and then Service VM.
	 */
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);

		if ((vm_config->name[0] != '\0')
			&& ((vm_config->cpu_affinity == 0UL) || ((vm_config->cpu_affinity & ~PLATFORM_CPUS_MASK) != 0UL))) {
			printf("%s: vm%u assigns invalid PCPU (affinity: 0x%016x)\n", __func__, vm_id, vm_config->cpu_affinity);
			ret = false;
		}

		switch (vm_config->load_order) {
		case PRE_LAUNCHED_VM:
			/* GUEST_FLAG_LAPIC_PASSTHROUGH must be set if we have GUEST_FLAG_RT set in guest_flags */
			if (((vm_config->guest_flags & GUEST_FLAG_RT) != 0U)
				&& ((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH)== 0U)) {
				ret = false;
			} else if (vm_config->epc.size != 0UL) {
				ret = false;
			}
			break;
		case SERVICE_VM:
			break;
		case POST_LAUNCHED_VM:
			if ((vm_config->severity == (uint8_t)SEVERITY_SAFETY_VM) || (vm_config->severity == (uint8_t)SEVERITY_SERVICE_VM)) {
				ret = false;
			}
			break;
		default:
			/* Nothing to do for a unknown VM, break directly. */
			break;
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
