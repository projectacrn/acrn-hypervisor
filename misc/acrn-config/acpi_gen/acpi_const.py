# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""constant for offline ACPI generator.

"""

import os, sys

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common

VM_CONFIGS_PATH = os.path.join(common.SOURCE_ROOT_DIR, 'misc', 'vm_configs')
TEMPLATE_ACPI_PATH = os.path.join(VM_CONFIGS_PATH, 'acpi', 'template')

ACPI_TABLE_LIST = [('rsdp.asl', 'rsdp.aml'), ('xsdt.asl', 'xsdt.aml'), ('facp.asl', 'facp.aml'),
                   ('mcfg.asl', 'mcfg.aml'), ('apic.asl', 'apic.aml'), ('tpm2.asl', 'tpm2.aml'),
                   ('dsdt.asl', 'dsdt.aml')]

ACPI_BASE = 0x7ff00000

ACPI_RSDP_ADDR_OFFSET = 0x0         # (36 bytes fixed)
ACPI_XSDT_ADDR_OFFSET = 0x80        # (36 bytes + 8*7 table addrs)
ACPI_FADT_ADDR_OFFSET = 0x100       # (244 bytes)
ACPI_DSDT_ADDR_OFFSET = 0x200       # (variable)
ACPI_MCFG_ADDR_OFFSET = 0x400       # (60 bytes)
ACPI_MADT_ADDR_OFFSET = 0x440       # (depends on #CPUs)
ACPI_TPM2_ADDR_OFFSET = 0x1100      # (52 bytes)

ACPI_RSDP_ADDR = (ACPI_BASE + ACPI_RSDP_ADDR_OFFSET)
ACPI_XSDT_ADDR = (ACPI_BASE + ACPI_XSDT_ADDR_OFFSET)
ACPI_FADT_ADDR = (ACPI_BASE + ACPI_FADT_ADDR_OFFSET)
ACPI_MCFG_ADDR = (ACPI_BASE + ACPI_MCFG_ADDR_OFFSET)
ACPI_MADT_ADDR = (ACPI_BASE + ACPI_MADT_ADDR_OFFSET)
ACPI_TPM2_ADDR = (ACPI_BASE + ACPI_TPM2_ADDR_OFFSET)
ACPI_DSDT_ADDR = (ACPI_BASE + ACPI_DSDT_ADDR_OFFSET)
ACPI_FACS_ADDR = 0x0

PM1A_EVN_LEN = 0x4
PM1A_CNT_LEN = 0x2
FADT_FLAGS = 0x00001125

VIRT_PCI_MMCFG_BASE = 0xE0000000

ACPI_MADT_TYPE_IOAPIC = 1
VIOAPIC_BASE = 0xFEC00000

ACPI_MADT_TYPE_LOCAL_APIC = 0
ACPI_MADT_TYPE_LOCAL_APIC_NMI = 4

TSN_DEVICE_LIST = ['8086:4ba0',
                   '8086:4bb0',
                   '8086:4b32']
