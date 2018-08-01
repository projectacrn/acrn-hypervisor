/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#include <bsp_extern.h>
#include <mptable.h>

#define MPTABLE_BASE		0xF0000U

/* floating pointer length + maximum length of configuration table */
#define	MPTABLE_MAX_LENGTH	(65536 + 16)U

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
#define MPEP_SIG_FAMILY		6U	/* XXX cwpdm should supply this */
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

static uint8_t mpt_compute_checksum(void *base, size_t len)
{
	uint8_t	*bytes;
	uint8_t	sum;

	for (bytes = base, sum = 0U; len > 0U; len--) {
		sum += *bytes;
		bytes++;
	}

	return (256U - sum);
}

static void mpt_build_mpfp(struct mpfps *mpfp, uint64_t gpa)
{

	memset(mpfp, 0U, sizeof(*mpfp));
	memcpy_s(mpfp->signature, 4U, MPFP_SIG, 4U);
	mpfp->pap = gpa + sizeof(*mpfp);
	mpfp->length = 1U;
	mpfp->spec_rev = MP_SPECREV;
	mpfp->checksum = mpt_compute_checksum(mpfp, sizeof(*mpfp));
}

static void mpt_build_mpch(struct mpcth * mpch)
{

	memset(mpch, 0U, sizeof(*mpch));
	memcpy_s(mpch->signature, 4U, MPCH_SIG, 4U);
	mpch->spec_rev = MP_SPECREV;
	memcpy_s(mpch->oem_id, MPCH_OEMID_LEN, MPCH_OEMID, MPCH_OEMID_LEN);
	memcpy_s(mpch->product_id, MPCH_PRODID_LEN, MPCH_PRODID,
		 MPCH_PRODID_LEN);
	mpch->apic_address = LAPIC_BASE;
}

static void mpt_build_proc_entries(struct proc_entry * mpep, int ncpu,
				   struct vm *vm)
{
	uint16_t i;
	struct vcpu *vcpu = NULL;

	for (i = 0U; i < ncpu; i++) {
		memset(mpep, 0U, sizeof(*mpep));
		mpep->type = MPCT_ENTRY_PROCESSOR;
		vcpu = vcpu_from_vid(vm, i);
		mpep->apic_id = per_cpu(lapic_id, vcpu->pcpu_id);
		mpep->apic_version = LAPIC_VERSION;
		mpep->cpu_flags = PROCENTRY_FLAG_EN;
		if (i == 0U)
			mpep->cpu_flags |= PROCENTRY_FLAG_BP;
		mpep->cpu_signature = MPEP_SIG;
		mpep->feature_flags = MPEP_FEATURES;
		mpep++;
	}
}

static void mpt_build_localint_entries(struct int_entry *mpie)
{

	/* Hardcode LINT0 as ExtINT on all CPUs. */
	memset(mpie, 0U, sizeof(*mpie));
	mpie->type = MPCT_ENTRY_LOCAL_INT;
	mpie->int_type = INTENTRY_TYPE_EXTINT;
	mpie->int_flags = INTENTRY_FLAGS_POLARITY_CONFORM |
	    INTENTRY_FLAGS_TRIGGER_CONFORM;
	mpie->dst_apic_id = 0xFFU;
	mpie->dst_apic_int = 0U;
	mpie++;

	/* Hardcode LINT1 as NMI on all CPUs. */
	memset(mpie, 0U, sizeof(*mpie));
	mpie->type = MPCT_ENTRY_LOCAL_INT;
	mpie->int_type = INTENTRY_TYPE_NMI;
	mpie->int_flags = INTENTRY_FLAGS_POLARITY_CONFORM |
	    INTENTRY_FLAGS_TRIGGER_CONFORM;
	mpie->dst_apic_id = 0xFFU;
	mpie->dst_apic_int = 1U;
}

static void mpt_build_bus_entries(struct bus_entry *mpeb)
{

	memset(mpeb, 0U, sizeof(*mpeb));
	mpeb->type = MPCT_ENTRY_BUS;
	mpeb->bus_id = 0U;
	memcpy_s(mpeb->bus_type, MPE_BUSNAME_LEN, MPE_BUSNAME_PCI,
		 MPE_BUSNAME_LEN);
	mpeb++;

	memset(mpeb, 0U, sizeof(*mpeb));
	mpeb->type = MPCT_ENTRY_BUS;
	mpeb->bus_id = 1U;
	memcpy_s(mpeb->bus_type, MPE_BUSNAME_LEN, MPE_BUSNAME_ISA,
		 MPE_BUSNAME_LEN);
}

int mptable_build(struct vm *vm, uint16_t ncpu)
{
	struct mpcth		*mpch;
	struct bus_entry	*mpeb;
	struct proc_entry	*mpep;
	struct mpfps		*mpfp;
	struct int_entry	*mpie;
	char			*curraddr;
	char			*startaddr;

	startaddr = GPA2HVA(vm, MPTABLE_BASE);

	ASSERT(startaddr, "mptable base is mapped to NULL");

	curraddr = startaddr;
	mpfp = (struct mpfps *)curraddr;
	mpt_build_mpfp(mpfp, MPTABLE_BASE);
	curraddr += sizeof(*mpfp);

	mpch = (struct mpcth *)curraddr;
	mpt_build_mpch(mpch);
	curraddr += sizeof(*mpch);

	mpep = (struct proc_entry *)curraddr;
	mpt_build_proc_entries(mpep, ncpu, vm);
	curraddr += sizeof(*mpep) * ncpu;
	mpch->entry_count += ncpu;

	mpeb = (struct bus_entry *) curraddr;
	mpt_build_bus_entries(mpeb);
	curraddr += sizeof(*mpeb) * MPE_NUM_BUSES;
	mpch->entry_count += MPE_NUM_BUSES;

	mpie = (struct int_entry *)curraddr;
	mpt_build_localint_entries(mpie);
	curraddr += sizeof(*mpie) * MPEII_NUM_LOCAL_IRQ;
	mpch->entry_count += MPEII_NUM_LOCAL_IRQ;

	mpch->base_table_length = curraddr - (char *)mpch;
	mpch->checksum = mpt_compute_checksum(mpch, mpch->base_table_length);

	return 0U;
}
