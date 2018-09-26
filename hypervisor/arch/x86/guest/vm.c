/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <bsp_extern.h>
#include <multiboot.h>
#include <vtd.h>

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

static struct vm vm_array[CONFIG_MAX_VM_NUM] __aligned(CPU_PAGE_SIZE);

#ifndef CONFIG_PARTITION_MODE
/* used for vmid allocation. And this means the max vm number is 64 */
static uint64_t vmid_bitmap;

static inline uint16_t alloc_vm_id(void)
{
	uint16_t id = ffz64(vmid_bitmap);

	while (id < (size_t)(sizeof(vmid_bitmap) * 8U)) {
		if (!bitmap_test_and_set_lock(id, &vmid_bitmap)) {
			return id;
		}
		id = ffz64(vmid_bitmap);
	}

	return INVALID_VM_ID;
}

static inline void free_vm_id(struct vm *vm)
{
	bitmap_clear_lock(vm->vm_id, &vmid_bitmap);
}
#endif

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
#ifdef CONFIG_PARTITION_MODE
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
		if (vm->vm_id == vm_id) {
			spinlock_release(&vm_list_lock);
			return vm;
		}
	}
	spinlock_release(&vm_list_lock);

	return NULL;
}

/**
 * @pre vm_desc != NULL && rtn_vm != NULL
 */
int create_vm(struct vm_description *vm_desc, struct vm **rtn_vm)
{
	struct vm *vm;
	int status;
	uint16_t vm_id;

#ifdef CONFIG_PARTITION_MODE
	vm_id = vm_desc->vm_id;
#else
	vm_id = alloc_vm_id();
#endif
	if (vm_id >= CONFIG_MAX_VM_NUM) {
		pr_err("%s, vm id is invalid!\n", __func__);
		return -ENODEV;
	}

	/* Allocate memory for virtual machine */
	vm = &vm_array[vm_id];
	(void)memset((void *)vm, 0U, sizeof(struct vm));
	vm->vm_id = vm_id;
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
		status = init_vm_boot_info(vm);
		if (status != 0) {
			goto err;
		}
#endif
		init_iommu_vm0_domain(vm);
	} else {
		/* populate UOS vm fields according to vm_desc */
		vm->sworld_control.flag.supported =
			vm_desc->sworld_supported;
		(void)memcpy_s(&vm->GUID[0], sizeof(vm->GUID),
					&vm_desc->GUID[0],
					sizeof(vm_desc->GUID));
#ifdef CONFIG_PARTITION_MODE
		ept_mr_add(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
				vm_desc->start_hpa, 0UL, vm_desc->mem_size,
				EPT_RWX|EPT_WB);
		init_vm_boot_info(vm);
#endif
	}

	INIT_LIST_HEAD(&vm->list);
	spinlock_obtain(&vm_list_lock);
	list_add(&vm->list, &vm_list);
	spinlock_release(&vm_list_lock);

	INIT_LIST_HEAD(&vm->softirq_dev_entry_list);
	spinlock_init(&vm->softirq_dev_lock);
	vm->intr_inject_delay_delta = 0UL;

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
		vuart_init(vm);
	}
	vpic_init(vm);

#ifdef CONFIG_PARTITION_MODE
	/* Create virtual uart */
	if (vm_desc->vm_vuart) {
		vuart_init(vm);
	}
	vrtc_init(vm);
	vpci_init(vm);
#endif

	/* vpic wire_mode default is INTR */
	vm->wire_mode = VPIC_WIRE_INTR;

	/* Init full emulated vIOAPIC instance */
	vioapic_init(vm);

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

	vioapic_cleanup(vm_ioapic(vm));

	if (vm->arch_vm.m2p != NULL) {
		free(vm->arch_vm.m2p);
	}

	if (vm->arch_vm.nworld_eptp != NULL) {
		free(vm->arch_vm.nworld_eptp);
	}

	if (vm->hw.vcpu_array != NULL) {
		free(vm->hw.vcpu_array);
	}
	return status;
}

/*
 * @pre vm != NULL
 */
int shutdown_vm(struct vm *vm)
{
	int status = 0;
	uint16_t i;
	struct vcpu *vcpu = NULL;

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

	/* cleanup vioapic */
	vioapic_cleanup(vm_ioapic(vm));

	/* Destroy secure world */
	if (vm->sworld_control.flag.active != 0UL) {
		destroy_secure_world(vm, true);
	}
	/* Free EPT allocated resources assigned to VM */
	destroy_ept(vm);

	/* TODO: De-initialize I/O Emulation */
	free_io_emulation_resource(vm);

	/* Free iommu */
	if (vm->iommu != NULL) {
		destroy_iommu_domain(vm->iommu);
	}

#ifndef CONFIG_PARTITION_MODE
	/* Free vm id */
	free_vm_id(vm);
#endif


#ifdef CONFIG_PARTITION_MODE
	vpci_cleanup(vm);
#endif
	free(vm->hw.vcpu_array);

	/* Return status to caller */
	return status;
}

/**
 *  * @pre vm != NULL
 */
int start_vm(struct vm *vm)
{
	struct vcpu *vcpu = NULL;

	vm->state = VM_STARTED;

	/* Only start BSP (vid = 0) and let BSP start other APs */
	vcpu = vcpu_from_vid(vm, 0U);
	ASSERT(vcpu != NULL, "vm%d, vcpu0", vm->vm_id);
	schedule_vcpu(vcpu);

	return 0;
}

/**
 *  * @pre vm != NULL
 */
int reset_vm(struct vm *vm)
{
	int i;
	struct vcpu *vcpu = NULL;

	if (vm->state != VM_PAUSED)
		return -1;

	foreach_vcpu(i, vm, vcpu) {
		reset_vcpu(vcpu);
		if (is_vcpu_bsp(vcpu))
			vm_sw_loader(vm, vcpu);

		vcpu->arch_vcpu.cpu_mode = CPU_MODE_REAL;
	}

	vioapic_reset(vm_ioapic(vm));
	destroy_secure_world(vm, false);
	vm->sworld_control.flag.active = 0UL;

	start_vm(vm);
	return 0;
}

/**
 *  * @pre vm != NULL
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

/**
 *  * @pre vm != NULL
 */
void resume_vm(struct vm *vm)
{
	uint16_t i;
	struct vcpu *vcpu = NULL;

	foreach_vcpu(i, vm, vcpu) {
		resume_vcpu(vcpu);
	}

	vm->state = VM_STARTED;
}

/**
 * @brief Resume vm from S3 state
 *
 * To resume vm after guest enter S3 state:
 * - reset BSP
 * - BSP will be put to real mode with entry set as wakeup_vec
 * - init_vmcs BSP. We could call init_vmcs here because we know current
 *   pcpu is mapped to BSP of vm.
 *
 * @vm[in]		vm pointer to vm data structure
 * @wakeup_vec[in]	The resume address of vm
 *
 * @pre vm != NULL
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

#ifdef CONFIG_PARTITION_MODE
/* Create vm/vcpu for vm */
int prepare_vm(uint16_t pcpu_id)
{
	int ret = 0;
	uint16_t i;
	struct vm *vm = NULL;
	struct vm_description *vm_desc = NULL;
	bool is_vm_bsp;

	vm_desc = pcpu_vm_desc_map[pcpu_id].vm_desc_ptr;
	is_vm_bsp = pcpu_vm_desc_map[pcpu_id].is_bsp;

	if (is_vm_bsp) {
		ret = create_vm(vm_desc, &vm);
		ASSERT(ret == 0, "VM creation failed!");

		mptable_build(vm);

		prepare_vcpu(vm, vm_desc->vm_pcpu_ids[0]);

		/* Prepare the AP for vm */
		for (i = 1U; i < vm_desc->vm_hw_num_cores; i++)
			prepare_vcpu(vm, vm_desc->vm_pcpu_ids[i]);

		/* start vm BSP automatically */
		start_vm(vm);

		pr_acrnlog("Start VM%x", vm_desc->vm_id);
	}

	return ret;
}

#else

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

	/* prepare vm0 if pcpu_id is BOOT_CPU_ID */
	if (pcpu_id == BOOT_CPU_ID) {
		err  = prepare_vm0();
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
