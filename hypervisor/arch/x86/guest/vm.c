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
	.head = 0U,
	.tail = 0U
};

/* used for vmid allocation. And this means the max vm number is 64 */
static uint64_t vmid_bitmap;

static void init_vm(struct vm_description *vm_desc,
		struct vm *vm_handle)
{
	/* Populate VM attributes from VM description */
#ifdef CONFIG_VM0_DESC
	if (is_vm0(vm_handle)) {
		/* Allocate all cpus to vm0 at the beginning */
		vm_handle->hw.num_vcpus = phys_cpu_num;
		vm_handle->hw.exp_num_vcpus = vm_desc->vm_hw_num_cores;
	} else {
		vm_handle->hw.num_vcpus = vm_desc->vm_hw_num_cores;
		vm_handle->hw.exp_num_vcpus = vm_desc->vm_hw_num_cores;
	}
#else
	vm_handle->hw.num_vcpus = vm_desc->vm_hw_num_cores;
#endif
#ifdef CONFIG_PARTITION_HV
	vm_handle->vm_desc = vm_desc;
#endif
}

/* return a pointer to the virtual machine structure associated with
 * this VM ID
 */
struct vm *get_vm_from_vmid(uint16_t vm_id)
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
	uint16_t id;
	struct vm *vm;
	int status;

	if ((vm_desc == NULL) || (rtn_vm == NULL)) {
		pr_err("%s, invalid paramater\n", __func__);
		return -EINVAL;
	}

	/* Allocate memory for virtual machine */
	vm = calloc(1U, sizeof(struct vm));
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

	if (vm->hw.num_vcpus == 0U) {
		vm->hw.num_vcpus = phys_cpu_num;
	}

	vm->hw.vcpu_array =
		calloc(1U, sizeof(struct vcpu *) * vm->hw.num_vcpus);
	if (vm->hw.vcpu_array == NULL) {
		pr_err("%s, vcpu_array allocation failed\n", __func__);
		status = -ENOMEM;
		goto err;
	}

#ifdef CONFIG_PARTITION_HV
	vm->attr.id = vm_desc->vm_id;
	vm->attr.boot_idx = vm_desc->vm_id;
	if (bitmap_test_and_set(vm->attr.id, &vmid_bitmap)) {
		pr_fatal("vm id %d already taken\n", vm->attr.id);
	}
#else
	for (id = 0U; id < (size_t)(sizeof(vmid_bitmap) * 8U); id++) {
		if (!bitmap_test_and_set_lock(id, &vmid_bitmap)) {
			break;
		}
	}
#endif
	if (id >= (size_t)(sizeof(vmid_bitmap) * 8U)) {
		pr_err("%s, no more VMs can be supported\n", __func__);
		status = -EINVAL;
		goto err;
	}

	vm->attr.id = id;
	vm->attr.boot_idx = id;

	atomic_store16(&vm->hw.created_vcpus, 0U);

	/* gpa_lowtop are used for system start up */
	vm->hw.gpa_lowtop = 0UL;

	vm->arch_vm.nworld_eptp = alloc_paging_struct();
	vm->arch_vm.m2p = alloc_paging_struct();
	if ((vm->arch_vm.nworld_eptp == NULL) ||
			(vm->arch_vm.m2p == NULL)) {
		pr_fatal("%s, alloc memory for EPTP failed\n", __func__);
		status = -ENOMEM;
		goto err;
	}

	/* Only for SOS: Configure VM software information */
	/* For UOS: This VM software information is configure in DM */
	if (is_vm0(vm)) {
		status = prepare_vm0_memmap_and_e820(vm);
		if (status != 0) {
			goto err;
		}
#ifndef CONFIG_EFI_STUB
		status = init_vm0_boot_info(vm);
		if (status != 0) {
			goto err;
		}
#endif
	} else {
		/* populate UOS vm fields according to vm_desc */
		vm->sworld_control.sworld_enabled =
			vm_desc->sworld_enabled;
		(void)memcpy_s(&vm->GUID[0], sizeof(vm->GUID),
					&vm_desc->GUID[0],
					sizeof(vm_desc->GUID));
#ifdef CONFIG_PARTITION_HV
		ept_mr_add(vm, vm_desc->start_hpa,
					0, vm_desc->mem_size, 0x47);
		init_vm_boot_info(vm);
#endif
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
		if (vm_load_pm_s_state(vm) == 0) {
			register_pm1ab_handler(vm);
		}

		/* Create virtual uart */
		vm->vuart = vuart_init(vm);
	}
#ifdef CONFIG_PARTITION_HV
	vm->vrtc = vrtc_init(vm);

	/* Create virtual uart */
	if (vm_desc->vm_vuart)
		vm->vuart = vuart_init(vm);
#endif
	vm->vpic = vpic_init(vm);

	/* vpic wire_mode default is INTR */
	vm->wire_mode = VPIC_WIRE_INTR;

	/* Allocate full emulated vIOAPIC instance */
	vm->arch_vm.virt_ioapic = vioapic_init(vm);
	if (vm->arch_vm.virt_ioapic == NULL) {
		status = -ENODEV;
		goto err;
	}

	/* Populate return VM handle */
	*rtn_vm = vm;
	vm->sw.io_shared_page = NULL;

	status = set_vcpuid_entries(vm);
	if (status != 0) {
		goto err;
	}

	vm->state = VM_CREATED;

	return 0;

err:
	if (vm->arch_vm.virt_ioapic != NULL) {
		vioapic_cleanup(vm->arch_vm.virt_ioapic);
	}

	if (vm->vpic != NULL) {
		vpic_cleanup(vm);
	}

	if (vm->arch_vm.m2p != NULL) {
		free(vm->arch_vm.m2p);
	}

	if (vm->arch_vm.nworld_eptp != NULL) {
		free(vm->arch_vm.nworld_eptp);
	}

	if (vm->hw.vcpu_array != NULL) {
		free(vm->hw.vcpu_array);
	}

	free(vm);
	return status;
}

int shutdown_vm(struct vm *vm)
{
	int status = 0;
	uint16_t i;
	struct vcpu *vcpu = NULL;

	if (vm == NULL) {
		return -EINVAL;
	}

	pause_vm(vm);

	/* Only allow shutdown paused vm */
	if (vm->state != VM_PAUSED) {
		return -EINVAL;
	}

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
	if (vm->sworld_control.sworld_enabled) {
		destroy_secure_world(vm);
	}
	/* Free EPT allocated resources assigned to VM */
	destroy_ept(vm);

	/* Free MSR bitmap */
	free(vm->arch_vm.msr_bitmap);

	/* TODO: De-initialize I/O Emulation */
	free_io_emulation_resource(vm);

	/* Free iommu */
	if (vm->iommu != NULL) {
		destroy_iommu_domain(vm->iommu);
	}

	bitmap_clear_lock(vm->attr.id, &vmid_bitmap);

#ifdef CONFIG_PARTITION_HV
	if (vm->vrtc)
		vrtc_deinit(vm);
#endif
	if (vm->vpic != NULL) {
		vpic_cleanup(vm);
	}

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
	vcpu = vcpu_from_vid(vm, 0U);
	ASSERT(vcpu != NULL, "vm%d, vcpu0", vm->attr.id);
	schedule_vcpu(vcpu);

	return 0;
}

/*
 * DM only pause vm for shutdown/reboot. If we need to
 * extend the pause vm for DM, this API should be extended.
 */
void pause_vm(struct vm *vm)
{
	uint16_t i;
	struct vcpu *vcpu = NULL;

	if (vm->state == VM_PAUSED) {
		return;
	}

	vm->state = VM_PAUSED;

	foreach_vcpu(i, vm, vcpu) {
		pause_vcpu(vcpu, VCPU_ZOMBIE);
	}
}

void resume_vm(struct vm *vm)
{
	uint16_t i;
	struct vcpu *vcpu = NULL;

	foreach_vcpu(i, vm, vcpu) {
		resume_vcpu(vcpu);
	}

	vm->state = VM_STARTED;
}

/* Resume vm from S3 state
 *
 * To resume vm after guest enter S3 state:
 * - reset BSP
 * - BSP will be put to real mode with entry set as wakeup_vec
 * - init_vmcs BSP. We could call init_vmcs here because we know current
 *   pcpu is mapped to BSP of vm.
 */
void resume_vm_from_s3(struct vm *vm, uint32_t wakeup_vec)
{
	struct vcpu *bsp = vcpu_from_vid(vm, 0U);

	vm->state = VM_STARTED;

	reset_vcpu(bsp);
	bsp->entry_addr = (void *)(uint64_t)wakeup_vec;
	bsp->arch_vcpu.cpu_mode = CPU_MODE_REAL;
	init_vmcs(bsp);

	schedule_vcpu(bsp);
}


#ifdef CONFIG_PARTITION_HV
static int get_vm_desc_and_cpu_role(int cpu_id, struct vm_description **vm_desc,
				enum vcpu_role *role, int *vm_idx)
{
	struct vm_description_array *vm_desc_array;
	struct vm_description *vm_descriptions;
	int i, j;
	int status = 0;

	if (vm_desc == NULL || role == NULL)
		status = -EINVAL;

	if (status != 0)
		return status;

	/* Obtain base of user defined VM description array data
	 * structure
	 */
	vm_desc_array = (struct vm_description_array *)get_vm_desc_base();
	/* Obtain VM description array base */
	vm_descriptions = &vm_desc_array->vm_desc_array[0];

	status = -EINVAL;
	/* Iterate virtual machine descriptions to find matching CPU */
	for (i = 0; i < vm_desc_array->num_vm_desc; i++) {
		/* Loop through each core allocated to the VM */
		/* TODO: Need a spin-lock around this loop for SMP VMs */
		for (j = 0; j < vm_descriptions[i].vm_hw_num_cores; j++) {
			/* Check to see if the currently running CPU ID
			 * matches the VM CPU ID
			 */
			if (cpu_id !=
			    vm_descriptions[i].vm_pcpu_ids[j])
				continue;

			/* See if first CPU for this VM */
			if (j == 0) {
				/* Assign role of first CPU as primary VCPU */
				*role = VCPU_PRIMARY;
			} else {
				/* Assign role of secondary CPU */
				*role = VCPU_SECONDARY;
			}

			/* Return a pointer to the VM description */
			*vm_desc = &vm_descriptions[i];
			*vm_idx = i;
			/* Set success return status and break from loop */
			status = 0;
			break;
		}
	}

	/* Return status to caller */
	return status;
}

static struct vm_description *get_vm_desc(int idx)
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

/* Create vm/vcpu for vm */
int prepare_vm(uint16_t pcpu_id)
{
	int i, ret;
	struct vm *vm = NULL;
	struct vm_description *vm_desc = NULL;
	int vm_id;
	enum vcpu_role cpu_role = VCPU_ROLE_UNKNOWN;

	ret = get_vm_desc_and_cpu_role(pcpu_id, &vm_desc, &cpu_role,
					&vm_id);
	if (!ret && (cpu_role == VCPU_PRIMARY)) {
		vm_desc = get_vm_desc(vm_id);
		ASSERT(vm_desc, "get vm desc failed");

		ret = create_vm(vm_desc, &vm);
		ASSERT(ret == 0, "VM creation failed!");

		mptable_build(vm, vm_desc->vm_hw_num_cores);

		prepare_vcpu(vm, vm_desc->vm_pcpu_ids[0]);

		/* Prepare the AP for vm */
		for (i = 1; i < vm_desc->vm_hw_num_cores; i++)
			prepare_vcpu(vm, vm_desc->vm_pcpu_ids[i]);

		/* start vm BSP automatically */
		start_vm(vm);

		pr_acrnlog("Start VM%x", vm_id);
	}

	return ret;
}

#else

static bool is_vm0_bsp(uint16_t pcpu_id)
{
#ifdef CONFIG_VM0_DESC
	return pcpu_id == vm0_desc.vm_pcpu_ids[0];
#else
	return pcpu_id == BOOT_CPU_ID;
#endif
}

/* Create vm/vcpu for vm0 */
int prepare_vm0(void)
{
	int err;
	uint16_t i;
	struct vm *vm = NULL;
	struct vm_description *vm_desc = &vm0_desc;

#ifndef CONFIG_VM0_DESC
	vm_desc->vm_hw_num_cores = phys_cpu_num;
#endif

	err = create_vm(vm_desc, &vm);
	if (err != 0) {
		return err;
	}

	/* Allocate all cpus to vm0 at the beginning */
	for (i = 0U; i < phys_cpu_num; i++) {
		err = prepare_vcpu(vm, i);
		if (err != 0) {
			return err;
		}
	}

	/* start vm0 BSP automatically */
	err = start_vm(vm);

	pr_acrnlog("Start VM0");

	return err;
}

int prepare_vm(uint16_t pcpu_id)
{
	int err = 0;

	if (is_vm0_bsp(pcpu_id)) {
		err  = prepare_vm0();
		if (err != 0) {
			return err;
		}
	}

	return err;
}
#endif

#ifdef CONFIG_VM0_DESC
static inline bool vcpu_in_vm_desc(struct vcpu *vcpu,
		struct vm_description *vm_desc)
{
	int i;

	for (i = 0; i < vm_desc->vm_hw_num_cores; i++) {
		if (vcpu->pcpu_id == vm_desc->vm_pcpu_ids[i]) {
			return true;
		}
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
		uint16_t i;

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
#endif
