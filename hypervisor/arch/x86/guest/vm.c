/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <bsp_extern.h>
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
	int status;

	if ((vm_desc == NULL) || (rtn_vm == NULL)) {
		pr_err("%s, invalid paramater\n", __func__);
		return -EINVAL;
	}

	/* Allocate memory for virtual machine */
	vm = calloc(1, sizeof(struct vm));
	if (vm == NULL) {
		pr_err("%s, vm allocation failed\n", __func__);
		return -ENOMEM;
	}

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
	if (vm->hw.vcpu_array == NULL) {
		pr_err("%s, vcpu_array allocation failed\n", __func__);
		status = -ENOMEM;
		goto err1;
	}

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
		status = prepare_vm0_memmap_and_e820(vm);
		if (status != 0)
			goto err2;
#ifndef CONFIG_EFI_STUB
		status = init_vm0_boot_info(vm);
		if (status != 0)
			goto err2;
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

	/* Set up IO bit-mask such that VM exit occurs on
	 * selected IO ranges
	 */
	setup_io_bitmap(vm);

	vm_setup_cpu_state(vm);

	if (is_vm0(vm)) {
		/* Load pm S state data */
		vm_load_pm_s_state(vm);

		/* Create virtual uart */
		vm->vuart = vuart_init(vm);
	}
	vm->vpic = vpic_init(vm);

	/* vpic wire_mode default is INTR */
	vm->vpic_wire_mode = VPIC_WIRE_INTR;

	/* Allocate full emulated vIOAPIC instance */
	vm->arch_vm.virt_ioapic = vioapic_init(vm);
	if (vm->arch_vm.virt_ioapic == NULL) {
		status = -ENODEV;
		goto err3;
	}

	/* Populate return VM handle */
	*rtn_vm = vm;
	vm->sw.io_shared_page = NULL;

	status = set_vcpuid_entries(vm);
	if (status != 0)
		goto err4;

	vm->state = VM_CREATED;

	return 0;

err4:
	vioapic_cleanup(vm->arch_vm.virt_ioapic);
err3:
	vpic_cleanup(vm);
err2:
	free(vm->hw.vcpu_array);
err1:
	free(vm);
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

	bitmap_clear(vm->attr.id, &vmid_bitmap);

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

/* Create vm/vcpu for vm0 */
int prepare_vm0(void)
{
	int i, ret;
	struct vm *vm = NULL;
	struct vm_description *vm_desc = &vm0_desc;

	ret = create_vm(vm_desc, &vm);
	if (ret != 0)
		return ret;

	/* Allocate all cpus to vm0 at the beginning */
	for (i = 0; i < phy_cpu_num; i++)
		prepare_vcpu(vm, i);

	/* start vm0 BSP automatically */
	start_vm(vm);

	pr_acrnlog("Start VM0");

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
		struct vm_description *vm_desc = &vm0_desc;
		struct vcpu *vcpu;
		int i;

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
