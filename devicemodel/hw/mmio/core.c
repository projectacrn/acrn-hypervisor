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
#include <sys/user.h>

#include "dm.h"
#include "vmmapi.h"
#include "acpi.h"
#include "inout.h"
#include "mem.h"
#include "log.h"
#include "mmio_dev.h"


struct mmio_dev {
	char name[16];
	struct acrn_mmiodev dev;
};

#define MAX_MMIO_DEV_NUM	2

static struct mmio_dev mmio_devs[MAX_MMIO_DEV_NUM];
static uint32_t mmio_dev_idx = 0U;

struct mmio_dev_ops {
	char *name;
	int (*init)(struct vmctx *, struct acrn_mmiodev *);
	void (*deinit)(struct vmctx *, struct acrn_mmiodev *);
};


SET_DECLARE(mmio_dev_ops_set, struct mmio_dev_ops);
#define DEFINE_MMIO_DEV(x)	DATA_SET(mmio_dev_ops_set, x)

struct mmio_dev_ops pt_mmiodev;

static uint32_t mmio_dev_base = MMIO_DEV_BASE;

int mmio_dev_alloc_gpa_resource32(uint32_t *addr, uint32_t size_in)
{
	uint32_t base, size;

	size = roundup2(size_in, PAGE_SIZE);
	base = roundup2(mmio_dev_base, size);
	if (base + size <= MMIO_DEV_LIMIT) {
		*addr = base;
		mmio_dev_base = base + size;
		return 0;
	} else {
		return -1;
	}
}

int parse_pt_acpidev(char *opt)
{
	int err = 0;

	if (mmio_dev_idx >= MAX_MMIO_DEV_NUM) {
		pr_err("MMIO dev number exceed MAX_MMIO_DEV_NUM!!!\n");
		return -EINVAL;
	}
	/* TODO: support acpi dev framework, remove these TPM hard code */
	if (strncmp(opt, "MSFT0101", 8) == 0) {
		strncpy(mmio_devs[mmio_dev_idx].name, "MSFT0101", 8);
		/* TODO: We would parse the /proc/iomem to get the corresponding resources */
		mmio_devs[mmio_dev_idx].dev.service_vm_pa = 0xFED40000UL;
		/* FIXME: The 0xFED40000 doesn't conflict with other mmio or system memory so far.
		 * This need to be fixed by redesign the mmio_dev_alloc_gpa_resource32().
		 */
		mmio_devs[mmio_dev_idx].dev.user_vm_pa = 0xFED40000UL;
		mmio_devs[mmio_dev_idx].dev.size = 0x00005000UL;
		mmio_dev_idx++;
		pt_tpm2 = true;
	}

	return err;
}

int parse_pt_mmiodev(char *opt)
{

	int err = 0;
	uint64_t base_hpa, size;
	char *cp;

	if (mmio_dev_idx >= MAX_MMIO_DEV_NUM) {
		pr_err("MMIO dev number exceed MAX_MMIO_DEV_NUM!!!\n");
		return -EINVAL;
	}

	if((!dm_strtoul(opt, &cp, 16, &base_hpa) && *cp == ',') &&
		(!dm_strtoul(cp + 1, &cp, 16, &size))) {
		pr_dbg("%s pt mmiodev base: 0x%lx, size: 0x%lx\n", __func__, base_hpa, size);
		strncpy(mmio_devs[mmio_dev_idx].name, pt_mmiodev.name, 8);
		mmio_devs[mmio_dev_idx].dev.service_vm_pa = base_hpa;
		mmio_devs[mmio_dev_idx].dev.size = size;
		mmio_dev_idx++;
	} else {
		pr_err("%s, %s invalid, please check!\n", __func__, opt);
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

int init_mmio_dev(struct vmctx *ctx, struct mmio_dev_ops *ops, struct acrn_mmiodev *mmiodev)
{
	int ret;
	uint32_t base;

	if (mmiodev->user_vm_pa == 0UL) {
	/* FIXME: The mmio_dev_alloc_gpa_resource32 needs to add one new parameter to indicate
	 * if the caller needs one specific GPA instead of dynamic allocation.
	 */
		ret = mmio_dev_alloc_gpa_resource32(&base, mmiodev->size);
		if (ret < 0)
			return ret;
		mmiodev->user_vm_pa = base;
	}

	return ops->init(ctx, mmiodev);
}

void deinit_mmio_dev(struct vmctx *ctx, struct mmio_dev_ops *ops,  struct acrn_mmiodev *mmiodev)
{
	ops->deinit(ctx, mmiodev);
}

int init_mmio_devs(struct vmctx *ctx)
{
	int i, err = 0;
	struct mmio_dev_ops *ops;

	for (i = 0; i < MAX_MMIO_DEV_NUM; i++) {
		ops = mmio_dev_finddev(mmio_devs[i].name);	
		if (ops != NULL) {
			err = init_mmio_dev(ctx, ops, &mmio_devs[i].dev);
			pr_notice("mmiodev[%d] hpa:0x%x gpa:0x%x size:0x%x err:%d\n", i,
				  mmio_devs[i].dev.service_vm_pa,  mmio_devs[i].dev.user_vm_pa,
				  mmio_devs[i].dev.size, err);
		}

		if (err != 0)
			goto init_mmio_devs_fail;
	}

	return 0;

init_mmio_devs_fail:
	for (; i>=0; i--) {
		ops = mmio_dev_finddev(mmio_devs[i].name);	
		if (ops != NULL)
			deinit_mmio_dev(ctx, ops, &mmio_devs[i].dev);
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
			deinit_mmio_dev(ctx, ops, &mmio_devs[i].dev);

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
	.init		= init_pt_mmiodev,
	.deinit		= deinit_pt_mmiodev,
};
DEFINE_MMIO_DEV(tpm2);

/* @pre (pt_tpm2 == true) */
uint64_t get_mmio_dev_tpm2_base_gpa(void)
{
	int i;
	uint64_t base_gpa = 0UL;

	for (i = 0; i < mmio_dev_idx; i++) {
		if (!strcmp(mmio_devs[i].name, "MSFT0101")) {
			base_gpa = mmio_devs[i].dev.user_vm_pa;
			break;
		}
	}

	return base_gpa;
}

struct mmio_dev_ops pt_mmiodev = {
	.name		= "MMIODEV",
	/* ToDo: we may allocate the gpa MMIO resource in a reserved MMIO region
	 * rether than hard-coded here.
	 */
	.init		= init_pt_mmiodev,
	.deinit		= deinit_pt_mmiodev,
};
DEFINE_MMIO_DEV(pt_mmiodev);
