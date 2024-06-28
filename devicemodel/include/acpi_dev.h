/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef _ACPI_DEV_H_
#define _ACPI_DEV_H_

#include "hsm_ioctl_defs.h"
#include "mmio_dev.h"

struct acpi_dev {
	char name[8];
	char *hid;
	char *cid;
	char *uid;
	char *dsdt;
	struct acrn_acpidev dev;
};

struct acpi_dev_ops {
	char hid[9];
	char modalias[32];
	int (*match)(char *);
	int (*create)(char *, struct acpi_dev *);
	int (*init)(struct vmctx *, struct acrn_acpidev *);
	void (*deinit)(struct vmctx *, struct acrn_acpidev *);
	void (*write_dsdt)(struct vmctx *, struct acpi_dev *);
	void (*write_madt)(struct vmctx *, FILE *fp, struct acpi_dev *);
	/* TODO: We may add more fields when we support other ACPI dev pt */
};

SET_DECLARE(acpi_dev_ops_set, struct acpi_dev_ops);
#define DEFINE_ACPI_DEV(x) DATA_SET(acpi_dev_ops_set, x);

struct acpi_dev *get_acpidev(char *hid, char *uid);
int get_more_acpi_dev_info(char *hid, uint32_t instance, struct acpi_dev_ops *ops);
void acpi_dev_write_dsdt(struct vmctx *ctx);
void acpi_dev_write_madt(FILE *fp, struct vmctx *ctx);

int create_pt_acpidev(char *arg);

int init_acpi_devs(struct vmctx *ctx);
void deinit_acpi_devs(struct vmctx *ctx);

int init_pt_acpidev(struct vmctx *ctx, struct acrn_acpidev *dev);
void deinit_pt_acpidev(struct vmctx *ctx, struct acrn_acpidev *dev);

#endif /* _ACPI_DEV_H_ */