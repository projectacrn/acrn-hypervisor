/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "acpi.h"
#include "vmmapi.h"
#include "log.h"
#include "acpi_dev.h"
#include "dm.h"

static int pt_ith_init(struct vmctx *ctx, struct acrn_acpidev *ith_dev);
static int is_hid_ith(char *opts);
static void ith_write_dsdt(struct vmctx *ctx, struct acpi_dev *ith_dev);
static void ith_write_madt(struct vmctx *ctx, FILE *fp, struct acpi_dev *ith_dev);

struct acpi_dev_ops pt_ith_dev_ops = {
	.match = is_hid_ith,
	.init = pt_ith_init,
	.deinit = deinit_pt_acpidev,
	.write_dsdt = ith_write_dsdt,
	.write_madt = ith_write_madt,
};
DEFINE_ACPI_DEV(pt_ith_dev_ops);

char* ith_device_id[] = {
	/* Tiger Lake */
	"INTC1001",
	/* Alder Lake */
	"INTC1001",
	/* Elkhart Lake */
	"INTC1001"
};

static int pt_ith_init(struct vmctx *ctx, struct acrn_acpidev *ith_dev)
{
	if(!ctx || !ith_dev)
		return -EINVAL;

	strcpy(ith_dev->name, "ith");
	return init_pt_acpidev(ctx, ith_dev);
}

static int is_hid_ith(char *opts)
{
	char *hid, *vtopts, *devopts;

	if (!opts || !*opts)
		return false;

	vtopts = devopts = strdup(opts);
	hid = strsep(&devopts, ",");
	if (!hid) {
		free(vtopts);
		return false;
	}

	for (int i = 0; i < (sizeof(ith_device_id) / sizeof(ith_device_id[0])); i++) {
		if (strstr(hid, ith_device_id[i]) != NULL)	{
			strcpy(pt_ith_dev_ops.hid, hid);
			free(vtopts);
			return true;
		}
	}

	free(vtopts);
	return false;
}

static void ith_write_dsdt(struct vmctx *ctx, struct acpi_dev *ith_dev)
{
	dsdt_line("");
	dsdt_line("  Scope (_SB)");
	dsdt_line("  {");
	dsdt_line("    Device (GPI%s)", ith_dev->uid);
	dsdt_line("    {");
	dsdt_line("      Method (_HID, 0, NotSerialized)");
	dsdt_line("      {");
	dsdt_line("        Return (\"%s\")", ith_dev->hid);
	dsdt_line("      }");
	dsdt_line("      Name (_UID, %s)", ith_dev->uid);
	dsdt_line("      Name (_CRS, ResourceTemplate ()");
	dsdt_line("      {");
	dsdt_indent(4);
	for (int i = 0; i < ACPIDEV_RES_NUM; i++) {
		if (ith_dev->dev.res[i].type == IRQ_RES) {
			dsdt_line("    Interrupt (ResourceConsumer, Level, ActiveLow, Shared, ,,)");
			dsdt_line("    {");
			dsdt_line("        0X%08X,", ith_dev->dev.res[i].irq_res.irq);
			dsdt_line("    }");
		} else if (ith_dev->dev.res[i].type == MEMORY_RES) {
			dsdt_fixed_mem32(ith_dev->dev.res[i].mmio_res.user_vm_pa, ith_dev->dev.res[i].mmio_res.size);
		}
	}
	dsdt_unindent(4);
	dsdt_line("      })");
	dsdt_line("    }");
	dsdt_line("  }");
}

static void ith_write_madt(struct vmctx *ctx, FILE *fp, struct acpi_dev *ith_dev)
{
	for (int i = 0; i < ACPIDEV_RES_NUM; i++) {
		if (ith_dev->dev.res[i].type == IRQ_RES) {
			EFPRINTF(fp, "[0001]\t\tSubtable Type : 02\n");
			EFPRINTF(fp, "[0001]\t\tLength : 0A\n");
			EFPRINTF(fp, "[0001]\t\tBus : 00\n");
			EFPRINTF(fp, "[0001]\t\tSource : %02X\n", ith_dev->dev.res[i].irq_res.irq);
			EFPRINTF(fp, "[0004]\t\tInterrupt : %08X\n", ith_dev->dev.res[i].irq_res.irq);
			EFPRINTF(fp, "[0002]\t\tFlags (decoded below) : 000F\n");
			EFPRINTF(fp, "\t\t\tPolarity : %d\n", ith_dev->dev.res[i].irq_res.polarity);
			EFPRINTF(fp, "\t\t\tTrigger Mode : %d\n", ith_dev->dev.res[i].irq_res.trigger_mode);
			EFPRINTF(fp, "\n");
		}
	}
}
