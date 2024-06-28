/*
 * Copyright (C) 2018-2024 Intel Corporation.
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

static uint32_t uart_idx = 0U;

static int pt_uart_init(struct vmctx *ctx, struct acrn_acpidev *uart_dev)
{
	if(!ctx || !uart_dev)
		return -EINVAL;

	strncpy(uart_dev->name, "uart", sizeof(uart_dev->name));
	return init_pt_acpidev(ctx, uart_dev);
}

static int is_hid_legacy_uart(char *opts)
{
	int ret = true;
	char *hid =NULL;
	char *vtopts = NULL, *bak_vtopts = NULL;

	if (!opts || !*opts)
		return false;

	vtopts = strdup(opts);
	bak_vtopts = vtopts;
	hid = strsep(&vtopts, ",");
	if (!hid || strstr(hid, "PNP0501") == NULL)
		ret = false;

	free(bak_vtopts);
	return ret;
}

static void uart_write_dsdt(struct vmctx *ctx, struct acpi_dev *acpidev)
{
	if (!ctx || !acpidev)
		return;
	struct acrn_acpidev *dev = &acpidev->dev;
	struct acrn_acpires *res =NULL;
	
	dsdt_line("");
	dsdt_line("Device (COM%d)", uart_idx);
	dsdt_line("{");
	dsdt_line("  Name (_HID, EisaId (\"PNP0501\"))");
	dsdt_line("  Name (_UID, %s)", acpidev->uid);
	dsdt_line("  Name (_CRS, ResourceTemplate ()");
	dsdt_line("  {");
	dsdt_indent(2);
	for(int i = 0; i < ACPIDEV_RES_NUM; i++) {
		res = &dev->res[i];
		if(res->type == MEMORY_RES)
			dsdt_fixed_mem32(res->mmio_res.host_pa, res->mmio_res.size);
		else if(res->type == IO_PORT_RES)
			dsdt_fixed_ioport(res->pio_res.port_address, res->pio_res.size);
		else if(res->type == IRQ_RES)
			dsdt_fixed_irq(res->irq_res.irq);
	}
	dsdt_unindent(2);
	dsdt_line("  })");
	dsdt_line("}");

	uart_idx++;
}

static void pt_uart_deinit(struct vmctx *ctx, struct acrn_acpidev *uartdev)
{
	if (!ctx || !uartdev)
		return;
	vm_deassign_acpidev(ctx, uartdev);
}

struct acpi_dev_ops pt_uart_dev_ops = {
	.hid 		= "PNP0501",
	.match 		= is_hid_legacy_uart,
	.init 		= pt_uart_init,
	.deinit 	= pt_uart_deinit,
	.write_dsdt 	= uart_write_dsdt,
};
DEFINE_ACPI_DEV(pt_uart_dev_ops);
