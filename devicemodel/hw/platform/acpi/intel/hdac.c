/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "types.h"
#include "acpi.h"
#include "acpi_device.h"

static void hdac_write_dsdt(struct acpi_device *adev)
{
	dsdt_line("Scope (PCI0.I2C4) {");
	dsdt_line("    Device (HDAC)");
	dsdt_line("    {");
	dsdt_line("        Name (_HID, \"INT34C3\")  // _HID: Hardware ID");
	dsdt_line("        Name (_CID, \"INT34C3\")  // _CID: Compatible ID");
	dsdt_line("        Name (_DDN, \"Intel(R) Smart Sound Technology "
			"Audio Codec\")  // _DDN: DOS Device Name");
	dsdt_line("        Name (_UID, One)  // _UID: Unique ID");
	dsdt_line("        Method (_INI, 0, NotSerialized)");
	dsdt_line("        {");
	dsdt_line("        }");
	dsdt_line("");
	dsdt_line("        Method (_CRS, 0, NotSerialized)");
	dsdt_line("        {");
	dsdt_line("            Name (SBFB, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("                I2cSerialBusV2 (0x006C, "
					"ControllerInitiated, 0x00061A80,");
	dsdt_line("                    AddressingMode7Bit, "
						"\"\\\\_SB.PCI0.I2C4\",");
	dsdt_line("                    0x00, ResourceConsumer, , Exclusive,");
	dsdt_line("                    )");
	dsdt_line("            })");
	dsdt_line("            Name (SBFI, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("            })");
	dsdt_line("            Return (ConcatenateResTemplate (SBFB, SBFI))");
	dsdt_line("        }");
	dsdt_line("");
	dsdt_line("        Method (_STA, 0, NotSerialized)  // _STA: Status");
	dsdt_line("        {");
	dsdt_line("            Return (0x0F)");
	dsdt_line("        }");
	dsdt_line("    }");
	dsdt_line("}");
}

static struct acpi_device hdac = {
	.name       = "HDAC",
	.bus_vendor = 0x8086,
	.bus_device = 0x5ab4,
	.write_dsdt = hdac_write_dsdt
};

DEFINE_ACPI_DEVICE(hdac);

