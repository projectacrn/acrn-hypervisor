/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20190703 (64-bit version)
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
}

