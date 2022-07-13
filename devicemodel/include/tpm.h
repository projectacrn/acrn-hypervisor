/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TPM_H_
#define _TPM_H_

#include "mmio_dev.h"
#include "acpi.h"

#define TPM_CRB_MMIO_ADDR 0xFED40000UL
#define TPM_CRB_MMIO_SIZE 0x5000U

uint32_t get_vtpm_crb_mmio_addr(void);
uint32_t get_tpm_crb_mmio_addr(void);
int basl_fwrite_tpm2(FILE *fp, struct vmctx *ctx);

struct acpi_table_tpm2 {
	struct acpi_table_hdr header;
	uint16_t platform_class;
	uint16_t reserved;
	uint64_t control_address;
	uint32_t start_method;
	uint8_t start_method_spec_para[12];
	uint32_t laml;
	uint64_t lasa;
} __attribute__((packed));

/* TPM CRB registers */
enum {
	CRB_REGS_LOC_STATE       =  0x00,
	CRB_REGS_RESERVED0       =  0x04,
	CRB_REGS_LOC_CTRL        =  0x08,
	CRB_REGS_LOC_STS         =  0x0C,
	CRB_REGS_RESERVED1       =  0x10,
	CRB_REGS_INTF_ID_LO      =  0x30,
	CRB_REGS_INTF_ID_HI      =  0x34,
	CRB_REGS_CTRL_EXT_LO     =  0x38,
	CRB_REGS_CTRL_EXT_HI     =  0x3C,
	CRB_REGS_CTRL_REQ        =  0x40,
	CRB_REGS_CTRL_STS        =  0x44,
	CRB_REGS_CTRL_CANCEL     =  0x48,
	CRB_REGS_CTRL_START      =  0x4C,
	CRB_REGS_CTRL_INT_ENABLE =  0x50,
	CRB_REGS_CTRL_INT_STS    =  0x54,
	CRB_REGS_CTRL_CMD_SIZE   =  0x58,
	CRB_REGS_CTRL_CMD_PA_LO  =  0x5C,
	CRB_REGS_CTRL_CMD_PA_HI  =  0x60,
	CRB_REGS_CTRL_RSP_SIZE   =  0x64,
	CRB_REGS_CTRL_RSP_PA     =  0x68,
	CRB_DATA_BUFFER          =  0x80
};

#define TPM_CRB_REG_SIZE (CRB_DATA_BUFFER)
#define TPM_CRB_DATA_BUFFER_SIZE ((TPM_CRB_MMIO_SIZE) - (TPM_CRB_REG_SIZE))

/* APIs by tpm.c */
/* Initialize Virtual TPM2 */
void init_vtpm2(struct vmctx *ctx);

/* Deinitialize Virtual TPM2 */
void deinit_vtpm2(struct vmctx *ctx);

/* Parse Virtual TPM option from command line */
int acrn_parse_vtpm2(char *arg);

#endif
