# Copyright (C) 2020 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common
import hv_cfg_lib

class LogLevel:

    def __init__(self, hv_file):
        self.hv_file = hv_file
        self.npk = 0
        self.mem = 0
        self.console = 0

    def get_info(self):
        self.npk = common.get_hv_item_tag(self.hv_file, "DEBUG_OPTIONS", "NPK_LOGLEVEL")
        self.mem = common.get_hv_item_tag(self.hv_file, "DEBUG_OPTIONS", "MEM_LOGLEVEL")
        self.console = common.get_hv_item_tag(self.hv_file, "DEBUG_OPTIONS", "CONSOLE_LOGLEVEL")

    def check_item(self):
        hv_cfg_lib.hv_range_check(self.npk, "DEBUG_OPTIONS", "NPK_LOGLEVEL", hv_cfg_lib.RANGE_DB['LOG_LEVEL'])
        hv_cfg_lib.hv_range_check(self.mem, "DEBUG_OPTIONS", "MEM_LOGLEVEL", hv_cfg_lib.RANGE_DB['LOG_LEVEL'])
        hv_cfg_lib.hv_range_check(self.console, "DEBUG_OPTIONS", "CONSOLE_LOGLEVEL", hv_cfg_lib.RANGE_DB['LOG_LEVEL'])


class LogOpt:

    def __init__(self, hv_file):
        self.hv_file = hv_file
        self.dest = 0
        self.release = ''
        self.buf_size = 0
        self.level = LogLevel(self.hv_file)

    def get_info(self):
        self.release = common.get_hv_item_tag(self.hv_file, "DEBUG_OPTIONS", "RELEASE")
        self.dest = common.get_hv_item_tag(self.hv_file, "DEBUG_OPTIONS", "LOG_DESTINATION")
        self.buf_size = common.get_hv_item_tag(self.hv_file, "DEBUG_OPTIONS", "LOG_BUF_SIZE")
        self.level.get_info()

    def check_item(self):
        hv_cfg_lib.release_check(self.release, "DEBUG_OPTIONS", "RELEASE")
        hv_cfg_lib.hv_range_check(self.dest, "DEBUG_OPTIONS", "LOG_DESTINATION", hv_cfg_lib.RANGE_DB['LOG_DESTINATION_BITMAP'])
        hv_cfg_lib.hv_size_check(self.buf_size, "DEBUG_OPTIONS", "LOG_BUF_SIZE")
        self.level.check_item()


class CapHv:

    def __init__(self, hv_file):
        self.hv_file = hv_file
        self.max_emu_mmio_regions = 0
        self.max_pt_irq_entries = 0
        self.max_ioapic_num = 0
        self.max_ioapic_lines = 0
        self.max_ir_entries = 0
        self.iommu_bus_num = 0
        self.max_pci_dev_num = 0
        self.max_msix_table_num = 0

    def get_info(self):
        self.max_emu_mmio_regions = common.get_hv_item_tag(self.hv_file, "CAPACITIES", "MAX_EMULATED_MMIO")
        self.max_pt_irq_entries = common.get_hv_item_tag(self.hv_file, "CAPACITIES", "MAX_PT_IRQ_ENTRIES")
        self.max_ioapic_num = common.get_hv_item_tag(self.hv_file, "CAPACITIES", "MAX_IOAPIC_NUM")
        self.max_ioapic_lines = common.get_hv_item_tag(self.hv_file, "CAPACITIES", "MAX_IOAPIC_LINES")
        self.max_ir_entries = common.get_hv_item_tag(self.hv_file, "CAPACITIES", "MAX_IR_ENTRIES")
        self.iommu_bus_num = common.get_hv_item_tag(self.hv_file, "CAPACITIES", "IOMMU_BUS_NUM")
        self.max_pci_dev_num = common.get_hv_item_tag(self.hv_file, "CAPACITIES", "MAX_PCI_DEV_NUM")
        self.max_msix_table_num = common.get_hv_item_tag(self.hv_file, "CAPACITIES", "MAX_MSIX_TABLE_NUM")

    def check_item(self):
        hv_cfg_lib.hv_range_check(self.max_emu_mmio_regions, "CAPACITIES", "MAX_EMULATED_MMIO", hv_cfg_lib.RANGE_DB['EMULATED_MMIO_REGIONS'])
        hv_cfg_lib.hv_range_check(self.max_pt_irq_entries, "CAPACITIES", "MAX_PT_IRQ_ENTRIES", hv_cfg_lib.RANGE_DB['PT_IRQ_ENTRIES'])
        hv_cfg_lib.hv_range_check(self.max_ioapic_num, "CAPACITIES", "MAX_IOAPIC_NUM", hv_cfg_lib.RANGE_DB['IOAPIC_NUM'])
        hv_cfg_lib.hv_range_check(self.max_ioapic_lines, "CAPACITIES", "MAX_IOAPIC_LINES", hv_cfg_lib.RANGE_DB['IOAPIC_LINES'])
        hv_cfg_lib.ir_entries_check(self.max_ir_entries, "CAPACITIES", "MAX_IR_ENTRIES")
        hv_cfg_lib.hv_size_check(self.iommu_bus_num, "CAPACITIES", "IOMMU_BUS_NUM")
        hv_cfg_lib.hv_range_check(self.max_pci_dev_num, "CAPACITIES", "MAX_PCI_DEV_NUM", hv_cfg_lib.RANGE_DB['PCI_DEV_NUM'])
        hv_cfg_lib.max_msix_table_num_check(self.max_msix_table_num, "CAPACITIES", "MAX_MSIX_TABLE_NUM")


class MisCfg:

    def __init__(self, hv_file):
        self.hv_file = hv_file
        self.gpu_sbdf = 0

    def get_info(self):
        self.gpu_sbdf =  common.get_hv_item_tag(self.hv_file, "MISC_CFG", "GPU_SBDF")

    def check_item(self):
        hv_cfg_lib.hv_size_check(self.gpu_sbdf, "MISC_CFG", "GPU_SBDF")


class Features:
    def __init__(self, hv_file):
        self.hv_file = hv_file
        self.reloc = ''
        self.multiboot2 = ''
        self.rdt_enabled = ''
        self.cdp_enabled = ''
        self.cat_max_mask = []
        self.mba_delay = []
        self.scheduler = ''
        self.hyperv_enabled = ''
        self.iommu_enforce_snp = ''
        self.acpi_parse_enabled = ''
        self.l1d_flush_vmentry_enabled = ''
        self.mce_on_psc_workaround_disabled = ''
        self.psram_enabled = ''

    def get_info(self):
        self.multiboot2 = common.get_hv_item_tag(self.hv_file, "FEATURES", "MULTIBOOT2")
        self.rdt_enabled = common.get_hv_item_tag(self.hv_file, "FEATURES", "RDT", "RDT_ENABLED")
        self.cdp_enabled = common.get_hv_item_tag(self.hv_file, "FEATURES", "RDT", "CDP_ENABLED")
        self.cat_max_mask = common.get_hv_item_tag(self.hv_file, "FEATURES", "RDT", "CLOS_MASK")
        self.mba_delay = common.get_hv_item_tag(self.hv_file, "FEATURES", "RDT", "MBA_DELAY")
        self.scheduler = common.get_hv_item_tag(self.hv_file, "FEATURES", "SCHEDULER")
        self.reloc = common.get_hv_item_tag(self.hv_file, "FEATURES", "RELOC")
        self.hyperv_enabled = common.get_hv_item_tag(self.hv_file, "FEATURES", "HYPERV_ENABLED")
        self.acpi_parse_enabled = common.get_hv_item_tag(self.hv_file, "FEATURES", "ACPI_PARSE_ENABLED")
        self.l1d_flush_vmentry_enabled = common.get_hv_item_tag(self.hv_file, "FEATURES", "L1D_VMENTRY_ENABLED")
        self.mce_on_psc_workaround_disabled = common.get_hv_item_tag(self.hv_file, "FEATURES", "MCE_ON_PSC_DISABLED")
        self.iommu_enforce_snp = common.get_hv_item_tag(self.hv_file, "FEATURES", "IOMMU_ENFORCE_SNP")
        self.psram_enabled = common.get_hv_item_tag(self.hv_file, "FEATURES", "PSRAM", "PSRAM_ENABLED")

    def check_item(self):
        hv_cfg_lib.ny_support_check(self.multiboot2, "FEATURES", "MULTIBOOT2")
        hv_cfg_lib.ny_support_check(self.rdt_enabled, "FEATURES", "RDT", "RDT_ENABLED")
        hv_cfg_lib.ny_support_check(self.cdp_enabled, "FEATURES", "RDT", "CDP_ENABLED")
        hv_cfg_lib.cat_max_mask_check(self.cat_max_mask, "FEATURES", "RDT", "CLOS_MASK")
        hv_cfg_lib.mba_delay_check(self.mba_delay, "FEATURES", "RDT", "MBA_DELAY")
        hv_cfg_lib.scheduler_check(self.scheduler, "FEATURES", "SCHEDULER")
        hv_cfg_lib.ny_support_check(self.reloc, "FEATURES", "RELOC")
        hv_cfg_lib.ny_support_check(self.hyperv_enabled, "FEATURES", "HYPERV_ENABLED")
        hv_cfg_lib.ny_support_check(self.acpi_parse_enabled, "FEATURES", "ACPI_PARSE_ENABLED")
        hv_cfg_lib.ny_support_check(self.l1d_flush_vmentry_enabled, "FEATURES", "L1D_VMENTRY_ENABLED")
        hv_cfg_lib.ny_support_check(self.mce_on_psc_workaround_disabled, "FEATURES", "MCE_ON_PSC_DISABLED")
        hv_cfg_lib.ny_support_check(self.iommu_enforce_snp, "FEATURES", "IOMMU_ENFORCE_SNP")
        # hv_cfg_lib.ny_support_check(self.psram_enabled, "FEATURES", "PSRAM", "PSRAM_ENABLED")
        hv_cfg_lib.hv_psram_check(self.psram_enabled, self.cdp_enabled, "FEATURES", "PSRAM", "PSRAM_ENABLED")


class Memory:

    def __init__(self, hv_file):
        self.hv_file = hv_file
        self.stack_size = 0
        self.low_ram_size = 0
        self.hv_ram_start = 0
        self.hv_ram_size = 0
        self.platform_ram_size = 0
        self.sos_ram_size = 0
        self.uos_ram_size = 0
        self.ivshmem_enable = 'n'
        self.ivshmem_region = []

    def get_info(self):
        self.stack_size = common.get_hv_item_tag(self.hv_file, "MEMORY", "STACK_SIZE")
        self.low_ram_size = common.get_hv_item_tag(self.hv_file, "MEMORY", "LOW_RAM_SIZE")
        self.hv_ram_size = common.get_hv_item_tag(self.hv_file, "MEMORY", "HV_RAM_SIZE")
        self.hv_ram_start = common.get_hv_item_tag(self.hv_file, "MEMORY", "HV_RAM_START")
        self.platform_ram_size = common.get_hv_item_tag(self.hv_file, "MEMORY", "PLATFORM_RAM_SIZE")
        self.sos_ram_size = common.get_hv_item_tag(self.hv_file, "MEMORY", "SOS_RAM_SIZE")
        self.uos_ram_size = common.get_hv_item_tag(self.hv_file, "MEMORY", "UOS_RAM_SIZE")
        self.ivshmem_enable = common.get_hv_item_tag(self.hv_file, "FEATURES", "IVSHMEM", "IVSHMEM_ENABLED")
        self.ivshmem_region = common.get_hv_item_tag(self.hv_file, "FEATURES", "IVSHMEM", "IVSHMEM_REGION")

    def check_item(self):
        hv_cfg_lib.hv_size_check(self.stack_size, "MEMORY", "STACK_SIZE")
        hv_cfg_lib.hv_size_check(self.low_ram_size, "MEMORY", "LOW_RAM_SIZE")
        hv_cfg_lib.hv_size_check(self.platform_ram_size, "MEMORY", "PLATFORM_RAM_SIZE")
        hv_cfg_lib.hv_size_check(self.sos_ram_size, "MEMORY", "SOS_RAM_SIZE")
        hv_cfg_lib.hv_size_check(self.uos_ram_size, "MEMORY", "UOS_RAM_SIZE")
        hv_cfg_lib.ny_support_check(self.ivshmem_enable, "FEATURES", "IVSHMEM", "IVSHMEM_ENABLED")


class HvInfo:

    def __init__(self, hv_file):
        self.hv_file = hv_file
        self.mem = Memory(self.hv_file)
        self.cap = CapHv(self.hv_file)
        self.log = LogOpt(self.hv_file)
        self.mis = MisCfg(self.hv_file)
        self.features = Features(self.hv_file)

    def get_info(self):
        self.mem.get_info()
        self.log.get_info()
        self.cap.get_info()
        self.mis.get_info()
        self.features.get_info()

    def check_item(self):
        self.mem.check_item()
        self.log.check_item()
        self.cap.check_item()
        self.mis.check_item()
        self.features.check_item()
