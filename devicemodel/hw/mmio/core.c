/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdbool.h>

#include "dm.h"
#include "vmmapi.h"
#include "acpi.h"
#include "inout.h"
#include "mem.h"
#include "log.h"


struct mmio_dev {
	char name[16];
};

#define MAX_MMIO_DEV_NUM	2

static struct mmio_dev mmio_devs[MAX_MMIO_DEV_NUM];

struct mmio_dev_ops {
	char *name;
	uint64_t base_gpa;
	uint64_t base_hpa;
	uint64_t size;
	int (*init)(struct vmctx *, struct acrn_mmiodev *);
	void (*deinit)(struct vmctx *, struct acrn_mmiodev *);
};


SET_DECLARE(mmio_dev_ops_set, struct mmio_dev_ops);
#define DEFINE_MMIO_DEV(x)	DATA_SET(mmio_dev_ops_set, x)

struct mmio_dev_ops pt_mmiodev;

int parse_pt_acpidev(char *opt)
{
	int err = -EINVAL;
	if (strncmp(opt, "MSFT0101", 8) == 0) {
		strncpy(mmio_devs[0].name, "MSFT0101", 8);
		pt_tpm2 = true;
		err = 0;
	}

	return err;
}

int parse_pt_mmiodev(char *opt)
{

	int err = -EINVAL;
	uint64_t base_hpa, size;
	char *cp;

	if((!dm_strtoul(opt, &cp, 16, &base_hpa) && *cp == ',') &&
		(!dm_strtoul(cp + 1, &cp, 16, &size))) {
		printf("%s pt mmiodev base: 0x%lx, size: 0x%lx\n", __func__, base_hpa, size);
		strncpy(mmio_devs[1].name, pt_mmiodev.name, 8);
		pt_mmiodev.base_hpa = base_hpa;
		pt_mmiodev.size = size;
	} else {
		printf("%s, %s invalid, please check!\n", __func__, opt);
	}

	return err;
}

static struct mmio_dev_ops *mmio_dev_finddev(char *name)
{
	struct mmio_dev_ops **mdpp, *mdp;

	SET_FOREACH(mdpp, mmio_dev_ops_set) {
		mdp = *mdpp;
		if (!strcmp(mdp->name, name))
			return mdp;
	}

	return NULL;
}

int init_mmio_dev(struct vmctx *ctx, struct mmio_dev_ops *ops)
{
	struct acrn_mmiodev mmiodev = {
		.base_gpa = ops->base_gpa,
		.base_hpa = ops->base_hpa,
		.size = ops->size,
	};

	return ops->init(ctx, &mmiodev);
}

void deinit_mmio_dev(struct vmctx *ctx, struct mmio_dev_ops *ops)
{
	struct acrn_mmiodev mmiodev = {
		.base_gpa = ops->base_gpa,
		.base_hpa = ops->base_hpa,
		.size = ops->size,
	};

	ops->deinit(ctx, &mmiodev);
}

int init_mmio_devs(struct vmctx *ctx)
{
	int i, err = 0;
	struct mmio_dev_ops *ops;

	for (i = 0; i < MAX_MMIO_DEV_NUM; i++) {
		ops = mmio_dev_finddev(mmio_devs[i].name);	
		if (ops != NULL)
			err = init_mmio_dev(ctx, ops);

		if (err != 0)
			goto init_mmio_devs_fail;
	}

	return 0;

init_mmio_devs_fail:
	for (; i>=0; i--) {
		ops = mmio_dev_finddev(mmio_devs[i].name);	
		if (ops != NULL)
			deinit_mmio_dev(ctx, ops);
	}

	return err;
}

void deinit_mmio_devs(struct vmctx *ctx)
{
	int i;
	struct mmio_dev_ops *ops;

	for (i = 0; i < MAX_MMIO_DEV_NUM; i++) {
		ops = mmio_dev_finddev(mmio_devs[i].name);	
		if (ops != NULL)
			deinit_mmio_dev(ctx, ops);

	}
}

static int init_pt_mmiodev(struct vmctx *ctx, struct acrn_mmiodev *dev)
{
	return vm_assign_mmiodev(ctx, dev);
}

static void deinit_pt_mmiodev(struct vmctx *ctx, struct acrn_mmiodev *dev)
{
	vm_deassign_mmiodev(ctx, dev);
}

struct mmio_dev_ops tpm2 = {
	.name		= "MSFT0101",
	/* ToDo: we may allocate the gpa MMIO resource in a reserved MMIO region
	 * rether than hard-coded here.
	 */
	.base_gpa	= 0xFED40000UL,
	.base_hpa	= 0xFED40000UL,
	.size		= 0x00005000UL,
	.init		= init_pt_mmiodev,
	.deinit		= deinit_pt_mmiodev,
};
DEFINE_MMIO_DEV(tpm2);

struct mmio_dev_ops pt_mmiodev = {
	.name		= "MMIODEV",
	/* ToDo: we may allocate the gpa MMIO resource in a reserved MMIO region
	 * rether than hard-coded here.
	 */
	.base_gpa	= 0xF0000000UL,
	.init		= init_pt_mmiodev,
	.deinit		= deinit_pt_mmiodev,
};
DEFINE_MMIO_DEV(pt_mmiodev);
