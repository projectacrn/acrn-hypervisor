/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#define MPTABLE_BASE		0xF0000U

/* 
 * floating pointer length + maximum length of configuration table
 * ACRN uses contiguous guest memory from 0xF0000 to place floating pointer
 * structure and config table. Maximum length of config table is 64K. So the
 * maximum length of combined floating pointer and config table can go up to
 * 64K + 16 bytes.Since we are left with only 64K from 0xF0000 to 0x100000(1MB)
 * max length is limited to 64K.
 */
#define	MPTABLE_MAX_LENGTH	65536U

#define LAPIC_VERSION		16U

#define MP_SPECREV		4U
#define MPFP_SIG		"_MP_"

/* Configuration header defines */
#define MPCH_SIG		"PCMP"
#define MPCH_OEMID		"BHyVe   "
#define MPCH_OEMID_LEN          8U
#define MPCH_PRODID             "Hypervisor  "
#define MPCH_PRODID_LEN         12U

/* Processor entry defines */
#define MPEP_SIG_FAMILY		6U
#define MPEP_SIG_MODEL		26U
#define MPEP_SIG_STEPPING	5U
#define MPEP_SIG		\
	((MPEP_SIG_FAMILY << 8U) | \
	 (MPEP_SIG_MODEL << 4U)	| \
	 (MPEP_SIG_STEPPING))

#define MPEP_FEATURES           0xBFEBFBFFU /* XXX Intel i7 */

/* Number of local intr entries */
#define	MPEII_NUM_LOCAL_IRQ	2U

/* Bus entry defines */
#define MPE_NUM_BUSES		2U
#define MPE_BUSNAME_LEN		6U
#define MPE_BUSNAME_ISA		"ISA   "
#define MPE_BUSNAME_PCI		"PCI   "

/* Base table entries */

#define	MPCT_ENTRY_PROCESSOR	0U
#define	MPCT_ENTRY_BUS		1U
#define	MPCT_ENTRY_LOCAL_INT	4U

#define PROCENTRY_FLAG_EN	0x01U
#define PROCENTRY_FLAG_BP	0x02U

#define	INTENTRY_TYPE_NMI	1U
#define	INTENTRY_TYPE_EXTINT	3U

#define	INTENTRY_FLAGS_POLARITY_CONFORM		0x0U
#define	INTENTRY_FLAGS_TRIGGER_CONFORM		0x0U

#define VM1_NUM_CPUS 2U
#define VM2_NUM_CPUS 2U

/* MP Floating Pointer Structure */
struct mpfps {
	uint8_t	signature[4];
	uint32_t pap;
	uint8_t	length;
	uint8_t	spec_rev;
	uint8_t	checksum;
	uint8_t	config_type;
	uint8_t	mpfb2;
	uint8_t	mpfb3;
	uint8_t	mpfb4;
	uint8_t	mpfb5;
} __attribute__((packed));

/* MP Configuration Table Header */
struct mpcth {
	uint8_t	signature[4];
	uint16_t base_table_length;
	uint8_t	spec_rev;
	uint8_t	checksum;
	uint8_t	oem_id[8];
	uint8_t	product_id[12];
	uint32_t oem_table_pointer;
	uint16_t oem_table_size;
	uint16_t entry_count;
	uint32_t apic_address;
	uint16_t extended_table_length;
	uint8_t	extended_table_checksum;
	uint8_t	reserved;
} __attribute__((packed));

struct proc_entry {
	uint8_t	type;
	uint8_t	apic_id;
	uint8_t	apic_version;
	uint8_t	cpu_flags;
	uint32_t cpu_signature;
	uint32_t feature_flags;
	uint32_t reserved1;
	uint32_t reserved2;
} __attribute__((packed));

struct bus_entry {
	uint8_t	type;
	uint8_t	bus_id;
	uint8_t	bus_type[6];
} __attribute__((packed));

struct int_entry {
	uint8_t	type;
	uint8_t	int_type;
	uint16_t int_flags;
	uint8_t	src_bus_id;
	uint8_t	src_bus_irq;
	uint8_t	dst_apic_id;
	uint8_t	dst_apic_int;
} __attribute__((packed));

struct mptable_info {
	struct mpfps		mpfp;
	struct mpcth		mpch;
	struct bus_entry	bus_entry_array[MPE_NUM_BUSES];
	struct int_entry	int_entry_array[MPEII_NUM_LOCAL_IRQ];
	struct proc_entry	proc_entry_array[];
};

struct mptable_info mptable_vm1 = {
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
				.entry_count = (VM1_NUM_CPUS + MPE_NUM_BUSES \
						+ MPEII_NUM_LOCAL_IRQ),
				.base_table_length = (sizeof(struct mpcth) \
					+ VM1_NUM_CPUS * sizeof(struct proc_entry) \
					+ MPE_NUM_BUSES * sizeof(struct bus_entry) \
					+ MPEII_NUM_LOCAL_IRQ * sizeof(struct int_entry))
			},
			.proc_entry_array = {
				{
					.type = MPCT_ENTRY_PROCESSOR,
					.apic_id = 0U,
					.apic_version = LAPIC_VERSION,
					.cpu_flags = PROCENTRY_FLAG_EN | PROCENTRY_FLAG_BP,
					.cpu_signature = MPEP_SIG,
					.feature_flags = MPEP_FEATURES
				},
				{
					.type = MPCT_ENTRY_PROCESSOR,
					.apic_id = 4U,
					.apic_version = LAPIC_VERSION,
					.cpu_flags = PROCENTRY_FLAG_EN,
					.cpu_signature = MPEP_SIG,
					.feature_flags = MPEP_FEATURES,
				}
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

struct mptable_info mptable_vm2 = {
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
				.entry_count = (VM2_NUM_CPUS + MPE_NUM_BUSES \
						+ MPEII_NUM_LOCAL_IRQ),
				.base_table_length = (sizeof(struct mpcth) \
					+ VM2_NUM_CPUS * sizeof(struct proc_entry) \
					+ MPE_NUM_BUSES * sizeof(struct bus_entry) \
					+ MPEII_NUM_LOCAL_IRQ * sizeof(struct int_entry))
			},
			.proc_entry_array = {
				{
					.type = MPCT_ENTRY_PROCESSOR,
					.apic_id = 6U,
					.apic_version = LAPIC_VERSION,
					.cpu_flags = PROCENTRY_FLAG_EN | PROCENTRY_FLAG_BP,
					.cpu_signature = MPEP_SIG,
					.feature_flags = MPEP_FEATURES
				},
				{
					.type = MPCT_ENTRY_PROCESSOR,
					.apic_id = 2U,
					.apic_version = LAPIC_VERSION,
					.cpu_flags = PROCENTRY_FLAG_EN,
					.cpu_signature = MPEP_SIG,
					.feature_flags = MPEP_FEATURES,
				}
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

int32_t mptable_build(struct acrn_vm *vm)
{
	char                    *startaddr;
	char                    *curraddr;
	struct mpcth            *mpch;
	struct mpfps            *mpfp;
	size_t			mptable_length, table_length;

	startaddr = (char *)gpa2hva(vm, MPTABLE_BASE);

	table_length = vm->vm_desc->mptable->mpch.base_table_length;
	mptable_length = sizeof(struct mpfps) + table_length;
	/* Copy mptable info into guest memory */
	(void)memcpy_s((void *)startaddr, MPTABLE_MAX_LENGTH,
				(void *)vm->vm_desc->mptable,
				mptable_length);

	curraddr = startaddr;
	mpfp = (struct mpfps *)curraddr;
	mpfp->checksum = mpt_compute_checksum(mpfp, sizeof(struct mpfps));
	curraddr += sizeof(struct mpfps);

	mpch = (struct mpcth *)curraddr;
	mpch->checksum = mpt_compute_checksum(mpch, mpch->base_table_length);

	return 0U;
}
