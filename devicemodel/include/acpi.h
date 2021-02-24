/*-
 * Copyright (c) 2012 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _ACPI_H_
#define _ACPI_H_

#define	SCI_INT			9

#define	SMI_CMD			0xb2
#define	ACPI_ENABLE		0xa0
#define	ACPI_DISABLE		0xa1

#define	PM1A_EVT_ADDR		0x400

#define	IO_PMTMR		0x0	/* PM Timer is disabled in ACPI */

struct acpi_table_hdr {
	/* ASCII table signature */
	char                    signature[4];
	/* Length of table in bytes, including this header */
	uint32_t                length;
	/* ACPI Specification minor version number */
	uint8_t                 revision;
	/* To make sum of entire table == 0 */
	uint8_t                 checksum;
	/* ASCII OEM identification */
	char                    oem_id[6];
	/* ASCII OEM table identification */
	char                    oem_table_id[8];
	/* OEM revision number */
	uint32_t                oem_revision;
	/* ASCII ASL compiler vendor ID */
	char                    asl_compiler_id[4];
	/* ASL compiler version */
	uint32_t                asl_compiler_revision;
} __attribute__((packed));

/* All dynamic table entry no. */
#define NHLT_ENTRY_NO		8

void acpi_table_enable(int num);
uint32_t get_acpi_base(void);
uint32_t get_acpi_table_length(void);

struct vmctx;

int	acpi_build(struct vmctx *ctx, int ncpu);
void	dsdt_line(const char *fmt, ...);
void	dsdt_fixed_ioport(uint16_t iobase, uint16_t length);
void	dsdt_fixed_irq(uint8_t irq);
void	dsdt_fixed_mem32(uint32_t base, uint32_t length);
void	dsdt_indent(int levels);
void	dsdt_unindent(int levels);
void	sci_init(struct vmctx *ctx);
void	pm_write_dsdt(struct vmctx *ctx, int ncpu);
void	pm_backto_wakeup(struct vmctx *ctx);
void	inject_power_button_event(struct vmctx *ctx);
void	power_button_init(struct vmctx *ctx);
void	power_button_deinit(struct vmctx *ctx);

#endif /* _ACPI_H_ */
