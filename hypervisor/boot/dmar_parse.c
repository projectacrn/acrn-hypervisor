/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef CONFIG_DMAR_PARSE_ENABLED
#include <types.h>
#include <logmsg.h>
#include <host_pm.h>
#include <io.h>
#include <spinlock.h>
#include "pci.h"
#include "vtd.h"
#include "acpi_priv.h"

enum acpi_dmar_type {
	ACPI_DMAR_TYPE_HARDWARE_UNIT        = 0,
	ACPI_DMAR_TYPE_RESERVED_MEMORY      = 1,
	ACPI_DMAR_TYPE_ROOT_ATS             = 2,
	ACPI_DMAR_TYPE_HARDWARE_AFFINITY    = 3,
	ACPI_DMAR_TYPE_NAMESPACE            = 4,
	ACPI_DMAR_TYPE_RESERVED             = 5
};

struct acpi_table_dmar {
	/* Common ACPI table header */
	struct acpi_table_header  header;
	/* Host address Width */
	uint8_t                   width;
	uint8_t                   flags;
	uint8_t                   reserved[10];
};

/* DMAR subtable header */
struct acpi_dmar_header {
	uint16_t                  type;
	uint16_t                  length;
};

struct acpi_dmar_hardware_unit {
	struct acpi_dmar_header   header;
	uint8_t                   flags;
	uint8_t                   reserved;
	uint16_t                  segment;
	/* register base address */
	uint64_t                  address;
};

struct find_iter_args {
	int32_t i;
	struct acpi_dmar_hardware_unit *res;
};

struct acpi_dmar_pci_path {
	uint8_t                   device;
	uint8_t                   function;
};

struct acpi_dmar_device_scope {
	uint8_t                   entry_type;
	uint8_t                   length;
	uint16_t                  reserved;
	uint8_t                   enumeration_id;
	uint8_t                   bus;
};

typedef int32_t (*dmar_iter_t)(struct acpi_dmar_header*, void*);

static int32_t dmar_unit_cnt;

static void
dmar_iterate_tbl(dmar_iter_t iter, void *arg)
{
	struct acpi_table_dmar *dmar_tbl;
	struct acpi_dmar_header *dmar_header;
	char *ptr, *ptr_end;

	dmar_tbl = (struct acpi_table_dmar *)get_dmar_table();
	ASSERT(dmar_tbl != NULL, "");

	ptr = (char *)dmar_tbl + sizeof(*dmar_tbl);
	ptr_end = (char *)dmar_tbl + dmar_tbl->header.length;

	for (;;) {
		if (ptr >= ptr_end)
			break;
		dmar_header = (struct acpi_dmar_header *)ptr;
		if (dmar_header->length <= 0) {
			pr_err("drhd: corrupted DMAR table, l %d\n",
				dmar_header->length);
			break;
		}
		ptr += dmar_header->length;
		if (!iter(dmar_header, arg))
			break;
	}
}

static int32_t
drhd_count_iter(struct acpi_dmar_header *dmar_header, __unused void *arg)
{
	if (dmar_header->type == ACPI_DMAR_TYPE_HARDWARE_UNIT)
		dmar_unit_cnt++;
	return 1;
}

static int32_t
drhd_find_iter(struct acpi_dmar_header *dmar_header, void *arg)
{
	struct find_iter_args *args;

	if (dmar_header->type != ACPI_DMAR_TYPE_HARDWARE_UNIT)
		return 1;

	args = arg;
	if (args->i == 0) {
		args->res = (struct acpi_dmar_hardware_unit *)dmar_header;
		return 0;
	}
	args->i--;
	return 1;
}

static struct acpi_dmar_hardware_unit *
drhd_find_by_index(int32_t idx)
{
	struct find_iter_args args;

	args.i = idx;
	args.res = NULL;
	dmar_iterate_tbl(drhd_find_iter, &args);
	return args.res;
}

static uint8_t get_secondary_bus(uint8_t bus, uint8_t dev, uint8_t func)
{
	uint32_t data;

	pio_write32(PCI_CFG_ENABLE | (bus << 16U) | (dev << 11U) |
		(func << 8U) | 0x18U, PCI_CONFIG_ADDR);

	data = pio_read32(PCI_CONFIG_DATA);

	return (data >> 8U) & 0xffU;
}

static uint16_t
dmar_path_bdf(int32_t path_len, int32_t busno,
	const struct acpi_dmar_pci_path *path)
{
	int32_t i;
	uint8_t bus;
	uint8_t dev;
	uint8_t fun;


	bus = (uint8_t)busno;
	dev = path->device;
	fun = path->function;


	for (i = 1; i < path_len; i++) {
		bus = get_secondary_bus(bus, dev, fun);
		dev = path[i].device;
		fun = path[i].function;
	}
	return (bus << 8U | DEVFUN(dev, fun));
}


static int32_t
handle_dmar_devscope(struct dmar_dev_scope *dev_scope,
	void *addr, int32_t remaining)
{
	int32_t path_len;
	uint16_t bdf;
	struct acpi_dmar_pci_path *path;
	struct acpi_dmar_device_scope *apci_devscope = addr;

	if (remaining < (int32_t)sizeof(struct acpi_dmar_device_scope))
		return -1;

	if (remaining < apci_devscope->length)
		return -1;

	path = (struct acpi_dmar_pci_path *)(apci_devscope + 1);
	path_len = (apci_devscope->length -
			sizeof(struct acpi_dmar_device_scope)) /
			sizeof(struct acpi_dmar_pci_path);

	bdf = dmar_path_bdf(path_len, apci_devscope->bus, path);
	dev_scope->id = apci_devscope->enumeration_id;
	dev_scope->type = apci_devscope->entry_type;
	dev_scope->bus = (bdf >> 8U) & 0xffU;
	dev_scope->devfun = bdf & 0xffU;

	return apci_devscope->length;
}

static uint32_t
get_drhd_dev_scope_cnt(struct acpi_dmar_hardware_unit *drhd)
{
	struct acpi_dmar_device_scope *scope;
	char *start;
	char *end;
	uint32_t count = 0;

	start = (char *)drhd + sizeof(struct acpi_dmar_hardware_unit);
	end = (char *)drhd + drhd->header.length;

	while (start < end) {
		scope = (struct acpi_dmar_device_scope *)start;
		if ((scope->entry_type != ACPI_DMAR_SCOPE_TYPE_NOT_USED) &&
			(scope->entry_type < ACPI_DMAR_SCOPE_TYPE_RESERVED))
			count++;
		start += scope->length;
	}
	return count;
}

static int32_t
handle_one_drhd(struct acpi_dmar_hardware_unit *acpi_drhd,
		struct dmar_drhd *drhd)
{
	struct dmar_dev_scope *dev_scope;
	struct acpi_dmar_device_scope *ads;
	int32_t remaining, consumed;
	char *cp;
	uint32_t dev_count;

	drhd->segment = acpi_drhd->segment;
	drhd->flags = acpi_drhd->flags;
	drhd->reg_base_addr = acpi_drhd->address;

	dev_count = get_drhd_dev_scope_cnt(acpi_drhd);
	ASSERT(dev_count <= MAX_DRHD_DEVSCOPES, "parsed dev_count > MAX_DRHD_DEVSCOPES");

	drhd->dev_cnt = dev_count;

	remaining = acpi_drhd->header.length -
			sizeof(struct acpi_dmar_hardware_unit);

	dev_scope = drhd->devices;

	while (remaining > 0) {
		cp = (char *)acpi_drhd + acpi_drhd->header.length - remaining;

		consumed = handle_dmar_devscope(dev_scope, cp, remaining);

		if (((drhd->segment << 16U) |
		     (dev_scope->bus << 8U) |
		     dev_scope->devfun) == CONFIG_GPU_SBDF) {
			ASSERT(dev_count == 1, "no dedicated iommu for gpu");
			drhd->ignore = true;
		}

		if (consumed <= 0)
			break;

		remaining -= consumed;
		/* skip IOAPIC & HPET */
		ads = (struct acpi_dmar_device_scope *)cp;
		if ((ads->entry_type != ACPI_DMAR_SCOPE_TYPE_NOT_USED) &&
			(ads->entry_type < ACPI_DMAR_SCOPE_TYPE_RESERVED)) {
			dev_scope++;
		}
		else
			pr_dbg("drhd: skip dev_scope type %d",
				ads->entry_type);
	}

	return 0;
}

int32_t parse_dmar_table(struct dmar_info *plat_dmar_info)
{
	int32_t i;
	struct acpi_dmar_hardware_unit *acpi_drhd;

	/* find out how many dmar units */
	dmar_iterate_tbl(drhd_count_iter, NULL);
	ASSERT(dmar_unit_cnt <= MAX_DRHDS, "parsed dmar_unit_cnt > MAX_DRHDS");

	plat_dmar_info->drhd_count = dmar_unit_cnt;

	for (i = 0; i < dmar_unit_cnt; i++) {
		acpi_drhd = drhd_find_by_index(i);
		if (acpi_drhd == NULL)
			continue;
		if (acpi_drhd->flags & DRHD_FLAG_INCLUDE_PCI_ALL_MASK)
			ASSERT((i+1) == dmar_unit_cnt,
				"drhd with flags set should be the last one");
		handle_one_drhd(acpi_drhd, &(plat_dmar_info->drhd_units[i]));
	}

	return 0;
}

#endif
