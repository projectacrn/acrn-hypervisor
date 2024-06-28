/*
 * Copyright (C) 2020-2024 Intel Corporation.
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
#include "irq.h"
#include "log.h"
#include "acpi_dev.h"

#define MAX_ACPI_DEV_NUM	8

#define	MAX_IOPORTS	(1 << 16)

enum {
	TYPE_OPT = 0,
	MINIMUM_ADDRESS_OPT,
	LENGTH_OPT,
	IRQ_OPT,
	POLARTY_OPT,
	TRIGGER_MODE_OPT,
	DSDT_OPT
};

char *const pt_acpi_token[] = {
	[TYPE_OPT]            = "type",
	[MINIMUM_ADDRESS_OPT] = "min",
	[LENGTH_OPT]          = "len",
	[IRQ_OPT]             = "irq",
	[POLARTY_OPT]         = "polarity",
	[TRIGGER_MODE_OPT]    = "trigger_mode",
	[DSDT_OPT]            = "dsdt",
	NULL
};

static struct acpi_dev acpi_devs[MAX_ACPI_DEV_NUM];
static uint32_t acpi_dev_idx = 0U;

struct acpi_dev *get_acpidev(char *hid, char *uid)
{
	int i;
	struct acpi_dev *dev;

	/* acpi_dev_idx is the current depth of acpi devices array */
	for (i = 0; i < acpi_dev_idx; i++) {
		dev = &acpi_devs[i];
		if ((!strcmp(dev->hid, hid)) && (!strcmp(dev->uid, uid))) {
			return dev;
		}
	}

	return NULL;
}

struct acpi_dev *alloc_acpidev(char *hid, char *uid)
{
	struct acpi_dev *dev =  NULL;
	dev = get_acpidev(hid, uid);
	if (dev != NULL) {
		return dev;
	}

	/* Check if ACPI device array is full */
	if (acpi_dev_idx == MAX_ACPI_DEV_NUM) {
		return NULL;
	} else if (hid && uid) {
		acpi_devs[acpi_dev_idx].hid = strdup(hid);
		acpi_devs[acpi_dev_idx].uid = strdup(uid);
		return &acpi_devs[acpi_dev_idx++];
	}

	return NULL;
}

int create_pt_acpidev(char *opts)
{
	struct acpi_dev *dev;
	struct acpi_dev_ops **adops, *ops;
	char *devopts, *vtopts, *value;
	char *hid, *uid;
	uint32_t base_address, length;
	uint32_t buf;
	int i, ret = 0, index = -1;

	SET_FOREACH(adops, acpi_dev_ops_set) {
		ops = *adops;
		if (ops->match && opts && ops->match(opts)) {
			devopts = vtopts = strdup(opts);
			hid = strsep(&vtopts, ",");
			if (!hid) {
				free(devopts);
				return -EINVAL;
			}

			uid = (char *)malloc(16);
			if (!uid) {
				pr_err(("UID malloc failed for ACPI device!\n"));
				return -ENOMEM;
			}
			if (vtopts != NULL && !strncmp(vtopts, "uid=", 4)) {
				strcpy(uid, strsep(&vtopts, ",") + 4);
			} else {
				strcpy(uid, "0");
			}

			dev = alloc_acpidev(hid, uid);
			if (!dev) {
				pr_err("Failed to create ACPI device %s:%s due to exceeding max ACPI device number\n", hid, uid);
				free(uid);
				free(devopts);
				return -EINVAL;
			}

			if (ops->create) {
				return ops->create(opts, dev);
			}

			index = -1;
			for (i = 0; i < ACPIDEV_RES_NUM; i++) {
				if (dev->dev.res[i].type == INVALID_RES) {
					index = i;
					break;
				}
			}
			if (index == -1) {
				pr_err("ACPI resource number for device %s:%s exceed limit.\n", hid, uid);
				free(uid);
				free(devopts);
				return -EINVAL;
			}

			while (vtopts != NULL && strcmp(vtopts, "") && !ret)
			{
				switch (getsubopt(&vtopts, pt_acpi_token, &value)) {
					case TYPE_OPT:
						if (value == NULL) {
							pr_err("Missing value for --acpidev_pt suboption '%s'\n", pt_acpi_token[TYPE_OPT]);
							ret = -EINVAL;
							break;
						}
						if (!strcasecmp(value, "memory")) {
							dev->dev.res[index].type = MEMORY_RES;
						} else if (!strcasecmp(value, "io_port")) {
							dev->dev.res[index].type = IO_PORT_RES;
						} else if (!strcasecmp(value, "irq")) {
							dev->dev.res[index].type = IRQ_RES;
						} else {
							pr_err("Invalid ACPI device resource type: %s\n", value);
							ret = -EINVAL;
						}
						break;
					case MINIMUM_ADDRESS_OPT:
					    if (value == NULL) {
							pr_err("Missing value for --acpidev_pt suboption '%s'\n", pt_acpi_token[MINIMUM_ADDRESS_OPT]);
							ret = -EINVAL;
							break;
						}
						if (!dm_strtoui(value, NULL, 16, &base_address)) {
							if (dev->dev.res[index].type == MEMORY_RES && (base_address & (PAGE_SIZE - 1UL)) == 0) {
								dev->dev.res[index].mmio_res.host_pa = base_address;
							} else if (dev->dev.res[index].type == IO_PORT_RES && base_address < MAX_IOPORTS) {
								dev->dev.res[index].pio_res.port_address = base_address;
							} else {
								pr_err("Invalid ACPI device resource address: %s for type %d\n",
										base_address, dev->dev.res[index].type);
								ret = -EINVAL;
							}
						} else {
							pr_err("Invalid ACPI device resource address: %s\n", value);
							ret = -EINVAL;
						}
						break;
					case LENGTH_OPT:
						if (value == NULL) {
							pr_err("Missing value for --acpidev_pt suboption '%s'\n", pt_acpi_token[LENGTH_OPT]);
							ret = -EINVAL;
							break;
						}
						if (!dm_strtoui(value, NULL, 16, &length)) {
							if (dev->dev.res[index].type == MEMORY_RES) {
								dev->dev.res[index].mmio_res.size = roundup2(length, PAGE_SIZE);
								if (!mmio_dev_alloc_gpa_resource32(&base_address, length)) {
									dev->dev.res[index].mmio_res.user_vm_pa = base_address;
								} else {
									pr_err("Cannot allocate GPA for ACPI device memory resource of length: %d\n", length);
									ret =  -EINVAL;
								}
							} else if (dev->dev.res[index].type == IO_PORT_RES) {
								dev->dev.res[index].pio_res.size = (uint16_t)length;
							}
						} else {
							pr_err("Invalid ACPI device resource length: %s\n", value);
							ret = -EINVAL;
						}
						break;
					case IRQ_OPT:
						if (value == NULL) {
							pr_err("Missing value for --acpidev_pt suboption '%s'\n", pt_acpi_token[IRQ_OPT]);
							ret = -EINVAL;
							break;
						}
						if(!dm_strtoui(value, NULL, 10, &buf)) {
							dev->dev.res[index].irq_res.irq = (uint8_t)buf;
							pci_irq_reserve(buf);
							pr_warn("ACPI device IRQ resource may be shared with other device. Please use with caution.\n");
						} else {
							pr_err("Invalid ACPI device IRQ resource number: %s\n", value);
							ret = -EINVAL;
						}
						break;
					case POLARTY_OPT:
						if (value == NULL) {
							pr_err("Missing value for --acpidev_pt suboption '%s'\n", pt_acpi_token[POLARTY_OPT]);
							ret = -EINVAL;
							break;
						}
						if(!dm_strtoui(value, NULL, 10, &buf) && (buf & ~(uint32_t)(0x3)) == 0) {
							dev->dev.res[index].irq_res.polarity = (uint8_t)buf;
						} else {
							pr_err("Invalid ACPI device IRQ resource polarity: %s\n", value);
							ret = -EINVAL;
						}
						break;
					case TRIGGER_MODE_OPT:
						if (value == NULL) {
							pr_err("Missing value for --acpidev_pt suboption '%s'\n", pt_acpi_token[TRIGGER_MODE_OPT]);
							ret = -EINVAL;
							break;
						}
						if(!dm_strtoui(value, NULL, 10, &buf) && (buf & ~(uint32_t)(0x3)) == 0) {
							dev->dev.res[index].irq_res.trigger_mode = (uint8_t)buf;
						} else {
							pr_err("Invalid ACPI device IRQ resource trigger mode: %s\n", value);
							ret = -EINVAL;
						}
						break;
					case DSDT_OPT:
						if (value == NULL) {
							pr_err("Missing value for --acpidev_pt suboption '%s'\n", pt_acpi_token[DSDT_OPT]);
							ret = -EINVAL;
							break;
						}
						dev->dsdt = strdup(value);
						break;
					default:
						pr_err("No match found for pt_acpi_token: %s\n", vtopts);
						ret = -EINVAL;
				}
			}

			free(uid);
			free(devopts);
			return ret;
		}
	}

	pr_err("Unrecognized or unsupported ACPI device: %s\n", opts);
	ret = -EINVAL;
	return ret;
}

static struct acpi_dev_ops *acpi_dev_finddev(char *hid)
{
	struct acpi_dev_ops **adpp, *adp;

	SET_FOREACH(adpp, acpi_dev_ops_set) {
		adp = *adpp;
		if (adp->hid != NULL && !strcmp(adp->hid, hid))
			return adp;
	}

	return NULL;
}

/**
 * Search /sys/bus/acpi/devices for given HID and fill modalias to ops.
 * (TODO: we may add more functionality later when we support pt
 * of other ACPI dev)
 * According to https://www.kernel.org/doc/Documentation/acpi/namespace.txt,
 *
 * The Linux ACPI subsystem converts ACPI namespace objects into a Linux
 * device tree under the /sys/devices/LNXSYSTEM:00 and updates it upon
 * receiving ACPI hotplug notification events.  For each device object in this
 * hierarchy there is a corresponding symbolic link in the
 * /sys/bus/acpi/devices.
 */
int get_more_acpi_dev_info(char *hid, uint32_t instance, struct acpi_dev_ops *ptops)
{
	char pathbuf[128], line[32];
	int ret = -1;
	size_t ch_read;
	FILE *fp;

	snprintf(pathbuf, sizeof(pathbuf), "/sys/bus/acpi/devices/%s:%02x/modalias", hid, instance);
	fp = fopen(pathbuf, "r");
	if (!fp)
		return ret;

	ch_read = fread(line, 1, sizeof(line), fp);
	if (!ch_read)
		goto out;

	strcpy(ptops->hid, hid);
	memcpy(ptops->modalias, line, ch_read);
	ret = 0;

out:
	fclose(fp);
	return ret;
}

void acpi_dev_write_dsdt(struct vmctx *ctx)
{
	int i;
	struct acpi_dev_ops *ops;

	for (i = 0; i < acpi_dev_idx; i++) {
		ops = acpi_dev_finddev(acpi_devs[i].hid);
		if (ops != NULL && ops->write_dsdt) {
			ops->write_dsdt(ctx, &acpi_devs[i]);
		}
	}
}

void acpi_dev_write_madt(FILE *fp, struct vmctx *ctx)
{
	int i;
	struct acpi_dev_ops *ops;

	for (i = 0; i < acpi_dev_idx; i++) {
		ops = acpi_dev_finddev(acpi_devs[i].hid);
		if (ops != NULL && ops->write_madt) {
			ops->write_madt(ctx, fp, &acpi_devs[i]);
		}
	}
}

int init_acpi_dev(struct vmctx *ctx, struct acpi_dev_ops *ops, struct acrn_acpidev *acpidev)
{
	struct inout_port iop;
	int i = 0;

	for (i = 0; i < ACPIDEV_RES_NUM; i++) {
		if (acpidev->res[i].type == IO_PORT_RES) {
			bzero(&iop, sizeof(iop));
			iop.name = "Passthrough";
			iop.port = acpidev->res[i].pio_res.port_address;
			iop.size = acpidev->res[i].pio_res.size;
			iop.flags = IOPORT_F_INOUT;
			iop.handler = default_inout;
			if (iop.port + iop.size >= MAX_IOPORTS) {
				pr_err("ACPI device port resource 0x%x-0x%x exceed valid range",
						iop.port, iop.port + iop.size);
				return -EINVAL;
			} else if(register_inout(&iop)) {
				pr_err("ACPI device port resource 0x%x-0x%x conflict with allocated port address",
						iop.port, iop.port + iop.size);
				return -EINVAL;
			}
		}
	}

	return ops->init(ctx, acpidev);
}

void deinit_acpi_dev(struct vmctx *ctx, struct acpi_dev_ops *ops,  struct acrn_acpidev *acpidev)
{
	struct inout_port iop;
	int i = 0;

	for (i = 0; i < ACPIDEV_RES_NUM; i++) {
		if (acpidev->res[i].type == IO_PORT_RES) {
			bzero(&iop, sizeof(iop));
			iop.name = "Passthrough";
			iop.port = acpidev->res[i].pio_res.port_address;
			iop.size = acpidev->res[i].pio_res.size;
			unregister_inout(&iop);
		}
	}

	ops->deinit(ctx, acpidev);
}

int init_acpi_devs(struct vmctx *ctx)
{
	int i, err = 0;
	struct acpi_dev_ops *ops;

	for (i = 0; i < acpi_dev_idx; i++) {
		ops = acpi_dev_finddev(acpi_devs[i].hid);
		if (ops != NULL) {
			err = init_acpi_dev(ctx, ops, &acpi_devs[i].dev);
			pr_notice("pt acpidev[%d] hid:%s uid:%s err:%d\n", i,
				  acpi_devs[i].hid, acpi_devs[i].uid, err);
		}

		if (err != 0)
			goto init_acpi_devs_fail;
	}

	return 0;

init_acpi_devs_fail:
	for (; i>=0; i--) {
		ops = acpi_dev_finddev(acpi_devs[i].hid);
		if (ops != NULL)
			deinit_acpi_dev(ctx, ops, &acpi_devs[i].dev);
	}

	return err;
}

void deinit_acpi_devs(struct vmctx *ctx)
{
	int i;
	struct acpi_dev_ops *ops;

	for (i = 0; i < acpi_dev_idx; i++) {
		ops = acpi_dev_finddev(acpi_devs[i].hid);
		if (ops != NULL)
			deinit_acpi_dev(ctx, ops, &acpi_devs[i].dev);
		
		free(acpi_devs[i].hid);
		free(acpi_devs[i].cid);
		free(acpi_devs[i].uid);
		free(acpi_devs[i].dsdt);
	}
}

int init_pt_acpidev(struct vmctx *ctx, struct acrn_acpidev *dev)
{
	return vm_assign_acpidev(ctx, dev);
}

void deinit_pt_acpidev(struct vmctx *ctx, struct acrn_acpidev *dev)
{
	vm_deassign_acpidev(ctx, dev);
}