/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static struct mptable_info mptable_template = {
			.mpfp = {
				.signature = MPFP_SIG,
				.pap = MPTABLE_BASE + sizeof(struct mpfps),
				.length = 1U,
				.spec_rev = MP_SPECREV,
			},
			.mpch = {
				.signature = MPCH_SIG,
				.spec_rev = MP_SPECREV,
				.oem_id = MPCH_OEMID,
				.product_id = MPCH_PRODID,
				.apic_address = LAPIC_BASE,
			},
			.bus_entry_array = {
				{
					.type = MPCT_ENTRY_BUS,
					.bus_id = 0U,
					.bus_type = MPE_BUSNAME_PCI,
				},
				{
					.type = MPCT_ENTRY_BUS,
					.bus_id = 1U,
					.bus_type = MPE_BUSNAME_ISA,
				},
			},
			.int_entry_array = {
				{
					.type = MPCT_ENTRY_LOCAL_INT,
					.int_type = INTENTRY_TYPE_EXTINT,
					.int_flags = INTENTRY_FLAGS_POLARITY_CONFORM \
						| INTENTRY_FLAGS_TRIGGER_CONFORM,
					.dst_apic_id = 0xFFU,
					.dst_apic_int = 0U,
				},
				{
					.type = MPCT_ENTRY_LOCAL_INT,
					.int_type = INTENTRY_TYPE_NMI,
					.int_flags = INTENTRY_FLAGS_POLARITY_CONFORM \
						| INTENTRY_FLAGS_TRIGGER_CONFORM,
					.dst_apic_id = 0xFFU,
					.dst_apic_int = 1U,
				},
			},
};

static struct proc_entry proc_entry_template = {
	.type = MPCT_ENTRY_PROCESSOR,
	.apic_version = LAPIC_VERSION,
	.cpu_flags = PROCENTRY_FLAG_EN,
	.cpu_signature = MPEP_SIG,
	.feature_flags = MPEP_FEATURES
};

static uint8_t mpt_compute_checksum(void *base, size_t len)
{
	uint8_t	*bytes;
	uint8_t	sum;
	size_t length = len;

	for (bytes = base, sum = 0U; length > 0U; length--) {
		sum += *bytes;
		bytes++;
	}

	return (256U - sum);
}

/**
 * @pre vm_config != NULL
 */
int32_t mptable_build(struct acrn_vm *vm)
{
	char		*startaddr;
	char		*curraddr;
	struct mpcth	*mpch;
	struct mpfps	*mpfp;
	size_t		mptable_length;
	uint16_t	i;
	uint16_t	vcpu_num;
	uint64_t	pcpu_bitmap = 0U;
	struct mptable_info *mptable = &vm->mptable;
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vm->vm_id);
	vcpu_num = get_vm_pcpu_nums(vm_config);
	pcpu_bitmap = vm_config->pcpu_bitmap;
	(void *)memcpy_s((void *)mptable, sizeof(struct mptable_info),
		(const void *)&mptable_template, sizeof(struct mptable_info));

	mptable->mpch.entry_count = vcpu_num + MPE_NUM_BUSES + MPEII_NUM_LOCAL_IRQ;
	mptable->mpch.base_table_length = sizeof(struct mpcth)
			+ vcpu_num * sizeof(struct proc_entry)
			+ MPE_NUM_BUSES * sizeof(struct bus_entry)
			+ MPEII_NUM_LOCAL_IRQ * sizeof(struct int_entry);

	mptable_length = sizeof(struct mpfps) + mptable->mpch.base_table_length;
	if (mptable_length > MPTABLE_MAX_LENGTH) {
		return -1;
	}

	for (i = 0U; i < vcpu_num; i++) {
		uint16_t pcpu_id = ffs64(pcpu_bitmap);

		(void *)memcpy_s((void *)(mptable->proc_entry_array + i), sizeof(struct proc_entry),
			(const void *)&proc_entry_template, sizeof(struct proc_entry));
		mptable->proc_entry_array[i].apic_id = per_cpu(lapic_id, pcpu_id);
		if (i == 0) {
			mptable->proc_entry_array[i].cpu_flags |= PROCENTRY_FLAG_BP;
		}
		bitmap_clear_lock(pcpu_id, &pcpu_bitmap);
	}

	/* Copy mptable info into guest memory */
	copy_to_gpa(vm, (void *)mptable, MPTABLE_BASE, mptable_length);

	startaddr = (char *)gpa2hva(vm, MPTABLE_BASE);
	curraddr = startaddr;
	stac();
	mpfp = (struct mpfps *)curraddr;
	mpfp->checksum = mpt_compute_checksum(mpfp, sizeof(struct mpfps));
	curraddr += sizeof(struct mpfps);

	mpch = (struct mpcth *)curraddr;
	mpch->checksum = mpt_compute_checksum(mpch, mpch->base_table_length);
	clac();

	return 0U;
}
