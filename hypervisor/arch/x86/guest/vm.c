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

static struct acrn_vm vm_array[CONFIG_MAX_VM_NUM] __aligned(CPU_PAGE_SIZE);

static uint64_t vmid_bitmap;

/* used for vmid allocation. And this means the max vm number is 64 */
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

static inline void free_vm_id(const struct acrn_vm *vm)
{
	bitmap_clear_lock(vm->vm_id, &vmid_bitmap);
}

static inline bool is_vm_valid(uint16_t vm_id)
{
	return bitmap_test(vm_id, &vmid_bitmap);
}

/* return a pointer to the virtual machine structure associated with
 * this VM ID
 */
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id)
{
	struct acrn_vm *ret;

	if (is_vm_valid(vm_id)) {
	        ret = &vm_array[vm_id];
	} else {
	        ret = NULL;
	}

	return ret;
}

/**
 * @pre vm_desc != NULL && rtn_vm != NULL
 */
int create_vm(struct vm_description *vm_desc, struct acrn_vm **rtn_vm)
{
	struct acrn_vm *vm;
	int status;
	uint16_t vm_id;

#ifdef CONFIG_PARTITION_MODE
	vm_id = vm_desc->vm_id;
	bitmap_set_lock(vm_id, &vmid_bitmap);
#else
	vm_id = alloc_vm_id();
#endif
	if (vm_id >= CONFIG_MAX_VM_NUM) {
		pr_err("%s, vm id is invalid!\n", __func__);
		return -ENODEV;
	}

	/* Allocate memory for virtual machine */
	vm = &vm_array[vm_id];
	(void)memset((void *)vm, 0U, sizeof(struct acrn_vm));
	vm->vm_id = vm_id;
#ifdef CONFIG_PARTITION_MODE
	/* Map Virtual Machine to its VM Description */
	vm->vm_desc = vm_desc;
#endif
	vm->hw.created_vcpus = 0U;
	vm->emul_mmio_regions = 0U;

	/* gpa_lowtop are used for system start up */
	vm->hw.gpa_lowtop = 0UL;

	init_ept_mem_ops(vm);
	vm->arch_vm.nworld_eptp = vm->arch_vm.ept_mem_ops.get_pml4_page(vm->arch_vm.ept_mem_ops.info, 0UL);
	sanitize_pte((uint64_t *)vm->arch_vm.nworld_eptp);

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
		vm->sworld_control.flag.supported = vm_desc->sworld_supported;
		if (vm->sworld_control.flag.supported != 0UL) {
			struct memory_ops *ept_mem_ops = &vm->arch_vm.ept_mem_ops;
			ept_mr_add(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
				hva2hpa(ept_mem_ops->get_sworld_memory_base(ept_mem_ops->info)),
				TRUSTY_EPT_REBASE_GPA, TRUSTY_RAM_SIZE, EPT_WB | EPT_RWX);
		}

		(void)memcpy_s(&vm->GUID[0], sizeof(vm->GUID),
					&vm_desc->GUID[0], sizeof(vm_desc->GUID));
#ifdef CONFIG_PARTITION_MODE
		ept_mr_add(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
				vm_desc->start_hpa, 0UL, vm_desc->mem_size,
				EPT_RWX|EPT_WB);
		init_vm_boot_info(vm);
#endif
	}

	enable_iommu();

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
#endif

	vpci_init(vm);

	/* vpic wire_mode default is INTR */
	vm->wire_mode = VPIC_WIRE_INTR;

	/* Init full emulated vIOAPIC instance */
	vioapic_init(vm);

	/* Populate return VM handle */
	*rtn_vm = vm;
	vm->sw.io_shared_page = NULL;
#ifdef CONFIG_IOREQ_POLLING
	/* Now, enable IO completion polling mode for all VMs with CONFIG_IOREQ_POLLING. */
	vm->sw.is_completion_polling = true;
#endif

	status = set_vcpuid_entries(vm);
	if (status != 0) {
		goto err;
	}

	vm->state = VM_CREATED;

	return 0;

err:

	if (vm->arch_vm.nworld_eptp != NULL) {
		(void)memset(vm->arch_vm.nworld_eptp, 0U, CPU_PAGE_SIZE);
	}

	return status;
}

/*
 * @pre vm != NULL
 */
int32_t shutdown_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;
	int32_t ret;

	pause_vm(vm);

	/* Only allow shutdown paused vm */
	if (vm->state == VM_PAUSED) {
		foreach_vcpu(i, vm, vcpu) {
			reset_vcpu(vcpu);
			offline_vcpu(vcpu);
		}

		ptdev_release_all_entries(vm);

		/* Free EPT allocated resources assigned to VM */
		destroy_ept(vm);

		/* Free iommu */
		if (vm->iommu != NULL) {
			destroy_iommu_domain(vm->iommu);
		}

		/* Free vm id */
		free_vm_id(vm);

		vpci_cleanup(vm);
		ret = 0;
	} else {
	        ret = -EINVAL;
	}

	/* Return status to caller */
	return ret;
}

/**
 *  * @pre vm != NULL
 */
int start_vm(struct acrn_vm *vm)
{
	struct acrn_vcpu *vcpu = NULL;

	vm->state = VM_STARTED;

	/* Only start BSP (vid = 0) and let BSP start other APs */
	vcpu = vcpu_from_vid(vm, 0U);
	schedule_vcpu(vcpu);

	return 0;
}

/**
 *  * @pre vm != NULL
 */
int32_t reset_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;
	int32_t ret;

	if (vm->state == VM_PAUSED) {
		foreach_vcpu(i, vm, vcpu) {
			reset_vcpu(vcpu);
		}

		if (is_vm0(vm)) {
			(void )vm_sw_loader(vm);
		}

		reset_vm_ioreqs(vm);
		vioapic_reset(vm_ioapic(vm));
		destroy_secure_world(vm, false);
		vm->sworld_control.flag.active = 0UL;
		ret = 0;
	} else {
		ret = -1;
	}

	return ret;
}

/**
 *  * @pre vm != NULL
 */
void pause_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	if (vm->state != VM_PAUSED) {
		vm->state = VM_PAUSED;

		foreach_vcpu(i, vm, vcpu) {
			pause_vcpu(vcpu, VCPU_ZOMBIE);
		}
	}
}

/**
 *  * @pre vm != NULL
 */
void resume_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

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
void resume_vm_from_s3(struct acrn_vm *vm, uint32_t wakeup_vec)
{
	struct acrn_vcpu *bsp = vcpu_from_vid(vm, 0U);

	vm->state = VM_STARTED;

	reset_vcpu(bsp);

	/* When SOS resume from S3, it will return to real mode
	 * with entry set to wakeup_vec.
	 */
	set_ap_entry(bsp, wakeup_vec);

	init_vmcs(bsp);
	schedule_vcpu(bsp);
}

#ifdef CONFIG_PARTITION_MODE
/* Create vm/vcpu for vm */
int prepare_vm(uint16_t pcpu_id)
{
	int ret = 0;
	uint16_t i;
	struct acrn_vm *vm = NULL;
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

		if (vm_sw_loader == NULL) {
			vm_sw_loader = general_sw_loader;
		}

		(void )vm_sw_loader(vm);

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
	struct acrn_vm *vm = NULL;
	struct vm_description vm0_desc;

	(void)memset((void *)&vm0_desc, 0U, sizeof(vm0_desc));
	vm0_desc.vm_hw_num_cores = phys_cpu_num;

	err = create_vm(&vm0_desc, &vm);
	if (err != 0) {
		return err;
	}

	/* Allocate all cpus to vm0 at the beginning */
	for (i = 0U; i < vm0_desc.vm_hw_num_cores; i++) {
		err = prepare_vcpu(vm, i);
		if (err != 0) {
			return err;
		}
	}

	if (vm_sw_loader == NULL) {
		vm_sw_loader = general_sw_loader;
	}

	if (is_vm0(vm)) {
		(void )vm_sw_loader(vm);
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
