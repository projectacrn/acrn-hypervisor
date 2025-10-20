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

/*
 * @brief Update the state of vCPU and state of vlapic
 *
 * The vlapic state of VM shall be updated for some vCPU
 * state update cases, such as from VCPU_INIT to VCPU_RUNNING.

 * @pre (vcpu != NULL)
 */
void vcpu_set_state(struct acrn_vcpu *vcpu, enum vcpu_state new_state);

uint16_t pcpuid_from_vcpu(const struct acrn_vcpu *vcpu);
int32_t arch_init_vcpu(struct acrn_vcpu *vcpu);
void arch_deinit_vcpu(struct acrn_vcpu *vcpu);
void arch_reset_vcpu(struct acrn_vcpu *vcpu);

void arch_vcpu_thread(struct thread_object *obj);
void arch_context_switch_out(struct thread_object *prev);
void arch_context_switch_in(struct thread_object *next);
uint64_t arch_build_stack_frame(struct acrn_vcpu *vcpu);

/**
 * @brief create a vcpu for the target vm
 *
 * Creates/allocates and initialize a vCPU instance.
 *
 * @param[in] pcpu_id created vcpu will run on this pcpu
 * @param[in] vm pointer to vm data structure
 *
 * @retval 0 vcpu created successfully, other values failed.
 */
int32_t create_vcpu(struct acrn_vm *vm, uint16_t pcpu_id);

/**
 * @brief Destroy a vcpu structure
 *
 * Unmap the vcpu with pcpu and deinitialize a vcpu structure
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @pre vcpu != NULL
 * @pre vcpu->state == VCPU_ZOMBIE
 */
void destroy_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief set the vcpu to running state, then it will be scheculed.
 *
 * Adds a vCPU into the run queue and make a reschedule request for it. It sets the vCPU state to VCPU_RUNNING.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @pre vcpu != NULL
 * @pre vcpu->state == VCPU_INIT
 */
void launch_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief reset vcpu state and values
 *
 * Reset all fields in a vCPU instance, the vCPU state is reset to VCPU_INIT.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @pre vcpu != NULL
 * @pre vcpu->state == VCPU_ZOMBIE
 */
void reset_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief pause the vcpu and set new state
 *
 * Change a vCPU state to VCPU_ZOMBIE, and make a reschedule request for it.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 */
void zombie_vcpu(struct acrn_vcpu *vcpu);

/**
 * @}
 */
/* End of acrn_vcpu */

#endif /* ASSEMBLER */

#endif /* VCPU_H */
