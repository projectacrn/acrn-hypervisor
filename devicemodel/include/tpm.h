/*
 * Copyright (C) 2018 Intel Corporation
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TPM_H_
#define _TPM_H_

#define TPM_CRB_MMIO_ADDR 0xFED40000UL
#define TPM_CRB_MMIO_SIZE 0x5000U

/* TPM CRB registers */
enum {
	CRB_REGS_LOC_STATE       = TPM_CRB_MMIO_ADDR + 0x00,
	CRB_REGS_RESERVED0       = TPM_CRB_MMIO_ADDR + 0x04,
	CRB_REGS_LOC_CTRL        = TPM_CRB_MMIO_ADDR + 0x08,
	CRB_REGS_LOC_STS         = TPM_CRB_MMIO_ADDR + 0x0C,
	CRB_REGS_RESERVED1       = TPM_CRB_MMIO_ADDR + 0x10,
	CRB_REGS_INTF_ID_LO      = TPM_CRB_MMIO_ADDR + 0x30,
	CRB_REGS_INTF_ID_HI      = TPM_CRB_MMIO_ADDR + 0x34,
	CRB_REGS_CTRL_EXT_LO     = TPM_CRB_MMIO_ADDR + 0x38,
	CRB_REGS_CTRL_EXT_HI     = TPM_CRB_MMIO_ADDR + 0x3C,
	CRB_REGS_CTRL_REQ        = TPM_CRB_MMIO_ADDR + 0x40,
	CRB_REGS_CTRL_STS        = TPM_CRB_MMIO_ADDR + 0x44,
	CRB_REGS_CTRL_CANCEL     = TPM_CRB_MMIO_ADDR + 0x48,
	CRB_REGS_CTRL_START      = TPM_CRB_MMIO_ADDR + 0x4C,
	CRB_REGS_CTRL_INT_ENABLE = TPM_CRB_MMIO_ADDR + 0x50,
	CRB_REGS_CTRL_INT_STS    = TPM_CRB_MMIO_ADDR + 0x54,
	CRB_REGS_CTRL_CMD_SIZE   = TPM_CRB_MMIO_ADDR + 0x58,
	CRB_REGS_CTRL_CMD_PA_LO  = TPM_CRB_MMIO_ADDR + 0x5C,
	CRB_REGS_CTRL_CMD_PA_HI  = TPM_CRB_MMIO_ADDR + 0x60,
	CRB_REGS_CTRL_RSP_SIZE   = TPM_CRB_MMIO_ADDR + 0x64,
	CRB_REGS_CTRL_RSP_PA     = TPM_CRB_MMIO_ADDR + 0x68,
	CRB_DATA_BUFFER          = TPM_CRB_MMIO_ADDR + 0x80
};

#define TPM_CRB_REG_SIZE ((CRB_DATA_BUFFER) - (TPM_CRB_MMIO_ADDR))
#define TPM_CRB_DATA_BUFFER_SIZE ((TPM_CRB_MMIO_SIZE) - (TPM_CRB_REG_SIZE))

/* APIs by tpm.c */
/* Initialize Virtual TPM2 */
void init_vtpm2(struct vmctx *ctx);

/* Deinitialize Virtual TPM2 */
void deinit_vtpm2(struct vmctx *ctx);

/* Parse Virtual TPM option from command line */
int acrn_parse_vtpm2(char *arg);

#endif
