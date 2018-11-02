/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef CONFIG_DMAR_PARSE_ENABLED
#include <hypervisor.h>
#include "pci.h"
#include "vtd.h"
#include "acpi.h"

enum acpi_dmar_type {
	ACPI_DMAR_TYPE_HARDWARE_UNIT        = 0,
	ACPI_DMAR_TYPE_RESERVED_MEMORY      = 1,
	ACPI_DMAR_TYPE_ROOT_ATS             = 2,
	ACPI_DMAR_TYPE_HARDWARE_AFFINITY    = 3,
	ACPI_DMAR_TYPE_NAMESPACE            = 4,
	ACPI_DMAR_TYPE_RESERVED             = 5
};

/* Values for entry_type in ACPI_DMAR_DEVICE_SCOPE - device types */
enum acpi_dmar_scope_type {
	ACPI_DMAR_SCOPE_TYPE_NOT_USED       = 0,
	ACPI_DMAR_SCOPE_TYPE_ENDPOINT       = 1,
	ACPI_DMAR_SCOPE_TYPE_BRIDGE         = 2,
	ACPI_DMAR_SCOPE_TYPE_IOAPIC         = 3,
	ACPI_DMAR_SCOPE_TYPE_HPET           = 4,
	ACPI_DMAR_SCOPE_TYPE_NAMESPACE      = 5,
	ACPI_DMAR_SCOPE_TYPE_RESERVED       = 6 /* 6 and greater are reserved */
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
	int i;
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

typedef int (*dmar_iter_t)(struct acpi_dmar_header*, void*);

static struct dmar_info dmar_info_parsed;
static int dmar_unit_cnt;

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

static int
drhd_count_iter(struct acpi_dmar_header *dmar_header, __unused void *arg)
{
	if (dmar_header->type == ACPI_DMAR_TYPE_HARDWARE_UNIT)
		dmar_unit_cnt++;
	return 1;
}

static int
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
drhd_find_by_index(int idx)
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
dmar_path_bdf(int path_len, int busno,
	const struct acpi_dmar_pci_path *path)
{
	int i;
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


static int
handle_dmar_devscope(struct dmar_dev_scope *dev_scope,
	void *addr, int remaining)
{
	int path_len;
	uint16_t bdf;
	struct acpi_dmar_pci_path *path;
	struct acpi_dmar_device_scope *apci_devscope = addr;

	if (remaining < (int)sizeof(struct acpi_dmar_device_scope))
		return -1;

	if (remaining < apci_devscope->length)
		return -1;

	path = (struct acpi_dmar_pci_path *)(apci_devscope + 1);
	path_len = (apci_devscope->length -
			sizeof(struct acpi_dmar_device_scope)) /
			sizeof(struct acpi_dmar_pci_path);

	bdf = dmar_path_bdf(path_len, apci_devscope->bus, path);
	dev_scope->bus = (bdf >> 8) & 0xff;
	dev_scope->devfun = bdf & 0xff;

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
		if (scope->entry_type == ACPI_DMAR_SCOPE_TYPE_ENDPOINT ||
			scope->entry_type == ACPI_DMAR_SCOPE_TYPE_BRIDGE ||
			scope->entry_type == ACPI_DMAR_SCOPE_TYPE_NAMESPACE)
			count++;
		start += scope->length;
	}
	return count;
}

static int
handle_one_drhd(struct acpi_dmar_hardware_unit *acpi_drhd,
		struct dmar_drhd *drhd)
{
	struct dmar_dev_scope *dev_scope;
	struct acpi_dmar_device_scope *ads;
	int remaining, consumed;
	char *cp;
	uint32_t dev_count;

	drhd->segment = acpi_drhd->segment;
	drhd->flags = acpi_drhd->flags;
	drhd->reg_base_addr = acpi_drhd->address;

	if (drhd->flags & DRHD_FLAG_INCLUDE_PCI_ALL_MASK) {
		drhd->dev_cnt = 0;
		drhd->devices = NULL;
		return 0;
	}

	dev_count = get_drhd_dev_scope_cnt(acpi_drhd);
	drhd->dev_cnt = dev_count;
	if (dev_count) {
		drhd->devices =
			calloc(dev_count, sizeof(struct dmar_dev_scope));
		ASSERT(drhd->devices, "");
	} else {
		drhd->devices = NULL;
		return 0;
	}

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
		if (ads->entry_type != ACPI_DMAR_SCOPE_TYPE_IOAPIC &&
			ads->entry_type != ACPI_DMAR_SCOPE_TYPE_HPET)
			dev_scope++;
		else
			pr_dbg("drhd: skip dev_scope type %d",
				ads->entry_type);
	}

	return 0;
}

int parse_dmar_table(void)
{
	int i;
	struct acpi_dmar_hardware_unit *acpi_drhd;

	/* find out how many dmar units */
	dmar_iterate_tbl(drhd_count_iter, NULL);

	/* alloc memory for dmar uint */
	dmar_info_parsed.drhd_units =
		calloc(dmar_unit_cnt, sizeof(struct dmar_drhd));
	ASSERT(dmar_info_parsed.drhd_units, "");

	dmar_info_parsed.drhd_count = dmar_unit_cnt;

	for (i = 0; i < dmar_unit_cnt; i++) {
		acpi_drhd = drhd_find_by_index(i);
		if (acpi_drhd == NULL)
			continue;
		if (acpi_drhd->flags & DRHD_FLAG_INCLUDE_PCI_ALL_MASK)
			ASSERT((i+1) == dmar_unit_cnt,
				"drhd with flags set should be the last one");
		handle_one_drhd(acpi_drhd, &dmar_info_parsed.drhd_units[i]);
	}

	return 0;
}

/**
 * @post return != NULL
 */
struct dmar_info *get_dmar_info(void)
{
	if (dmar_info_parsed.drhd_count == 0) {
		parse_dmar_table();
	}
	return &dmar_info_parsed;
}
#endif
