/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <hypervisor.h>
#include "acpi.h"
#ifdef CONFIG_EFI_STUB
#include <acrn_efi.h>
#endif

#define ACPI_SIG_RSDP             "RSD PTR " /* Root System Description Ptr */
#define ACPI_OEM_ID_SIZE           6
#define ACPI_SIG_MADT             "APIC" /* Multiple APIC Description Table */
#define ACPI_SIG_DMAR             "DMAR"
#define RSDP_CHECKSUM_LENGTH       20
#define ACPI_NAME_SIZE             4
#define ACPI_MADT_TYPE_LOCAL_APIC  0
#define ACPI_MADT_ENABLED          1U
#define ACPI_OEM_TABLE_ID_SIZE     8

struct acpi_table_rsdp {
	/* ACPI signature, contains "RSD PTR " */
	char                    signature[8];
	/* ACPI 1.0 checksum */
	uint8_t                 checksum;
	/* OEM identification */
	char                    oem_id[ACPI_OEM_ID_SIZE];
	/* Must be (0) for ACPI 1.0 or (2) for ACPI 2.0+ */
	uint8_t                 revision;
	/* 32-bit physical address of the RSDT */
	uint32_t                rsdt_physical_address;
	/* Table length in bytes, including header (ACPI 2.0+) */
	uint32_t                length;
	/* 64-bit physical address of the XSDT (ACPI 2.0+) */
	uint64_t                xsdt_physical_address;
	/* Checksum of entire table (ACPI 2.0+) */
	uint8_t                 extended_checksum;
	/* Reserved, must be zero */
	uint8_t                 reserved[3];
};

struct acpi_table_rsdt {
	/* Common ACPI table header */
	struct acpi_table_header   header;
	/* Array of pointers to ACPI tables */
	uint32_t                   table_offset_entry[1];
} __packed;

struct acpi_table_xsdt {
	/* Common ACPI table header */
	struct acpi_table_header    header;
	/* Array of pointers to ACPI tables */
	uint64_t                    table_offset_entry[1];
} __packed;

struct acpi_subtable_header {
	uint8_t                   type;
	uint8_t                   length;
};

struct acpi_table_madt {
	/* Common ACPI table header */
	struct acpi_table_header     header;
	/* Physical address of local APIC */
	uint32_t                     address;
	uint32_t                     flags;
};

struct acpi_madt_local_apic {
	struct acpi_subtable_header    header;
	/* ACPI processor id */
	uint8_t                        processor_id;
	/* Processor's local APIC id */
	uint8_t                        id;
	uint32_t                       lapic_flags;
};

static void *global_rsdp;

static struct acpi_table_rsdp*
biosacpi_search_rsdp(char *base, int length)
{
	struct acpi_table_rsdp *rsdp;
	uint8_t *cp, sum;
	int ofs, idx;

	/* search on 16-byte boundaries */
	for (ofs = 0; ofs < length; ofs += 16) {
		rsdp = (struct acpi_table_rsdp *)(base + ofs);

		/* compare signature, validate checksum */
		if (strncmp(rsdp->signature, ACPI_SIG_RSDP,
				strnlen_s(ACPI_SIG_RSDP, 8)) == 0) {
			cp = (uint8_t *)rsdp;
			sum = NULL;
			for (idx = 0; idx < RSDP_CHECKSUM_LENGTH; idx++) {
				sum += *(cp + idx);
			}

			if (sum != NULL) {
				continue;
			}

			return rsdp;
		}
	}

	return NULL;
}

static void *get_rsdp(void)
{
	struct acpi_table_rsdp *rsdp = NULL;
	uint16_t *addr;

#ifdef CONFIG_EFI_STUB
	rsdp = get_rsdp_from_uefi();
	if (rsdp) {
		return rsdp;
	}
#endif

	/* EBDA is addressed by the 16 bit pointer at 0x40E */
	addr = (uint16_t *)HPA2HVA(0x40E);

	rsdp = biosacpi_search_rsdp((char *)HPA2HVA((uint64_t)(*addr << 4)), 0x400);
	if (rsdp != NULL) {
		return rsdp;
	}

	/* Check the upper memory BIOS space, 0xe0000 - 0xfffff. */
	rsdp = biosacpi_search_rsdp((char *)HPA2HVA(0xe0000), 0x20000);
	if (rsdp != NULL) {
		return rsdp;
	}

	return rsdp;
}

static int
probe_table(uint64_t address, const char *sig)
{
	void *va =  HPA2HVA(address);
	struct acpi_table_header *table = (struct acpi_table_header *)va;

	if (strncmp(table->signature, sig, ACPI_NAME_SIZE) != 0) {
		return 0;
	}

	return 1;
}

void *get_acpi_tbl(const char *sig)
{
	struct acpi_table_rsdp *rsdp;
	struct acpi_table_rsdt *rsdt;
	struct acpi_table_xsdt *xsdt;
	uint64_t addr = 0;
	int i, count;

	rsdp = (struct acpi_table_rsdp *)global_rsdp;

	if (rsdp->revision >= 2 && (rsdp->xsdt_physical_address != 0U)) {
		/*
		 * AcpiOsGetRootPointer only verifies the checksum for
		 * the version 1.0 portion of the RSDP.  Version 2.0 has
		 * an additional checksum that we verify first.
		 */
		xsdt = (struct acpi_table_xsdt *)HPA2HVA(rsdp->xsdt_physical_address);
		count = (xsdt->header.length -
				sizeof(struct acpi_table_header)) /
		    sizeof(uint64_t);

		for (i = 0; i < count; i++) {
			if (probe_table(xsdt->table_offset_entry[i], sig) != 0) {
				addr = xsdt->table_offset_entry[i];
				break;
			}
		}
	} else {
		/* Root table is an RSDT (32-bit physical addresses) */
		rsdt = (struct acpi_table_rsdt *)
				HPA2HVA((uint64_t)rsdp->rsdt_physical_address);
		count = (rsdt->header.length -
				sizeof(struct acpi_table_header)) /
			sizeof(uint32_t);

		for (i = 0; i < count; i++) {
			if (probe_table(rsdt->table_offset_entry[i], sig) != 0) {
				addr = rsdt->table_offset_entry[i];
				break;
			}
		}
	}

	return HPA2HVA(addr);
}

static uint16_t _parse_madt(void *madt, uint8_t lapic_id_array[MAX_PCPU_NUM])
{
	uint16_t pcpu_id = 0;
	struct acpi_madt_local_apic *processor;
	struct acpi_table_madt *madt_ptr;
	void *first;
	void *end;
	struct acpi_subtable_header *entry;

	madt_ptr = (struct acpi_table_madt *)madt;

	first = madt_ptr + 1;
	end = (char *)madt_ptr + madt_ptr->header.length;

	for (entry = first; (void *)entry < end; ) {
		if (entry->length < sizeof(struct acpi_subtable_header)) {
			break;
		}

		if (entry->type == ACPI_MADT_TYPE_LOCAL_APIC) {
			processor = (struct acpi_madt_local_apic *)entry;
			if ((processor->lapic_flags & ACPI_MADT_ENABLED) != 0U) {
				lapic_id_array[pcpu_id] = processor->id;
				pcpu_id++;
				/*
				 * set the pcpu_num as 0U to indicate the
				 * potential overflow
				 */
				if (pcpu_id >= MAX_PCPU_NUM) {
					pcpu_id = 0U;
					break;
				}
			}
		}

		entry = (struct acpi_subtable_header *)
				(((uint64_t)entry) + entry->length);
	}

	return pcpu_id;
}

/* The lapic_id info gotten from madt will be returned in lapic_id_array */
uint16_t parse_madt(uint8_t lapic_id_array[MAX_PCPU_NUM])
{
	void *madt;

	global_rsdp = get_rsdp();
	ASSERT(global_rsdp != NULL, "fail to get rsdp");

	madt = get_acpi_tbl(ACPI_SIG_MADT);
	ASSERT(madt != NULL, "fail to get madt");

	return _parse_madt(madt, lapic_id_array);
}

void *get_dmar_table(void)
{
	return get_acpi_tbl(ACPI_SIG_DMAR);
}
