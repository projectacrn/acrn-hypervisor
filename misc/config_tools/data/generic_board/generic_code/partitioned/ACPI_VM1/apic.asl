/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20190703 (64-bit version)
 * Copyright (c) 2000 - 2022 Intel Corporation
 *
 * ACPI Data Table [APIC]
 *
 * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue
 */

[0004]                    Signature : "APIC"    [Multiple APIC Description Table (MADT)]
[0004]                 Table Length : 0000004E
[0001]                     Revision : 03
[0001]                     Checksum : 9B
[0006]                       Oem ID : "ACRN  "
[0008]                 Oem Table ID : "ACRNMADT"
[0004]                 Oem Revision : 00000001
[0004]              Asl Compiler ID : "INTL"
[0004]        Asl Compiler Revision : 20190703

[0004]           Local Apic Address : FEE00000
[0004]        Flags (decoded below) : 00000001
                PC-AT Compatibility : 1

[0001]                Subtable Type : 01 [I/O APIC]
[0001]                       Length : 0C
[0001]                  I/O Apic ID : 01
[0001]                     Reserved : 00
[0004]                      Address : FEC00000
[0004]                    Interrupt : 00000000

[0001]                Subtable Type : 04 [Local APIC NMI]
[0001]                       Length : 06
[0001]                 Processor ID : FF
[0002]        Flags (decoded below) : 0005
                           Polarity : 1
                       Trigger Mode : 1
[0001]         Interrupt Input LINT : 01

[0001]                Subtable Type : 00 [Processor Local APIC]
[0001]                       Length : 08
[0001]                 Processor ID : 00
[0001]                Local Apic ID : 02
[0004]        Flags (decoded below) : 00000001
                  Processor Enabled : 1
             Runtime Online Capable : 0

[0001]                Subtable Type : 00 [Processor Local APIC]
[0001]                       Length : 08
[0001]                 Processor ID : 01
[0001]                Local Apic ID : 06
[0004]        Flags (decoded below) : 00000001
                  Processor Enabled : 1
             Runtime Online Capable : 0
