/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <bsp_extern.h>
#include <hv_debug.h>
#include <multiboot.h>

/* Local variables */

/* VMs list */
struct list_head vm_list = {
	.next = &vm_list,
	.prev = &vm_list,
};

/* Lock for VMs list */
spinlock_t vm_list_lock = {
	.head = 0,
	.tail = 0
};

/* used for vmid allocation. And this means the max vm number is 64 */
static unsigned long vmid_bitmap;

static void init_vm(struct vm_description *vm_desc,
		struct vm *vm_handle)
{
	/* Populate VM attributes from VM description */
	if (is_vm0(vm_handle)) {
		/* Allocate all cpus to vm0 at the beginning */
		vm_handle->hw.num_vcpus = phy_cpu_num;
		vm_handle->hw.exp_num_vcpus = vm_desc->vm_hw_num_cores;
	} else {
		vm_handle->hw.num_vcpus = vm_desc->vm_hw_num_cores;
		vm_handle->hw.exp_num_vcpus = vm_desc->vm_hw_num_cores;
	}
	vm_handle->state_info.privilege = vm_desc->vm_state_info_privilege;
	vm_handle->state_info.boot_count = 0;
}

/* return a pointer to the virtual machine structure associated with
 * this VM ID
 */
struct vm *get_vm_from_vmid(int vm_id)
{
	struct vm *vm = NULL;
	struct list_head *pos;

	spinlock_obtain(&vm_list_lock);
	list_for_each(pos, &vm_list) {
		vm = list_entry(pos, struct vm, list);
		if (vm->attr.id == vm_id) {
			spinlock_release(&vm_list_lock);
			return vm;
		}
	}
	spinlock_release(&vm_list_lock);

	return NULL;
}

int create_vm(struct vm_description *vm_desc, struct vm **rtn_vm)
{
	unsigned int id;
	struct vm *vm;
	int status = 0;

	if ((vm_desc == NULL) || (rtn_vm == NULL))
		status = -EINVAL;

	if (status == 0) {
		/* Allocate memory for virtual machine */
		vm = calloc(1, sizeof(struct vm));
		ASSERT(vm != NULL, "vm allocation failed");

		/*
		 * Map Virtual Machine to its VM Description
		 */
		init_vm(vm_desc, vm);


		/* Init mmio list */
		INIT_LIST_HEAD(&vm->mmio_list);

		if (vm->hw.num_vcpus == 0)
			vm->hw.num_vcpus = phy_cpu_num;

		vm->hw.vcpu_array =
			calloc(1, sizeof(struct vcpu *) * vm->hw.num_vcpus);
		ASSERT(vm->hw.vcpu_array != NULL,
			"vcpu_array allocation failed");

		for (id = 0; id < sizeof(long) * 8; id++)
			if (bitmap_test_and_set(id, &vmid_bitmap) == 0)
				break;
		vm->attr.id = vm->attr.boot_idx = id;
		snprintf(&vm->attr.name[0], MAX_VM_NAME_LEN, "vm_%d",
			vm->attr.id);

		atomic_store(&vm->hw.created_vcpus, 0);

		/* gpa_lowtop are used for system start up */
		vm->hw.gpa_lowtop = 0;
		/* Only for SOS: Configure VM software information */
		/* For UOS: This VM software information is configure in DM */
		if (is_vm0(vm)) {
			prepare_vm0_memmap_and_e820(vm);
#ifndef CONFIG_EFI_STUB
			status = init_vm0_boot_info(vm);
#endif
		} else {
			/* populate UOS vm fields according to vm_desc */
			vm->sworld_control.sworld_enabled =
				vm_desc->sworld_enabled;
			memcpy_s(&vm->GUID[0], sizeof(vm->GUID),
						&vm_desc->GUID[0],
						sizeof(vm_desc->GUID));
		}

		INIT_LIST_HEAD(&vm->list);
		spinlock_obtain(&vm_list_lock);
		list_add(&vm->list, &vm_list);
		spinlock_release(&vm_list_lock);

		/* Ensure VM software information obtained */
		if (status == 0) {

			/* Set up IO bit-mask such that VM exit occurs on
			 * selected IO ranges
			 */
			setup_io_bitmap(vm);

			vm_setup_cpu_state(vm);

			/* Create virtual uart */
			if (is_vm0(vm))
				vm->vuart = vuart_init(vm);

			vm->vpic = vpic_init(vm);

			/* vpic wire_mode default is INTR */
			vm->vpic_wire_mode = VPIC_WIRE_INTR;

			/* Allocate full emulated vIOAPIC instance */
			vm->arch_vm.virt_ioapic = vioapic_init(vm);

			/* Populate return VM handle */
			*rtn_vm = vm;
			vm->sw.io_shared_page = NULL;

			status = set_vcpuid_entries(vm);
			if (status)
				vm->state = VM_CREATED;
		}

	}

	/* Return status to caller */
	return status;
}

int shutdown_vm(struct vm *vm)
{
	int i, status = 0;
	struct vcpu *vcpu = NULL;

	if (vm == NULL)
		return -EINVAL;

	pause_vm(vm);

	/* Only allow shutdown paused vm */
	if (vm->state != VM_PAUSED)
		return -EINVAL;

	foreach_vcpu(i, vm, vcpu) {
		reset_vcpu(vcpu);
		destroy_vcpu(vcpu);
	}

	spinlock_obtain(&vm_list_lock);
	list_del_init(&vm->list);
	spinlock_release(&vm_list_lock);

	ptdev_release_all_entries(vm);

	/* cleanup and free vioapic */
	vioapic_cleanup(vm->arch_vm.virt_ioapic);

	/* Destroy secure world */
	if (vm->sworld_control.sworld_enabled)
		destroy_secure_world(vm);
	/* Free EPT allocated resources assigned to VM */
	destroy_ept(vm);

	/* Free MSR bitmap */
	free(vm->arch_vm.msr_bitmap);

	/* TODO: De-initialize I/O Emulation */
	free_io_emulation_resource(vm);

	/* Free iommu_domain */
	if (vm->iommu_domain)
		destroy_iommu_domain(vm->iommu_domain);

	bitmap_clr(vm->attr.id, &vmid_bitmap);

	if (vm->vpic)
		vpic_cleanup(vm);

	free(vm->hw.vcpu_array);

	/* TODO: De-Configure HV-SW */
	/* Deallocate VM */
	free(vm);

	/* Return status to caller */
	return status;
}

int start_vm(struct vm *vm)
{
	struct vcpu *vcpu = NULL;

	vm->state = VM_STARTED;

	/* Only start BSP (vid = 0) and let BSP start other APs */
	vcpu = vcpu_from_vid(vm, 0);
	ASSERT(vcpu != NULL, "vm%d, vcpu0", vm->attr.id);
	schedule_vcpu(vcpu);

	return 0;
}

/*
 * DM only pause vm for shutdown/reboot. If we need to
 * extend the pause vm for DM, this API should be extended.
 */
int pause_vm(struct vm *vm)
{
	int i;
	struct vcpu *vcpu = NULL;

	if (vm->state == VM_PAUSED)
		return 0;

	vm->state = VM_PAUSED;

	foreach_vcpu(i, vm, vcpu)
		pause_vcpu(vcpu, VCPU_ZOMBIE);

	return 0;
}

int vm_resume(struct vm *vm)
{
	int i;
	struct vcpu *vcpu = NULL;

	foreach_vcpu(i, vm, vcpu)
		resume_vcpu(vcpu);

	vm->state = VM_STARTED;

	return 0;
}

/* Finally, we will remove the array and only maintain vm0 desc */
struct vm_description *get_vm_desc(int idx)
{
	struct vm_description_array *vm_desc_array;

	/* Obtain base of user defined VM description array data
	 * structure
	 */
	vm_desc_array = (struct vm_description_array *)get_vm_desc_base();
	/* Obtain VM description array base */
	if (idx >= vm_desc_array->num_vm_desc)
		return NULL;
	else
		return &vm_desc_array->vm_desc_array[idx];
}

/* Create vm/vcpu for vm0 */
int prepare_vm0(void)
{
	int i, ret;
	struct vm *vm = NULL;
	struct vm_description *vm_desc = NULL;

	vm_desc = get_vm_desc(0);
	ASSERT(vm_desc, "get vm desc failed");
	ret = create_vm(vm_desc, &vm);
	ASSERT(ret == 0, "VM creation failed!");

	/* Allocate all cpus to vm0 at the beginning */
	for (i = 0; i < phy_cpu_num; i++)
		prepare_vcpu(vm, i);

	/* start vm0 BSP automatically */
	start_vm(vm);

	pr_fatal("Start VM0");

	return 0;
}

static inline bool vcpu_in_vm_desc(struct vcpu *vcpu,
		struct vm_description *vm_desc)
{
	int i;

	for (i = 0; i < vm_desc->vm_hw_num_cores; i++) {
		if (vcpu->pcpu_id == vm_desc->vm_hw_logical_core_ids[i])
			return true;
	}

	return false;
}

/*
 * fixup vm0 for expected vcpu:
 *  vm0 is starting with all physical cpus, it's mainly for UEFI boot to
 *  handle all physical mapped APs wakeup during boot service exit.
 *  this fixup is used to pause then destroy non-expect-enabled vcpus from VM0.
 *
 * NOTE: if you want to enable mult-vpucs for vm0, please make sure the pcpu_id
 *       is in order, for example:
 *       - one vcpu:    VM0_CPUS[VM0_NUM_CPUS] = {0};
 *       - two vcpus:   VM0_CPUS[VM0_NUM_CPUS] = {0, 1};
 *       - three vcpus: VM0_CPUS[VM0_NUM_CPUS] = {0, 1, 2};
 */
void vm_fixup(struct vm *vm)
{
	if (is_vm0(vm) && (vm->hw.exp_num_vcpus < vm->hw.num_vcpus)) {
		struct vm_description *vm_desc = NULL;
		struct vcpu *vcpu;
		int i;

		vm_desc = get_vm_desc(0);
		foreach_vcpu(i, vm, vcpu) {
			if (!vcpu_in_vm_desc(vcpu, vm_desc)) {
				pause_vcpu(vcpu, VCPU_ZOMBIE);
				reset_vcpu(vcpu);
				destroy_vcpu(vcpu);
			}
		}

		vm->hw.num_vcpus = vm->hw.exp_num_vcpus;
	}
}
