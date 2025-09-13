/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <per_cpu.h>
#include <vcpu.h>
#include <vm.h>
#include <logmsg.h>
#include <sbuf.h>
#include <sprintf.h>
#include <asm/host_pm.h>

static struct acrn_vm vm_array[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE);

static struct acrn_vm *service_vm_ptr = NULL;

uint16_t get_unused_vmid(void)
{
	uint16_t vm_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if ((vm_config->name[0] == '\0') && ((vm_config->guest_flags & GUEST_FLAG_STATIC_VM) == 0U)) {
			break;
		}
	}
	return (vm_id < CONFIG_MAX_VM_NUM) ? (vm_id) : (ACRN_INVALID_VMID);
}

uint16_t get_vmid_by_name(const char *name)
{
	uint16_t vm_id;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if ((*name != '\0') && vm_has_matched_name(vm_id, name)) {
			break;
		}
	}
	return (vm_id < CONFIG_MAX_VM_NUM) ? (vm_id) : (ACRN_INVALID_VMID);
}

/**
 * @pre vm != NULL
 */
bool is_poweroff_vm(const struct acrn_vm *vm)
{
	return (vm->state == VM_POWERED_OFF);
}

/**
 * @pre vm != NULL
 */
bool is_created_vm(const struct acrn_vm *vm)
{
	return (vm->state == VM_CREATED);
}

bool is_service_vm(const struct acrn_vm *vm)
{
	return (vm != NULL)  && (get_vm_config(vm->vm_id)->load_order == SERVICE_VM);
}

/**
 * @pre vm != NULL
 * @pre vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_postlaunched_vm(const struct acrn_vm *vm)
{
	return (get_vm_config(vm->vm_id)->load_order == POST_LAUNCHED_VM);
}

/**
 * @pre vm != NULL
 * @pre vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_prelaunched_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vm->vm_id);
	return (vm_config->load_order == PRE_LAUNCHED_VM);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_rt_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_RT) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 *
 * Stateful VM refers to VM that has its own state (such as internal file cache),
 * and will experience state loss (file system corruption) if force powered down.
 */
bool is_stateful_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	/* TEE VM has GUEST_FLAG_STATELESS set implicitly */
	return ((vm_config->guest_flags & GUEST_FLAG_STATELESS) == 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_static_configured_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_STATIC_VM) != 0U);
}

struct acrn_vm *get_highest_severity_vm(bool runtime)
{
	uint16_t vm_id, highest_vm_id = 0U;

	for (vm_id = 1U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if (runtime && is_poweroff_vm(get_vm_from_vmid(vm_id))) {
			/* If vm is non-existed or shutdown, it's not highest severity VM */
			continue;
		}

		if (get_vm_severity(vm_id) > get_vm_severity(highest_vm_id)) {
			highest_vm_id = vm_id;
		}
	}

	return get_vm_from_vmid(highest_vm_id);
}

/**
 * @pre vm != NULL
 */
void poweroff_if_rt_vm(struct acrn_vm *vm)
{
	if (is_rt_vm(vm) && !is_paused_vm(vm) && !is_poweroff_vm(vm)) {
		vm->state = VM_READY_TO_POWEROFF;
	}
}

/**
 * if there is RT VM return true otherwise return false.
 */
bool has_rt_vm(void)
{
	uint16_t vm_id;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if (is_rt_vm(get_vm_from_vmid(vm_id))) {
			break;
		}
	}

	return (vm_id != CONFIG_MAX_VM_NUM);
}

/*
 * @pre vm != NULL
 */
void get_vm_lock(struct acrn_vm *vm)
{
	spinlock_obtain(&vm->vm_state_lock);
}
/*
 * @pre vm != NULL
 */
void put_vm_lock(struct acrn_vm *vm)
{
	spinlock_release(&vm->vm_state_lock);
}
/**
 * @pre vm != NULL
 */
bool is_paused_vm(const struct acrn_vm *vm)
{
	return (vm->state == VM_PAUSED);
}

/**
 * @pre vm_config != NULL
 */
static inline uint16_t get_configured_bsp_pcpu_id(const struct acrn_vm_config *vm_config)
{
	/*
	 * The set least significant bit represents the pCPU ID for BSP
	 * vm_config->cpu_affinity has been sanitized to contain valid pCPU IDs
	 */
	return ffs64(vm_config->cpu_affinity);
}

/**
 * return a pointer to the virtual machine structure associated with
 * this VM ID
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 */
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id)
{
	return &vm_array[vm_id];
}

/* return a pointer to the virtual machine structure of Service VM */
struct acrn_vm *get_service_vm(void)
{
	ASSERT(service_vm_ptr != NULL, "service_vm_ptr is NULL");

	return service_vm_ptr;
}

bool is_ready_for_system_shutdown(void)
{
	bool ret = true;
	uint16_t vm_id;
	struct acrn_vm *vm;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm = get_vm_from_vmid(vm_id);
		/* TODO: Update code to cover hybrid mode */
		if (!is_poweroff_vm(vm) && is_stateful_vm(vm)) {
			ret = false;
			break;
		}
	}

	return ret;
}

/**
 * @pre vm_config != NULL
 * @Application constraint: The validity of vm_config->cpu_affinity should be guaranteed before run-time.
 */
void launch_vms(uint16_t pcpu_id)
{
	uint16_t vm_id;
	struct acrn_vm *vm;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);

		if (((vm_config->guest_flags & GUEST_FLAG_REE) != 0U) &&
		    ((vm_config->guest_flags & GUEST_FLAG_TEE) != 0U)) {
			ASSERT(false, "%s: Wrong VM (VM id: %u) configuration, can't set both REE and TEE flags",
				__func__, vm_id);
		}

		if ((vm_config->load_order == SERVICE_VM) || (vm_config->load_order == PRE_LAUNCHED_VM)) {
			if (pcpu_id == get_configured_bsp_pcpu_id(vm_config)) {
				if (vm_config->load_order == SERVICE_VM) {
					service_vm_ptr = &vm_array[vm_id];
				}

				/*
				 * We can only start a VM when there is no error in prepare_vm.
				 * Otherwise, print out the corresponding error.
				 *
				 * We can only start REE VM when get the notification from TEE VM.
				 * so skip "start_vm" here for REE, and start it in TEE hypercall
				 * HC_TEE_VCPU_BOOT_DONE.
				 */
				if (create_vm(vm_id, vm_config->cpu_affinity, vm_config, &vm) == 0) {
					if ((vm_config->guest_flags & GUEST_FLAG_REE) != 0U) {
						/* Nothing need to do here, REE will start in TEE hypercall */
					} else {
						if (prepare_os_image(vm) == 0) {
							start_vm(vm);
							pr_acrnlog("Start VM id: %x name: %s", vm_id, vm_config->name);
						}
					}
				}
			}
		}
	}
}

/**
 * @pre vm != NULL
 * @pre vm->state == VM_CREATED
 */
void start_vm(struct acrn_vm *vm)
{
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BSP_CPU_ID);
	arch_vm_prepare_bsp(vcpu);
	launch_vcpu(vcpu);
	vm->state = VM_RUNNING;
}

/**
 * @pre vm != NULL
 */
void pause_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	if (((is_severity_pass(vm->vm_id)) && (vm->state == VM_RUNNING)) ||
			(vm->state == VM_READY_TO_POWEROFF) ||
			(vm->state == VM_CREATED)) {
		foreach_vcpu(i, vm, vcpu) {
			zombie_vcpu(vcpu);
		}
		vm->state = VM_PAUSED;
	}
}

int32_t destroy_vm(struct acrn_vm *vm)
{
	int32_t ret = 0;
	uint16_t i;
	struct acrn_vm_config *vm_config = NULL;
	struct acrn_vcpu *vcpu = NULL;

	/* Only allow shutdown paused vm */
	vm->state = VM_POWERED_OFF;

	if (is_service_vm(vm)) {
		sbuf_reset();
	}

	/* TODO: Same as create_vm, we have several common module initialization
	 * logic inside arch_deinit_vm. */
	ret = arch_deinit_vm(vm);

	foreach_vcpu(i, vm, vcpu) {
		destroy_vcpu(vcpu);
	}

	/* after guest_flags not used, then clear it */
	vm_config = get_vm_config(vm->vm_id);
	vm_config->guest_flags &= ~DM_OWNED_GUEST_FLAG_MASK;
	if (!is_static_configured_vm(vm)) {
		memset(vm_config->name, 0U, MAX_VM_NAME_LEN);
	}

	/* Return status to caller */
	return ret;
}

/**
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL && rtn_vm != NULL
 * @pre vm->state == VM_POWERED_OFF
 */
int32_t create_vm(uint16_t vm_id, uint64_t pcpu_bitmap, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm)
{
	int32_t status = 0;
	uint16_t pcpu_id;
	struct acrn_vm *vm = NULL;

	/* Allocate memory for virtual machine */
	vm = &vm_array[vm_id];
	vm->vm_id = vm_id;
	vm->hw.created_vcpus = 0U;

	if (vm_config->name[0] == '\0') {
		/* if VM name is not configured, specify with VM ID */
		snprintf(vm_config->name, 16, "ACRN VM_%d", vm_id);
	}

	(void)memcpy_s(&vm->name[0], MAX_VM_NAME_LEN, &vm_config->name[0], MAX_VM_NAME_LEN);

	vm->sw.vm_event_sbuf = NULL;
	vm->sw.io_shared_page = NULL;
	vm->sw.asyncio_sbuf = NULL;

	if ((vm_config->load_order == POST_LAUNCHED_VM)
			&& ((vm_config->guest_flags & GUEST_FLAG_IO_COMPLETION_POLLING) != 0U)) {
		/* enable IO completion polling mode per its guest flags in vm_config. */
		vm->sw.is_polling_ioreq = true;
	}

	spinlock_init(&vm->stg2pt_lock);
	spinlock_init(&vm->emul_mmio_lock);
	vm->nr_emul_mmio_regions = 0U;

	/* TODO: Some logic inside arch_init_vm can also be moved to common but
	 * we didn't come up with abstraction good enough to capture dependencies. Leave those
	 * inside arch for now. */
	status = arch_init_vm(vm, vm_config);

	if (status == 0) {
		/* We have assumptions:
		 *   1) vcpus used by Service VM has been offlined by DM before User VM re-use it.
		 *   2) pcpu_bitmap passed sanitization is OK for vcpu creating.
		 */
		vm->hw.cpu_affinity = pcpu_bitmap;

		uint64_t tmp64 = pcpu_bitmap;
		while (tmp64 != 0UL) {
			pcpu_id = ffs64(tmp64);
			bitmap_clear_non_atomic(pcpu_id, &tmp64);
			status = create_vcpu(vm, pcpu_id);
			if (status != 0) {
				break;
			}
		}
	}

	if (status == 0) {
		vm->state = VM_CREATED;

		/* Populate return VM handle */
		*rtn_vm = vm;
	}

	return status;
}

/**
 * "Warm" reset a VM.
 * To "Cold" reset a VM, simply destroy and re-create.
 *
 * @pre vm->state == VM_PAUSED
 */
int32_t reset_vm(struct acrn_vm *vm)
{
	int32_t ret = -1;

	ret = arch_reset_vm(vm);
	if (ret == 0) {
		vm->state = VM_CREATED;
	}

	return ret;
}

void make_shutdown_vm_request(uint16_t pcpu_id)
{
	bitmap_set(NEED_SHUTDOWN_VM, &per_cpu(pcpu_flag, pcpu_id));
	if (get_pcpu_id() != pcpu_id) {
		arch_smp_call_kick_pcpu(pcpu_id);
	}
}

bool need_shutdown_vm(uint16_t pcpu_id)
{
	return bitmap_test_and_clear(NEED_SHUTDOWN_VM, &per_cpu(pcpu_flag, pcpu_id));
}

void shutdown_vm_from_idle(uint16_t pcpu_id)
{
	uint16_t vm_id;
	uint64_t *vms = &per_cpu(shutdown_vm_bitmap, pcpu_id);
	struct acrn_vm *vm;

	for (vm_id = fls64(*vms); vm_id < CONFIG_MAX_VM_NUM; vm_id = fls64(*vms)) {
		vm = get_vm_from_vmid(vm_id);
		get_vm_lock(vm);
		if (is_paused_vm(vm)) {
			(void)destroy_vm(vm);
			if (is_ready_for_system_shutdown()) {
				shutdown_system();
			}
		}
		put_vm_lock(vm);
		bitmap_clear_non_atomic(vm_id, vms);
	}
}
