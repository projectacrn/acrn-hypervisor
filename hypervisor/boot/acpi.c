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
#include "acpi_priv.h"
#include "acpi.h"
#include <vm0_boot.h>

#define ACPI_SIG_RSDP             "RSD PTR " /* Root System Description Ptr */
#define ACPI_OEM_ID_SIZE           6
#define ACPI_SIG_MADT             "APIC" /* Multiple APIC Description Table */
#define ACPI_SIG_FADT             "FACP" /* Fixed ACPI Description Table */
#define ACPI_SIG_DMAR             "DMAR"
#define RSDP_CHECKSUM_LENGTH       20
#define ACPI_NAME_SIZE             4U
#define ACPI_MADT_TYPE_LOCAL_APIC  0U
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
				strnlen_s(ACPI_SIG_RSDP, 8U)) == 0) {
			cp = (uint8_t *)rsdp;
			sum = 0U;
			for (idx = 0; idx < RSDP_CHECKSUM_LENGTH; idx++) {
				sum += *(cp + idx);
			}

			if (sum != 0U) {
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
	addr = (uint16_t *)hpa2hva(0x40eUL);

	rsdp = biosacpi_search_rsdp((char *)hpa2hva((uint64_t)(*addr) << 4U),
									0x400);
	if (rsdp != NULL) {
		return rsdp;
	}

	/* Check the upper memory BIOS space, 0xe0000 - 0xfffff. */
	rsdp = biosacpi_search_rsdp((char *)hpa2hva(0xe0000UL), 0x20000);
	if (rsdp != NULL) {
		return rsdp;
	}

	return rsdp;
}

static bool
probe_table(uint64_t address, const char *sig)
{
	void *va =  hpa2hva(address);
	struct acpi_table_header *table = (struct acpi_table_header *)va;
	bool ret;

	if (strncmp(table->signature, sig, ACPI_NAME_SIZE) != 0) {
	        ret = false;
	} else {
		ret = true;
	}

	return ret;
}

static void *get_acpi_tbl(const char *sig)
{
	struct acpi_table_rsdp *rsdp;
	struct acpi_table_rsdt *rsdt;
	struct acpi_table_xsdt *xsdt;
	uint64_t addr = 0UL;
	uint32_t i, count;

	rsdp = (struct acpi_table_rsdp *)global_rsdp;

	if ((rsdp->revision >= 2U) && (rsdp->xsdt_physical_address != 0UL)) {
		/*
		 * AcpiOsGetRootPointer only verifies the checksum for
		 * the version 1.0 portion of the RSDP.  Version 2.0 has
		 * an additional checksum that we verify first.
		 */
		xsdt = (struct acpi_table_xsdt *)
					hpa2hva(rsdp->xsdt_physical_address);
		count = (xsdt->header.length -
				sizeof(struct acpi_table_header)) /
		    sizeof(uint64_t);

		for (i = 0U; i < count; i++) {
			if (probe_table(xsdt->table_offset_entry[i], sig)) {
				addr = xsdt->table_offset_entry[i];
				break;
			}
		}
	} else {
		/* Root table is an RSDT (32-bit physical addresses) */
		rsdt = (struct acpi_table_rsdt *)
				hpa2hva((uint64_t)rsdp->rsdt_physical_address);
		count = (rsdt->header.length -
				sizeof(struct acpi_table_header)) /
			sizeof(uint32_t);

		for (i = 0U; i < count; i++) {
			if (probe_table(rsdt->table_offset_entry[i], sig)) {
				addr = rsdt->table_offset_entry[i];
				break;
			}
		}
	}

	return hpa2hva(addr);
}

/* TODO: As ACRN supports only x2APIC mode, we need to
 * check upon using x2APIC APIC entries (Type 9) in MADT instead
 * of Type 0
 */
static uint16_t
local_parse_madt(void *madt, uint32_t lapic_id_array[CONFIG_MAX_PCPU_NUM])
{
	uint16_t pcpu_num = 0U;
	struct acpi_madt_local_apic *processor;
	struct acpi_table_madt *madt_ptr;
	void *first, *end, *iterator;
	struct acpi_subtable_header *entry;

	madt_ptr = (struct acpi_table_madt *)madt;

	first = madt_ptr + 1;
	end = (char *)madt_ptr + madt_ptr->header.length;

	for (iterator = first; (iterator) < (end); iterator += entry->length) {
		entry = (struct acpi_subtable_header *)iterator;
		if (entry->length < sizeof(struct acpi_subtable_header)) {
			break;
		}

		if (entry->type == ACPI_MADT_TYPE_LOCAL_APIC) {
			processor = (struct acpi_madt_local_apic *)iterator;
			if ((processor->lapic_flags & ACPI_MADT_ENABLED) != 0U) {
				if (pcpu_num < CONFIG_MAX_PCPU_NUM) {
					lapic_id_array[pcpu_num] = processor->id;
				}
				pcpu_num++;
			}
		}
	}

	return pcpu_num;
}

/* The lapic_id info gotten from madt will be returned in lapic_id_array */
uint16_t parse_madt(uint32_t lapic_id_array[CONFIG_MAX_PCPU_NUM])
{
	void *madt;

	global_rsdp = get_rsdp();
	ASSERT(global_rsdp != NULL, "fail to get rsdp");

	madt = get_acpi_tbl(ACPI_SIG_MADT);
	ASSERT(madt != NULL, "fail to get madt");

	return local_parse_madt(madt, lapic_id_array);
}

void *get_dmar_table(void)
{
	return get_acpi_tbl(ACPI_SIG_DMAR);
}

#ifndef CONFIG_CONSTANT_ACPI
/* Per ACPI spec:
 * There are two fundamental types of ACPI tables:
 *
 * Tables that contain AML code produced from the ACPI Source Language (ASL).
 * These include the DSDT, any SSDTs, and sometimes OEM-specific tables (OEMx).
 *
 * Tables that contain simple data and no AML byte code. These types of tables
 * are known as ACPI Data Tables. They include tables such as the FADT, MADT,
 * ECDT, SRAT, etc. -essentially any table other than a DSDT or SSDT.
 *
 * The second type of table, the ACPI Data Table, could be parsed here.
 *
 * When ACRN go production, the platform ACPI data should be fixed. The
 * Kconfig of CONSTANT_ACPI will be set to yes, then this code is not needed.
 */

#define ACPI_SIG_FACS		0x53434146U	/* "FACS" */

/* FACP field offsets */
#define OFFSET_FACS_ADDR	36U
#define OFFSET_FACS_X_ADDR	132U

/* FACS field offsets */
#define OFFSET_FACS_SIGNATURE	0U
#define OFFSET_FACS_LENGTH	4U
#define OFFSET_WAKE_VECTOR_32	12U
#define OFFSET_WAKE_VECTOR_64	24U

/* get a dword value from given table and its offset */
static inline uint32_t get_acpi_dt_dword(const uint8_t *dt_addr, uint32_t dt_offset)
{
	return *(uint32_t *)(dt_addr + dt_offset);
}

/* get a qword value from given table and its offset */
static inline uint64_t get_acpi_dt_qword(const uint8_t *dt_addr, uint32_t dt_offset)
{
	return *(uint64_t *)(dt_addr + dt_offset);
}

static void *get_facs_table(void)
{
	uint8_t *facp_addr, *facs_addr, *facs_x_addr;
	uint32_t signature, length;

	facp_addr = (uint8_t *)get_acpi_tbl(ACPI_SIG_FADT);

	if (facp_addr == NULL) {
		return NULL;
	}

	facs_addr = (uint8_t *)(uint64_t)get_acpi_dt_dword(facp_addr,
						OFFSET_FACS_ADDR);

	facs_x_addr = (uint8_t *)get_acpi_dt_qword(facp_addr,
						OFFSET_FACS_X_ADDR);

	if (facs_x_addr != NULL) {
		facs_addr = facs_x_addr;
	}

	if (facs_addr == NULL) {
		return NULL;
	}

	signature = get_acpi_dt_dword(facs_addr, OFFSET_FACS_SIGNATURE);

	if (signature != ACPI_SIG_FACS) {
		return NULL;
	}

	length = get_acpi_dt_dword(facs_addr, OFFSET_FACS_LENGTH);

	if (length < 64U) {
		return NULL;
	}

	return facs_addr;
}

/* put all ACPI fix up code here */
void acpi_fixup(void)
{
	void *facs_addr = get_facs_table();

	if (facs_addr != NULL) {
		host_pm_s_state.wake_vector_32 = (uint32_t *)(facs_addr + OFFSET_WAKE_VECTOR_32);
		host_pm_s_state.wake_vector_64 = (uint64_t *)(facs_addr + OFFSET_WAKE_VECTOR_64);
	}
}
#endif
