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
#include <host_pm.h>
#include <acrn_common.h>
#include <vm_reset.h>

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
 * When ACRN go FuSa, the platform ACPI data should be fixed and this file is not needed.
 */

#define ACPI_SIG_FACS		0x53434146U	/* "FACS" */
#define ACPI_SIG_FADT             "FACP" 	/* Fixed ACPI Description Table */

/* FACP field offsets */
#define OFFSET_FACS_ADDR	36U
#define OFFSET_RESET_REGISTER	116U
#define OFFSET_RESET_VALUE	128U
#define OFFSET_FACS_X_ADDR	132U
#define OFFSET_PM1A_EVT         148U
#define OFFSET_PM1A_CNT         172U

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

struct packed_gas {
	uint8_t 	space_id;
	uint8_t 	bit_width;
	uint8_t 	bit_offset;
	uint8_t 	access_size;
	uint64_t	address;
} __attribute__((packed));

/* get a GAS struct from given table and its offset.
 * ACPI table stores packed gas, but it is not guaranteed that
 * struct acpi_generic_address is packed, so do not use memcpy in function.
 * @pre dt_addr != NULL && gas != NULL
 */
static inline void get_acpi_dt_gas(const uint8_t *dt_addr, uint32_t dt_offset, struct acpi_generic_address *gas)
{
	struct packed_gas *dt_gas = (struct packed_gas *)(dt_addr + dt_offset);

	gas->space_id = dt_gas->space_id;
	gas->bit_width = dt_gas->bit_width;
	gas->bit_offset = dt_gas->bit_offset;
	gas->access_size = dt_gas->access_size;
	gas->address = dt_gas->address;
}

/* @pre facp_addr != NULL */
static void *get_facs_table(const uint8_t *facp_addr)
{
	uint8_t *facs_addr, *facs_x_addr;
	uint32_t signature, length;

	facs_addr = (uint8_t *)(uint64_t)get_acpi_dt_dword(facp_addr, OFFSET_FACS_ADDR);

	facs_x_addr = (uint8_t *)get_acpi_dt_qword(facp_addr, OFFSET_FACS_X_ADDR);

	if (facs_x_addr != NULL) {
		facs_addr = facs_x_addr;
	}

	if (facs_addr != NULL) {
		signature = get_acpi_dt_dword(facs_addr, OFFSET_FACS_SIGNATURE);

		if (signature != ACPI_SIG_FACS) {
			facs_addr = NULL;
		} else {
			length = get_acpi_dt_dword(facs_addr, OFFSET_FACS_LENGTH);

			if (length < 64U) {
				facs_addr = NULL;
			}
		}
	}
	return (void *)facs_addr;
}

/* put all ACPI fix up code here */
void acpi_fixup(void)
{
	uint8_t *facp_addr, *facs_addr;
	struct acpi_generic_address pm1a_cnt, pm1a_evt;
	struct pm_s_state_data *sx_data = get_host_sstate_data();

	facp_addr = (uint8_t *)get_acpi_tbl(ACPI_SIG_FADT);

	if (facp_addr != NULL) {
		get_acpi_dt_gas(facp_addr, OFFSET_PM1A_EVT, &pm1a_evt);
		get_acpi_dt_gas(facp_addr, OFFSET_PM1A_CNT, &pm1a_cnt);
		(void)memcpy_s((void *)&sx_data->pm1a_evt, sizeof(struct acpi_generic_address),
				(const void *)&pm1a_evt, sizeof(struct acpi_generic_address));
		(void)memcpy_s((void *)&sx_data->pm1a_cnt, sizeof(struct acpi_generic_address),
				(const void *)&pm1a_cnt, sizeof(struct acpi_generic_address));

		facs_addr = (uint8_t *)get_facs_table(facp_addr);
		if (facs_addr != NULL) {
			sx_data->wake_vector_32 = (uint32_t *)(facs_addr + OFFSET_WAKE_VECTOR_32);
			sx_data->wake_vector_64 = (uint64_t *)(facs_addr + OFFSET_WAKE_VECTOR_64);
		}

		const struct acpi_table_header *table = (const struct acpi_table_header *)facp_addr;

		if (table->revision >= 2U) {
			struct acpi_reset_reg *rr_data = get_host_reset_reg_data();

			get_acpi_dt_gas(facp_addr, OFFSET_RESET_REGISTER, &(rr_data->reg));
			rr_data->val = *(facp_addr + OFFSET_RESET_VALUE);
		}
	}
}
