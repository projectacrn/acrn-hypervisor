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

static void ipu_camera_write_dsdt(struct acpi_device *adev)
{
	/* CAM1 */

	dsdt_line("Scope (PCI0.I2C0) {");
	dsdt_line("    Device (CAM1)");
	dsdt_line("    {");
	dsdt_line("        Name (_ADR, Zero)  // _ADR: Address");
	dsdt_line("        Name (_HID, \"ADV7481A\")  // _HID: Hardware ID");
	dsdt_line("        Name (_CID, \"ADV7481A\")  // _CID: Compatible ID");
	dsdt_line("        Name (_UID, One)  // _UID: Unique ID");

	dsdt_line("        Method (_CRS, 0, Serialized)");
	dsdt_line("        {");
	dsdt_line("            Name (SBUF, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("                GpioIo (Exclusive, PullDefault, 0x0000, "
					"0x0000, IoRestrictionInputOnly,");
	dsdt_line("                    \"\\\\_SB.GPO0\", 0x00, "
					"ResourceConsumer, ,");
	dsdt_line("                    )");
	dsdt_line("                    {   // Pin list");
	dsdt_line("                        0x001E");
	dsdt_line("                    }");
	dsdt_line("                I2cSerialBusV2 (0x0070, "
					"ControllerInitiated, 0x00061A80,");
	dsdt_line("                    AddressingMode7Bit, "
						"\"\\\\_SB.PCI0.I2C0\",");
	dsdt_line("                    0x00, ResourceConsumer, , Exclusive,");
	dsdt_line("                    )");
	dsdt_line("            })");
	dsdt_line("            Return (SBUF)");
	dsdt_line("        }");

	dsdt_line("        Method (_DSM, 4, NotSerialized)");
	dsdt_line("        {");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"377ba76a-f390-4aff-ab38-9b1bf33a3015\")))");
	dsdt_line("            {");
	dsdt_line("                Return (\"ADV7481A\")");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"ea3b7bd8-e09b-4239-ad6e-ed525f3f26ab\")))");
	dsdt_line("            {");
	dsdt_line("                Return (0x40)");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"8dbe2651-70c1-4c6f-ac87-a37cb46e4af6\")))");
	dsdt_line("            {");
	dsdt_line("                Return (0xFF)");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"26257549-9271-4ca4-bb43-c4899d5a4881\")))");
	dsdt_line("            {");
	dsdt_line("                If (Arg2 == One)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02)");
	dsdt_line("                }");
	dsdt_line("                If (Arg2 == 0x02)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02001000)");
	dsdt_line("                }");
	dsdt_line("                If (Arg2 == 0x03)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02000E01)");
	dsdt_line("                }");
	dsdt_line("            }");
	dsdt_line("            Return (Zero)");
	dsdt_line("        }");
	dsdt_line("    }");
	dsdt_line("");

	/* CAM2 */
	dsdt_line("    Device (CAM2)");
	dsdt_line("    {");
	dsdt_line("        Name (_ADR, Zero)  // _ADR: Address");
	dsdt_line("        Name (_HID, \"ADV7481B\")  // _HID: Hardware ID");
	dsdt_line("        Name (_CID, \"ADV7481B\")  // _CID: Compatible ID");
	dsdt_line("        Name (_UID, One)  // _UID: Unique ID");

	dsdt_line("        Method (_CRS, 0, Serialized)");
	dsdt_line("        {");
	dsdt_line("            Name (SBUF, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("                GpioIo (Exclusive, PullDefault, 0x0000, "
					"0x0000, IoRestrictionInputOnly,");
	dsdt_line("                    \"\\\\_SB.GPO0\", 0x00, "
					"ResourceConsumer, ,");
	dsdt_line("                    )");
	dsdt_line("                    {   // Pin list");
	dsdt_line("                        0x001E");
	dsdt_line("                    }");
	dsdt_line("                I2cSerialBusV2 (0x0071, "
					"ControllerInitiated, 0x00061A80,");
	dsdt_line("                    AddressingMode7Bit, "
						"\"\\\\_SB.PCI0.I2C0\",");
	dsdt_line("                    0x00, ResourceConsumer, , Exclusive,");
	dsdt_line("                    )");
	dsdt_line("            })");
	dsdt_line("            Return (SBUF)");
	dsdt_line("        }");

	dsdt_line("        Method (_DSM, 4, NotSerialized) ");
	dsdt_line("        {");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"377ba76a-f390-4aff-ab38-9b1bf33a3015\")))");
	dsdt_line("            {");
	dsdt_line("                Return (\"ADV7481B\")");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"ea3b7bd8-e09b-4239-ad6e-ed525f3f26ab\")))");
	dsdt_line("            {");
	dsdt_line("                Return (0x14)");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"8dbe2651-70c1-4c6f-ac87-a37cb46e4af6\")))");
	dsdt_line("            {");
	dsdt_line("                Return (0xFF)");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"26257549-9271-4ca4-bb43-c4899d5a4881\")))");
	dsdt_line("            {");
	dsdt_line("                If (Arg2 == One)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02)");
	dsdt_line("                }");
	dsdt_line("                If (Arg2 == 0x02)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02001000)");
	dsdt_line("                }");
	dsdt_line("                If (Arg2 == 0x03)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02000E01)");
	dsdt_line("                }");
	dsdt_line("            }");
	dsdt_line("            Return (Zero)");
	dsdt_line("        }");
	dsdt_line("    }");
	dsdt_line("}");
	dsdt_line("");
}

static struct acpi_device ipu_camera = {
	.name       = "IPU Camera",
	.bus_vendor = 0x8086,
	.bus_device = 0x5aac,
	.write_dsdt = ipu_camera_write_dsdt
};

DEFINE_ACPI_DEVICE(ipu_camera);

