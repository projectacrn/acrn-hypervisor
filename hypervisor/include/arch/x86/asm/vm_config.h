
/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef X86_VM_CONFIG_H_
#define X86_VM_CONFIG_H_

#include <asm/sgx.h>

/* Bitmask of guest flags that can be programmed by device model. Other bits are set by hypervisor only. */
#if (SERVICE_VM_NUM == 0)
#define DM_OWNED_GUEST_FLAG_MASK	0UL
#elif defined(CONFIG_RELEASE)
#define DM_OWNED_GUEST_FLAG_MASK	(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH \
					| GUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)
#else
#define DM_OWNED_GUEST_FLAG_MASK	(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH \
					| GUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING | GUEST_FLAG_PMU_PASSTHROUGH)
#endif

struct arch_vm_config {
	/*
	 * below are variable length members (per build).
	 * Service VM can get the vm_configs[] array through hypercall, but Service VM may not
	 * need to parse these members.
	 */

	uint16_t num_pclosids; /* This defines the number of elements in the array pointed to by pclosids */
	/* pclosids: a pointer to an array of physical CLOSIDs (pCLOSIDs)) that is defined in vm_configurations.c
	 * by vmconfig,
	 * applicable only if CONFIG_RDT_ENABLED is defined on CAT capable platforms.
	 * The number of elements in the array must be equal to the value given by num_pclosids
	 */
	uint16_t *pclosids;

	/* max_type_pcbm (type: l2 or l3) specifies the allocated portion of physical cache
	 * for the VM and is a contiguous capacity bitmask (CBM) starting at bit position low
	 * (the lowest assigned physical cache way) and ending at position high
	 * (the highest assigned physical cache way, inclusive).
	 * As CBM only allows contiguous '1' combinations, so max_type_pcbm essentially
	 * is a bitmask that selects/covers all the physical cache ways assigned to the VM.
	 */
	uint32_t max_l2_pcbm;
	uint32_t max_l3_pcbm;

	bool pt_tpm2;

	bool pt_p2sb_bar; /* whether to passthru p2sb bridge to pre-launched VM or not */

	struct epc_section epc;				/* EPC memory configuration of VM */
};

#endif /* X86_VM_CONFIG_H_ */
