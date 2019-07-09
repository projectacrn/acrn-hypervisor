/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/************************************************************************
 *
 *   FILE NAME
 *
 *       mptable.h
 *
 *   DESCRIPTION
 *
 *       This file defines API and extern variable for VM mptable info
 *
 ************************************************************************/
/**********************************/
/* EXTERNAL VARIABLES             */
/**********************************/
#ifndef MPTABLE_H
#define MPTABLE_H

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

#define LAPIC_VERSION_NUM	16U

#define MP_SPECREV		4U
#define MPFP_SIG		"_MP_"

/* Configuration header defines */
#define MPCH_SIG		"PCMP"
#define MPCH_OEMID		"ACRN    "
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

/* MP Floating Pointer Structure */
struct mpfps {
	char signature[4];
	uint32_t pap;
	uint8_t	length;
	uint8_t	spec_rev;
	uint8_t	checksum;
	uint8_t	config_type;
	uint8_t	mpfb2;
	uint8_t	mpfb3;
	uint8_t	mpfb4;
	uint8_t	mpfb5;
} __packed;

/* MP Configuration Table Header */
struct mpcth {
	char signature[4];
	uint16_t base_table_length;
	uint8_t	spec_rev;
	uint8_t	checksum;
	char oem_id[8];
	char product_id[12];
	uint32_t oem_table_pointer;
	uint16_t oem_table_size;
	uint16_t entry_count;
	uint32_t apic_address;
	uint16_t extended_table_length;
	uint8_t	extended_table_checksum;
	uint8_t	reserved;
} __packed;

struct proc_entry {
	uint8_t	type;
	uint8_t	apic_id;
	uint8_t	apic_version;
	uint8_t	cpu_flags;
	uint32_t cpu_signature;
	uint32_t feature_flags;
	uint32_t reserved1;
	uint32_t reserved2;
} __packed;

struct bus_entry {
	uint8_t	type;
	uint8_t	bus_id;
	char bus_type[6];
} __packed;

struct int_entry {
	uint8_t	type;
	uint8_t	int_type;
	uint16_t int_flags;
	uint8_t	src_bus_id;
	uint8_t	src_bus_irq;
	uint8_t	dst_apic_id;
	uint8_t	dst_apic_int;
} __packed;

struct mptable_info {
	struct mpfps		mpfp;
	struct mpcth		mpch;
	struct bus_entry	bus_entry_array[MPE_NUM_BUSES];
	struct int_entry	int_entry_array[MPEII_NUM_LOCAL_IRQ];
	struct proc_entry	proc_entry_array[CONFIG_MAX_PCPU_NUM];
};

int32_t mptable_build(struct acrn_vm *vm);

#endif /* MPTABLE_H */
