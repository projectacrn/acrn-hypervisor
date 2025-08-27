/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file vcpu.h
 *
 * @brief public APIs for vcpu operations
 */

#ifndef VCPU_H
#define VCPU_H


#ifndef ASSEMBLER

#include <types.h>
#include <asm/page.h>
#include <schedule.h>
#include <event.h>
#include <io_req.h>
#include <asm/guest/vcpu.h>

/**
 * @brief vcpu
 *
 * @defgroup acrn_vcpu ACRN vcpu
 * @{
 */

/*
 * VCPU related APIs
 */

#define MAX_VCPU_EVENT_NUM	16

#define foreach_vcpu(idx, vm, vcpu)				\
	for ((idx) = 0U, (vcpu) = &((vm)->hw.vcpu_array[(idx)]);	\
		(idx) < (vm)->hw.created_vcpus;			\
		(idx)++, (vcpu) = &((vm)->hw.vcpu_array[(idx)])) \
		if ((vcpu)->state != VCPU_OFFLINE)

enum vcpu_state {
	VCPU_OFFLINE = 0U,
	VCPU_INIT,
	VCPU_RUNNING,
	VCPU_ZOMBIE,
};

struct acrn_vm;
struct acrn_vcpu {
	uint8_t stack[CONFIG_STACK_SIZE] __aligned(16);

	uint16_t vcpu_id;	/* virtual identifier for VCPU */
	struct acrn_vm *vm;		/* Reference to the VM this VCPU belongs to */

	volatile enum vcpu_state state;	/* State of this VCPU */

	struct thread_object thread_obj;
	bool launched; /* Whether the vcpu is launched on target pcpu */

	struct io_request req; /* used by io/ept emulation */

	/* pending requests bitmask. Each bit represents one arch-specific request */
	uint64_t pending_req;

	/* The first half (8) of the events are used for platform-independent
	 * events, and the latter half for platform-dependent events
	 */
	struct sched_event events[MAX_VCPU_EVENT_NUM];

	/* Architecture specific definitions for this VCPU */
	struct acrn_vcpu_arch arch;
} __aligned(PAGE_SIZE);

struct vcpu_dump {
	struct acrn_vcpu *vcpu;
	char *str;
	uint32_t str_max;
};

struct guest_mem_dump {
	struct acrn_vcpu *vcpu;
	uint64_t gva;
	uint64_t len;
};

/**
 * @}
 */
/* End of acrn_vcpu */

#endif /* ASSEMBLER */

#endif /* VCPU_H */
