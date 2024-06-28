/*
 * Copyright (C) 2020-2022 Intel Corporation.
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

static inline char *ltrim(char *s)
{
	while (*s && *s == ' ')
		s++;
	return s;
}

struct mmio_dev *get_mmiodev(char *name)
{
	int i;
	struct mmio_dev *dev;

	for (i = 0; i < mmio_dev_idx; i++) {
		dev = &mmio_devs[i];
		if (!strncmp(dev->name, name, 16)) {
			return dev;
		}
	}

	return NULL;
}

struct mmio_dev *alloc_mmiodev(void)
{
	return (mmio_dev_idx >= MAX_MMIO_DEV_NUM) ? NULL : &mmio_devs[mmio_dev_idx++];
}

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

/**
 * Parse /proc/iomem to see if there's an entry that contains name.
 * Returns false if not found,
 * Returns true if an entry is found, and res_start and res_size will be filled
 * 	to the start and (end - start + 1) of the first found entry.
 *
 * @pre (name != NULL) && (strlen(name) > 0)
 */
bool get_mmio_hpa_resource(char *name, uint64_t *res_start, uint64_t *res_size, uint8_t region_index)
{
	FILE *fp;
	uint64_t start, end;
	uint8_t region_count = 0;
	bool found = false;
	char line[128];
	char *cp;

	fp = fopen("/proc/iomem", "r");
	if (!fp) {
		pr_err("Error opening /proc/iomem\n");
		return false;
	}

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, name)) {
			if ((!dm_strtoul(ltrim(line), &cp, 16, &start) && *cp == '-') &&
				(!dm_strtoul(cp + 1, &cp, 16, &end))) {
				if ((start == 0) && (end == 0)) {
					pr_err("Please run acrn-dm with superuser privilege\n");
					break;
				}
			} else {
				pr_err("Parsing /proc/iomem failed\n");
				break;
			}

			if (region_count == region_index) {
				*res_start = start;
				/* proc/iomem displays regions like: 000-fff so we add 1 as size */
				*res_size = end - start + 1;
				found = true;
				break;
			}
			region_count++;
		}
	}

	fclose(fp);
	return found;
}

int create_pt_mmiodev(char *opt)
{

	int err = 0;
	uint64_t base_hpa, size;
	struct mmio_dev *dev;
	char *cp;

	dev = alloc_mmiodev();
	if (!dev) {
		pr_err("MMIO dev number exceed MAX_MMIO_DEV_NUM!!!\n");
		return -EINVAL;
	}

	if((!dm_strtoul(opt, &cp, 16, &base_hpa) && *cp == ',') &&
		(!dm_strtoul(cp + 1, &cp, 16, &size))) {
		pr_dbg("%s pt mmiodev base: 0x%lx, size: 0x%lx\n", __func__, base_hpa, size);
		strncpy(dev->name, pt_mmiodev.name, 8);
		dev->dev.res[0].host_pa = base_hpa;
		dev->dev.res[0].size = size;
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

	if (mmiodev->res[0].user_vm_pa == 0UL) {
	/* FIXME: The mmio_dev_alloc_gpa_resource32 needs to add one new parameter to indicate
	 * if the caller needs one specific GPA instead of dynamic allocation.
	 */
		ret = mmio_dev_alloc_gpa_resource32(&base, mmiodev->res[0].size);
		if (ret < 0)
			return ret;
		mmiodev->res[0].user_vm_pa = base;
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
				  mmio_devs[i].dev.res[0].host_pa,  mmio_devs[i].dev.res[0].user_vm_pa,
				  mmio_devs[i].dev.res[0].size, err);
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

struct mmio_dev_ops pt_mmiodev = {
	.name		= "MMIODEV",
	/* ToDo: we may allocate the gpa MMIO resource in a reserved MMIO region
	 * rether than hard-coded here.
	 */
	.init		= init_pt_mmiodev,
	.deinit		= deinit_pt_mmiodev,
};
DEFINE_MMIO_DEV(pt_mmiodev);
