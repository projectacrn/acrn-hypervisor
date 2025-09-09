/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <logmsg.h>
#include <asm/io.h>
#include <spinlock.h>
#include <asm/cpu_caps.h>
#include <pci.h>
#include <asm/vtd.h>
#include <acpi.h>

static uint32_t dmar_unit_cnt;
static struct dmar_drhd drhd_info_array[MAX_DRHDS];
static struct dmar_dev_scope drhd_dev_scope[MAX_DRHDS][MAX_DRHD_DEVSCOPES];

/*
 * @post return != NULL
 */
static void *get_dmar_table(void)
{
	return get_acpi_tbl(ACPI_SIG_DMAR);
}

static uint8_t get_secondary_bus(uint8_t bus, uint8_t dev, uint8_t func)
{
	uint32_t data;

	pio_write32(PCI_CFG_ENABLE | ((uint32_t)bus << 16U) | ((uint32_t)dev << 11U) |
		((uint32_t)func << 8U) | 0x18U, PCI_CONFIG_ADDR);

	data = pio_read32(PCI_CONFIG_DATA);

	return (data >> 8U) & 0xffU;
}

static union pci_bdf dmar_path_bdf(int32_t path_len, uint8_t busno, const struct acpi_dmar_pci_path *path)
{
	int32_t i;
	union pci_bdf dmar_bdf;

	dmar_bdf.bits.b = busno;
	dmar_bdf.bits.d = path->device;
	dmar_bdf.bits.f = path->function;

	for (i = 1; i < path_len; i++) {
		dmar_bdf.bits.b = get_secondary_bus(dmar_bdf.bits.b, dmar_bdf.bits.d, dmar_bdf.bits.f);
		dmar_bdf.bits.d = path[i].device;
		dmar_bdf.bits.f = path[i].function;
	}
	return dmar_bdf;
}


static int32_t handle_dmar_devscope(struct dmar_dev_scope *dev_scope, void *addr, int32_t remaining)
{
	int32_t path_len, ret = -1;
	union pci_bdf dmar_bdf;
	struct acpi_dmar_pci_path *path;
	struct acpi_dmar_device_scope *apci_devscope = addr;

	if ((remaining >= (int32_t)sizeof(struct acpi_dmar_device_scope)) &&
	    (remaining >= (int32_t)apci_devscope->length)) {
		path = (struct acpi_dmar_pci_path *)(apci_devscope + 1);
		path_len = (int32_t)((apci_devscope->length - sizeof(struct acpi_dmar_device_scope)) /
				sizeof(struct acpi_dmar_pci_path));

		dmar_bdf = dmar_path_bdf(path_len, apci_devscope->bus, path);
		dev_scope->id = apci_devscope->enumeration_id;
		dev_scope->type = apci_devscope->entry_type;
		dev_scope->bus = dmar_bdf.fields.bus;
		dev_scope->devfun = dmar_bdf.fields.devfun;
		ret = (int32_t)apci_devscope->length;
	}

	return ret;
}

static uint32_t get_drhd_dev_scope_cnt(struct acpi_dmar_hardware_unit *drhd)
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
			(scope->entry_type < ACPI_DMAR_SCOPE_TYPE_RESERVED)) {
			count++;
		}
		start += scope->length;
	}
	return count;
}

/**
 * @Application constraint: The dedicated DMAR unit for Intel integrated GPU
 * shall be available on the physical platform.
 */
static int32_t handle_one_drhd(struct acpi_dmar_hardware_unit *acpi_drhd, struct dmar_drhd *drhd)
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

	remaining = (int32_t)(acpi_drhd->header.length - sizeof(struct acpi_dmar_hardware_unit));

	dev_scope = drhd->devices;

	while (remaining > 0) {
		cp = (char *)acpi_drhd + acpi_drhd->header.length - remaining;

		consumed = handle_dmar_devscope(dev_scope, cp, remaining);

		/* Disable GPU IOMMU due to gvt-d hasn’t been enabled on APL yet. */
		if (is_apl_platform()) {
			if ((((uint32_t)drhd->segment << 16U) |
		     	     ((uint32_t)dev_scope->bus << 8U) |
		     	     dev_scope->devfun) == CONFIG_IGD_SBDF) {
				drhd->ignore = true;
			}
		}

		if (consumed <= 0) {
			break;
		}

		remaining -= consumed;
		/* skip IOAPIC & HPET */
		ads = (struct acpi_dmar_device_scope *)cp;
		if ((ads->entry_type != ACPI_DMAR_SCOPE_TYPE_NOT_USED) &&
			(ads->entry_type < ACPI_DMAR_SCOPE_TYPE_RESERVED)) {
			dev_scope++;
		} else {
			pr_dbg("drhd: skip dev_scope type %d", ads->entry_type);
		}
	}

	return 0;
}

int32_t parse_dmar_table(struct dmar_info *plat_dmar_info)
{
	struct acpi_table_dmar *dmar_tbl;
	struct acpi_dmar_header *dmar_header;
	struct acpi_dmar_hardware_unit *acpi_drhd;
	char *ptr, *ptr_end;
	uint32_t include_all_idx = ~0U;
	uint16_t segment = 0;

	dmar_tbl = (struct acpi_table_dmar *)get_dmar_table();
	ASSERT(dmar_tbl != NULL, "");

	ptr = (char *)dmar_tbl + sizeof(*dmar_tbl);
	ptr_end = (char *)dmar_tbl + dmar_tbl->header.length;

	plat_dmar_info->drhd_units = drhd_info_array;
	for (; ptr < ptr_end; ptr += dmar_header->length) {
		dmar_header = (struct acpi_dmar_header *)ptr;
		ASSERT(dmar_header->length >= sizeof(struct acpi_dmar_header), "corrupted DMAR table");

		if (dmar_header->type == ACPI_DMAR_TYPE_HARDWARE_UNIT) {
			acpi_drhd = (struct acpi_dmar_hardware_unit *)dmar_header;
			/* Treat a valid DRHD has a non-zero base address */
			ASSERT(acpi_drhd->address != 0UL, "a zero base address DRHD. Please fix the BIOS.");

			if (dmar_unit_cnt == 0U) {
				segment = acpi_drhd->segment;
			} else {
				/* Only support single PCI Segment */
				if (segment != acpi_drhd->segment) {
					panic("Only support single PCI Segment.");
				}
			}

			if (acpi_drhd->flags & DRHD_FLAG_INCLUDE_PCI_ALL_MASK) {
				/* Check more than one DRHD with INCLUDE_PCI_ALL flag ? */
				include_all_idx = dmar_unit_cnt;
			}

			dmar_unit_cnt++;
			plat_dmar_info->drhd_units[dmar_unit_cnt - 1].devices = drhd_dev_scope[dmar_unit_cnt - 1];
			handle_one_drhd(acpi_drhd, &(plat_dmar_info->drhd_units[dmar_unit_cnt - 1]));
		}
	}

	if ((include_all_idx != ~0U) && (dmar_unit_cnt != (include_all_idx + 1U))) {
		pr_err("DRHD%d with INCLUDE_PCI_ALL flag is NOT the last one. Please fix the BIOS.", include_all_idx);
	}

	ASSERT(dmar_unit_cnt <= MAX_DRHDS, "parsed dmar_unit_cnt > MAX_DRHDS");
	plat_dmar_info->drhd_count = dmar_unit_cnt;

	return 0;
}
