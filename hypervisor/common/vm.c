#include <vcpu.h>
#include <vm.h>
#include <logmsg.h>
#include <sprintf.h>

static struct acrn_vm vm_array[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE);

static struct acrn_vm *service_vm_ptr = NULL;

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
