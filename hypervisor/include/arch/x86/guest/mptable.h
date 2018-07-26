/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MPTABLE_H__
#define	__MPTABLE_H__

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

/* Base table entries */

#define	MPCT_ENTRY_PROCESSOR	0U
#define	MPCT_ENTRY_BUS		1U
#define	MPCT_ENTRY_LOCAL_INT	4U

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

#define PROCENTRY_FLAG_EN	0x01U
#define PROCENTRY_FLAG_BP	0x02U

struct bus_entry {
	uint8_t	type;
	uint8_t	bus_id;
	uint8_t	bus_type[6];
} __attribute__((packed));

struct int_entry{
	uint8_t	type;
	uint8_t	int_type;
	uint16_t int_flags;
	uint8_t	src_bus_id;
	uint8_t	src_bus_irq;
	uint8_t	dst_apic_id;
	uint8_t	dst_apic_int;
} __attribute__((packed));

#define	INTENTRY_TYPE_NMI	1U
#define	INTENTRY_TYPE_EXTINT	3U

#define	INTENTRY_FLAGS_POLARITY_CONFORM		0x0U
#define	INTENTRY_FLAGS_TRIGGER_CONFORM		0x0U

int mptable_build(struct vm *vm, uint16_t ncpu);
#endif /* !__MPTABLE_H__ */
