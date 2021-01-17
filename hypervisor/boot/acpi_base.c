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
#include <types.h>
#include <rtl.h>
#include "acpi.h"
#include <pgtable.h>
#include <ioapic.h>
#include <logmsg.h>
#include <acrn_common.h>
#include <util.h>
#include <e820.h>
#include <multiboot.h>

static struct acpi_table_rsdp *acpi_rsdp;

static struct acpi_table_rsdp *found_rsdp(char *base, uint64_t length)
{
	struct acpi_table_rsdp *rsdp, *ret = NULL;
	uint64_t ofs;

	/* search on 16-byte boundaries */
	for (ofs = 0UL; ofs < length; ofs += 16UL) {
		rsdp = (struct acpi_table_rsdp *)(base + ofs);

		/* compare signature, validate checksum */
		if ((strncmp(rsdp->signature, ACPI_SIG_RSDP, strnlen_s(ACPI_SIG_RSDP, sizeof(rsdp->signature))) == 0)
			&& (calculate_sum8(rsdp, ACPI_RSDP_CHECKSUM_LENGTH) == 0U)) {
			ret = rsdp;
			break;
		}
	}

	return ret;
}

/* RSDP parsed from BIOS region should exist.
 * If it is NULL, the hypervisor can't be booted
 */
static struct acpi_table_rsdp *get_rsdp(void)
{
	struct acpi_table_rsdp *rsdp = NULL;

	/* If acpi_rsdp is already parsed, it will be returned directly */
	if (acpi_rsdp != NULL) {
		rsdp = acpi_rsdp;
	} else {
		rsdp = (struct acpi_table_rsdp *)(get_multiboot_info()->mi_acpi_rsdp_va);
		if (rsdp == NULL) {
			uint16_t *addr;

			/* EBDA is addressed by the 16 bit pointer at 0x40E */
			addr = (uint16_t *)hpa2hva(0x40eUL);

			rsdp = found_rsdp((char *)hpa2hva((uint64_t)(*addr) << 4U), 0x400UL);
		}
		if (rsdp == NULL) {
			/* Check the upper memory BIOS space, 0xe0000 - 0xfffff. */
			rsdp = found_rsdp((char *)hpa2hva(0xe0000UL), 0x20000UL);
		}

		if (rsdp == NULL) {
			/* Check ACPI RECLAIM region, there might be multiple ACPI reclaimable regions. */
			uint32_t i;
			const struct e820_entry *entry = get_e820_entry();
			uint32_t entries_count = get_e820_entries_count();

			for (i = 0U; i < entries_count; i++) {
				if (entry[i].type == E820_TYPE_ACPI_RECLAIM) {
					rsdp = found_rsdp((char *)hpa2hva(entry[i].baseaddr), entry[i].length);
					if (rsdp != NULL) {
						break;
					}
				}
			}
		}

		if (rsdp == NULL) {
			panic("No RSDP is found");
		}

		/* After RSDP is parsed, it will be assigned to acpi_rsdp */
		acpi_rsdp = rsdp;
	}


	return rsdp;
}

static bool probe_table(uint64_t address, const char *signature)
{
	void *va =  hpa2hva(address);
	struct acpi_table_header *table = (struct acpi_table_header *)va;
	bool ret;

	if (strncmp(table->signature, signature, ACPI_NAME_SIZE) != 0) {
		ret = false;
	} else {
		ret = true;
	}

	return ret;
}

void *get_acpi_tbl(const char *signature)
{
	struct acpi_table_rsdp *rsdp;
	struct acpi_table_rsdt *rsdt;
	struct acpi_table_xsdt *xsdt;
	uint64_t addr = 0UL;
	uint32_t i, count;

	/* the returned RSDP should always exist. Otherwise the hypervisor
	 * can't be booted.
	 */
	rsdp = get_rsdp();

	if ((rsdp->revision >= 2U) && (rsdp->xsdt_physical_address != 0UL)) {
		/*
		 * AcpiOsGetRootPointer only verifies the checksum for
		 * the version 1.0 portion of the RSDP.  Version 2.0 has
		 * an additional checksum that we verify first.
		 */
		xsdt = (struct acpi_table_xsdt *)hpa2hva(rsdp->xsdt_physical_address);
		count = (xsdt->header.length - sizeof(struct acpi_table_header)) / sizeof(uint64_t);

		for (i = 0U; i < count; i++) {
			if (probe_table(xsdt->table_offset_entry[i], signature)) {
				addr = xsdt->table_offset_entry[i];
				break;
			}
		}
	} else {
		/* Root table is an RSDT (32-bit physical addresses) */
		rsdt = (struct acpi_table_rsdt *)hpa2hva((uint64_t)rsdp->rsdt_physical_address);
		count = (rsdt->header.length - sizeof(struct acpi_table_header)) / sizeof(uint32_t);

		for (i = 0U; i < count; i++) {
			if (probe_table(rsdt->table_offset_entry[i], signature)) {
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
local_parse_madt(struct acpi_table_madt *madt, uint32_t lapic_id_array[MAX_PCPU_NUM])
{
	uint16_t pcpu_num = 0U;
	struct acpi_madt_local_apic *processor;
	struct acpi_table_madt *madt_ptr;
	void *first, *end, *iterator;
	struct acpi_subtable_header *entry;

	madt_ptr = madt;

	first = madt_ptr + 1;
	end = (void *)madt_ptr + madt_ptr->header.length;

	for (iterator = first; (iterator) < (end); iterator += entry->length) {
		entry = (struct acpi_subtable_header *)iterator;
		if (entry->length < sizeof(struct acpi_subtable_header)) {
			break;
		}

		if (entry->type == ACPI_MADT_TYPE_LOCAL_APIC) {
			processor = (struct acpi_madt_local_apic *)iterator;
			if ((processor->lapic_flags & ACPI_MADT_ENABLED) != 0U) {
				if (pcpu_num < MAX_PCPU_NUM) {
					lapic_id_array[pcpu_num] = processor->id;
				}
				pcpu_num++;
			}
		}
	}

	return pcpu_num;
}

/* The lapic_id info gotten from madt will be returned in lapic_id_array */
uint16_t parse_madt(uint32_t lapic_id_array[MAX_PCPU_NUM])
{
	uint16_t ret = 0U;
	struct acpi_table_rsdp *rsdp = NULL;

	rsdp = get_rsdp();
	if (rsdp != NULL) {
		struct acpi_table_madt *madt = (struct acpi_table_madt *)get_acpi_tbl(ACPI_SIG_MADT);

		if (madt != NULL) {
			ret = local_parse_madt(madt, lapic_id_array);
		}
	}

	return ret;
}

uint8_t parse_madt_ioapic(struct ioapic_info *ioapic_id_array)
{
	uint8_t ioapic_idx = 0U;
	uint64_t entry, end;
	const struct acpi_madt_ioapic *ioapic;
	const struct acpi_table_madt *madt;

	if (get_rsdp() != NULL) {
		madt = (const struct acpi_table_madt *)get_acpi_tbl(ACPI_SIG_MADT);

		if (madt != NULL) {
			end = (uint64_t)madt + madt->header.length;

			for (entry = (uint64_t)(madt + 1); entry < end; entry += ioapic->header.length) {
				ioapic = (const struct acpi_madt_ioapic *)entry;

				if (ioapic->header.type == ACPI_MADT_TYPE_IOAPIC) {
					if (ioapic_idx < CONFIG_MAX_IOAPIC_NUM) {
						ioapic_id_array[ioapic_idx].id = ioapic->id;
						ioapic_id_array[ioapic_idx].addr = ioapic->addr;
						ioapic_id_array[ioapic_idx].gsi_base = ioapic->gsi_base;
					}
					ioapic_idx++;
				}
			}
		}
	}

	return ioapic_idx;
}
