/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20190703 (64-bit version)
 * Copyright (c) 2000 - 2019 Intel Corporation
 *
 * Original Table Header:
 *     Signature        "DSDT"
 *     Length           0x00000051 (81)
 *     Revision         0x03
 *     Checksum         0xF0
 *     OEM ID           "ACRN  "
 *     OEM Table ID     "ACRNDSDT"
 *     OEM Revision     0x00000001 (1)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20190703 (538511107)
 */
DefinitionBlock ("", "DSDT", 3, "ACRN  ", "ACRNDSDT", 0x00000001)
{
    Scope (_SB)
    {
        Device (OTN1)
        {
            Name (_ADR, 0x00020000)  // _ADR: Address
            OperationRegion (TSRT, PCI_Config, Zero, 0x0100)
            Field (TSRT, AnyAcc, NoLock, Preserve)
            {
                DVID,   16,
                Offset (0x10),
                TADL,   32,
                TADH,   32
            }
        }

        Device (PCS2)
        {
            Name (_HID, "INTC1033")  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }

            Method (_CRS, 0, Serialized)  // _CRS: Current Resource Settings
            {
                Name (PCSR, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x00000004,         // Address Length
                        _Y00)
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x00000004,         // Address Length
                        _Y01)
                })
                CreateDWordField (PCSR, \_SB.PCS2._CRS._Y00._BAS, MAL0)  // _BAS: Base Address
                MAL0 = ((^^OTN1.TADL & 0xFFFFF000) + 0x0200)
                CreateDWordField (PCSR, \_SB.PCS2._CRS._Y01._BAS, MDL0)  // _BAS: Base Address
                MDL0 = ((^^OTN1.TADL & 0xFFFFF000) + 0x0204)
                Return (PCSR) /* \_SB_.PCS2._CRS.PCSR */
            }
        }
    }
    Device (TPM)
    {
        Name (_HID, "MSFT0101" /* TPM 2.0 Security Device */)  // _HID: Hardware ID
        Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
        {
            Memory32Fixed (ReadWrite,
                0xFED40000,         // Address Base
                0x00005000,         // Address Length
                )
        })
    }
  Name (_S5, Package ()
  {
      0x05,
      Zero,
  })
}

