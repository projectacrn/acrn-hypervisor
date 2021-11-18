/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <asm/guest/ept.h>
#include <asm/vm_config.h>
#include <asm/mmu.h>
#include <asm/guest/optee.h>
#include <asm/trampoline.h>
#include <reloc.h>
#include <hypercall.h>
#include <logmsg.h>

void prepare_tee_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config)
{
	uint64_t hv_hpa;

	/*
	 * Only need to map following things, let init_vpci to map the secure devices
	 * if any.
	 *
	 * 1. go through physical e820 table, to ept add all system memory entries.
	 * 2. remove hv owned memory.
	 */
	if ((vm_config->guest_flags & GUEST_FLAG_TEE) != 0U) {
		vm->e820_entry_num = get_e820_entries_count();
		vm->e820_entries = (struct e820_entry *)get_e820_entry();

		prepare_vm_identical_memmap(vm, E820_TYPE_RAM, EPT_WB | EPT_RWX);

		hv_hpa = hva2hpa((void *)(get_hv_image_base()));
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, hv_hpa, get_hv_ram_size());
	}
}

static struct acrn_vm *get_companion_vm(struct acrn_vm *vm)
{
	return get_vm_from_vmid(get_vm_config(vm->vm_id)->companion_vm_id);
}

static int32_t tee_switch_to_ree(struct acrn_vcpu *vcpu)
{
	uint64_t rdi, rsi, rdx, rbx;
	struct acrn_vm *ree_vm;
	struct acrn_vcpu *ree_vcpu;
	int32_t ret = -EINVAL;

	rdi = vcpu_get_gpreg(vcpu, CPU_REG_RDI);
	rsi = vcpu_get_gpreg(vcpu, CPU_REG_RSI);
	rdx = vcpu_get_gpreg(vcpu, CPU_REG_RDX);
	rbx = vcpu_get_gpreg(vcpu, CPU_REG_RBX);

	ree_vm = get_companion_vm(vcpu->vm);
	ree_vcpu = vcpu_from_pid(ree_vm, get_pcpu_id());

	if (ree_vcpu != NULL) {
		/*
		 * We should avoid copy any values to REE registers,
		 * If this is a FIQ return.
		 */
		if (rdi != OPTEE_RETURN_FIQ_DONE) {
			vcpu_set_gpreg(ree_vcpu, CPU_REG_RDI, rdi);
			vcpu_set_gpreg(ree_vcpu, CPU_REG_RSI, rsi);
			vcpu_set_gpreg(ree_vcpu, CPU_REG_RDX, rdx);
			vcpu_set_gpreg(ree_vcpu, CPU_REG_RBX, rbx);
		}

		sleep_thread(&vcpu->thread_obj);
		ret = 0;
	} else {
		pr_fatal("No REE vCPU running on this pCPU%u, \n", get_pcpu_id());
	}

	return ret;
}

static int32_t ree_switch_to_tee(struct acrn_vcpu *vcpu)
{
	uint64_t rax, rdi, rsi, rdx, rbx, rcx;
	struct acrn_vm *tee_vm;
	struct acrn_vcpu *tee_vcpu;
	int32_t ret = -EINVAL;

	rax = vcpu_get_gpreg(vcpu, CPU_REG_RAX);
	rdi = vcpu_get_gpreg(vcpu, CPU_REG_RDI);
	rsi = vcpu_get_gpreg(vcpu, CPU_REG_RSI);
	rdx = vcpu_get_gpreg(vcpu, CPU_REG_RDX);
	rbx = vcpu_get_gpreg(vcpu, CPU_REG_RBX);
	rcx = vcpu_get_gpreg(vcpu, CPU_REG_RCX);

	tee_vm = get_companion_vm(vcpu->vm);
	tee_vcpu = vcpu_from_pid(tee_vm, get_pcpu_id());
	if (tee_vcpu != NULL) {
		vcpu_set_gpreg(tee_vcpu, CPU_REG_RAX, rax);
		vcpu_set_gpreg(tee_vcpu, CPU_REG_RDI, rdi);
		vcpu_set_gpreg(tee_vcpu, CPU_REG_RSI, rsi);
		vcpu_set_gpreg(tee_vcpu, CPU_REG_RDX, rdx);
		vcpu_set_gpreg(tee_vcpu, CPU_REG_RBX, rbx);
		vcpu_set_gpreg(tee_vcpu, CPU_REG_RCX, rcx);

		wake_thread(&tee_vcpu->thread_obj);

		ret = 0;
	} else {
		pr_fatal("No TEE vCPU running on this pCPU%u, \n", get_pcpu_id());
	}

	return ret;
}

int32_t hcall_handle_tee_vcpu_boot_done(struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	struct acrn_vm *ree_vm;
	uint64_t rdi;

	/*
	 * The (RDI == 1) indicates to start REE VM, otherwise only need
	 * to sleep the corresponding TEE vCPU.
	 */
	rdi = vcpu_get_gpreg(vcpu, CPU_REG_RDI);
	if (rdi == 1UL) {
		ree_vm = get_companion_vm(vcpu->vm);
		start_vm(ree_vm);
	}

	sleep_thread(&vcpu->thread_obj);

	return 0;
}

int32_t hcall_switch_ee(struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	int32_t ret = 0;

	if ((get_vm_config(vcpu->vm->vm_id)->guest_flags & GUEST_FLAG_TEE) != 0U) {
		ret = tee_switch_to_ree(vcpu);
	} else if ((get_vm_config(vcpu->vm->vm_id)->guest_flags & GUEST_FLAG_REE) != 0U) {
		ret = ree_switch_to_tee(vcpu);
	}

	return ret;
}
