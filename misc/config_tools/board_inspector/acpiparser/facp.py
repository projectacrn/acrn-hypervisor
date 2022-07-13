# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import copy

import inspectorlib.cdata as cdata
import inspectorlib.unpack as unpack
from acpiparser._utils import TableHeader, GAS

_preferred_pm_profile = {
    0: 'Unspecified',
    1: 'Desktop',
    2: 'Mobile',
    3: 'Workstation',
    4: 'Enterprise Server',
    5: 'SOHO Server',
    6: 'Appliance PC',
    7: 'Performance Server',
    8: 'Tablet'
}

class facp_flags_bits_v1(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('wbinvd', ctypes.c_uint32, 1),
        ('wbinvd_flush', ctypes.c_uint32, 1),
        ('proc_c1', ctypes.c_uint32, 1),
        ('p_lvl2_up', ctypes.c_uint32, 1),
        ('pwr_button', ctypes.c_uint32, 1),
        ('slp_button', ctypes.c_uint32, 1),
        ('fix_rtc', ctypes.c_uint32, 1),
        ('rtc_s4', ctypes.c_uint32, 1),
        ('tmr_val_ext', ctypes.c_uint32, 1),
        ('dck_cap', ctypes.c_uint32, 1),
    ]

class facp_flags_v1(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint32),
        ('bits', facp_flags_bits_v1),
    ]

class FACP_v1(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('header', TableHeader),
        ('firmware_ctrl', ctypes.c_uint32),
        ('dsdt', ctypes.c_uint32),
        ('int_model', ctypes.c_uint8),
        ('reserved0', ctypes.c_uint8),
        ('sci_int', ctypes.c_uint16),
        ('smi_cmd', ctypes.c_uint32),
        ('acpi_enable', ctypes.c_uint8),
        ('acpi_disable', ctypes.c_uint8),
        ('s4bios_req', ctypes.c_uint8),
        ('reserved1', ctypes.c_uint8),
        ('pm1a_evt_blk', ctypes.c_uint32),
        ('pm1b_evt_blk', ctypes.c_uint32),
        ('pm1a_cnt_blk', ctypes.c_uint32),
        ('pm1b_cnt_blk', ctypes.c_uint32),
        ('pm2_cnt_blk', ctypes.c_uint32),
        ('pm_tmr_blk', ctypes.c_uint32),
        ('gpe0_blk', ctypes.c_uint32),
        ('gpe1_blk', ctypes.c_uint32),
        ('pm1_evt_len', ctypes.c_uint8),
        ('pm1_cnt_len', ctypes.c_uint8),
        ('pm2_cnt_len', ctypes.c_uint8),
        ('pm_tmr_len', ctypes.c_uint8),
        ('gpe0_blk_len', ctypes.c_uint8),
        ('gpe1_blk_len', ctypes.c_uint8),
        ('gpe1_base', ctypes.c_uint8),
        ('reserved2', ctypes.c_uint8),
        ('p_lvl2_lat', ctypes.c_uint16),
        ('p_lvl3_lat', ctypes.c_uint16),
        ('flush_size', ctypes.c_uint16),
        ('flush_stride', ctypes.c_uint16),
        ('duty_offset', ctypes.c_uint8),
        ('duty_width', ctypes.c_uint8),
        ('day_alrm', ctypes.c_uint8),
        ('mon_alrm', ctypes.c_uint8),
        ('century', ctypes.c_uint8),
        ('reserved3', ctypes.c_uint8 * 3),
        ('flags', facp_flags_v1),
    ]

class facp_flags_bits_v3(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(facp_flags_bits_v1._fields_) + [
        ('reset_reg_sup', ctypes.c_uint32, 1),
        ('sealed_case', ctypes.c_uint32, 1),
        ('headless', ctypes.c_uint32, 1),
        ('cpu_sw_slp', ctypes.c_uint32, 1),
        ('pci_exp_wak', ctypes.c_uint32, 1),
        ('use_platform_clock', ctypes.c_uint32, 1),
        ('s4_rtc_sts_valid', ctypes.c_uint32, 1),
        ('remote_power_on_capable', ctypes.c_uint32, 1),
        ('force_apic_cluster_mode', ctypes.c_uint32, 1),
        ('force_apic_physical_destination_mode', ctypes.c_uint32, 1),
    ]

class facp_flags_v3(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint32),
        ('bits', facp_flags_bits_v3),
    ]

class facp_iapc_arch_bits_v3(cdata.Struct):
    _pack_ = 1
    _fields_ =  [
        ('legacy_devices', ctypes.c_uint16, 1),
        ('8042', ctypes.c_uint16, 1),
        ('vga_not_present', ctypes.c_uint16, 1),
        ('msi_not_supported', ctypes.c_uint16, 1),
    ]

class facp_iapc_arch_v3(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint16),
        ('bits', facp_iapc_arch_bits_v3),
    ]

class FACP_v3(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('header', TableHeader),
        ('firmware_ctrl', ctypes.c_uint32),
        ('dsdt', ctypes.c_uint32),
        ('reserved0', ctypes.c_uint8),
        ('preferred_pm_profile', ctypes.c_uint8),
        ('sci_int', ctypes.c_uint16),
        ('smi_cmd', ctypes.c_uint32),
        ('acpi_enable', ctypes.c_uint8),
        ('acpi_disable', ctypes.c_uint8),
        ('s4bios_req', ctypes.c_uint8),
        ('pstate_cnt', ctypes.c_uint8),
        ('pm1a_evt_blk', ctypes.c_uint32),
        ('pm1b_evt_blk', ctypes.c_uint32),
        ('pm1a_cnt_blk', ctypes.c_uint32),
        ('pm1b_cnt_blk', ctypes.c_uint32),
        ('pm2_cnt_blk', ctypes.c_uint32),
        ('pm_tmr_blk', ctypes.c_uint32),
        ('gpe0_blk', ctypes.c_uint32),
        ('gpe1_blk', ctypes.c_uint32),
        ('pm1_evt_len', ctypes.c_uint8),
        ('pm1_cnt_len', ctypes.c_uint8),
        ('pm2_cnt_len', ctypes.c_uint8),
        ('pm_tmr_len', ctypes.c_uint8),
        ('gpe0_blk_len', ctypes.c_uint8),
        ('gpe1_blk_len', ctypes.c_uint8),
        ('gpe1_base', ctypes.c_uint8),
        ('cst_cnt', ctypes.c_uint8),
        ('p_lvl2_lat', ctypes.c_uint16),
        ('p_lvl3_lat', ctypes.c_uint16),
        ('flush_size', ctypes.c_uint16),
        ('flush_stride', ctypes.c_uint16),
        ('duty_offset', ctypes.c_uint8),
        ('duty_width', ctypes.c_uint8),
        ('day_alrm', ctypes.c_uint8),
        ('mon_alrm', ctypes.c_uint8),
        ('century', ctypes.c_uint8),
        ('iapc_boot_arch', facp_iapc_arch_v3),
        ('reserved1', ctypes.c_uint8),
        ('flags', facp_flags_v3),
        ('reset_reg', GAS),
        ('reset_value', ctypes.c_uint8),
        ('reserved2', ctypes.c_uint8 * 3),
        ('x_firmware_ctrl', ctypes.c_uint64),
        ('x_dsdt', ctypes.c_uint64),
        ('x_pm1a_evt_blk', GAS),
        ('x_pm1b_evt_blk', GAS),
        ('x_pm1a_cnt_blk', GAS),
        ('x_pm1b_cnt_blk', GAS),
        ('x_pm2_cnt_blk', GAS),
        ('x_pm_tmr_blk', GAS),
        ('x_gpe0_blk', GAS),
        ('x_gpe1_blk', GAS),
    ]

    _formats = {
        'preferred_pm_profile': unpack.format_table("{}", _preferred_pm_profile),
    }

class facp_iapc_arch_bits_v4(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(facp_iapc_arch_bits_v3._fields_) + [
        ('pcie_aspm_controls', ctypes.c_uint16, 1),
    ]

class facp_iapc_arch_v4(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint16),
        ('bits', facp_iapc_arch_bits_v4),
    ]

class FACP_v4(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('header', TableHeader),
        ('firmware_ctrl', ctypes.c_uint32),
        ('dsdt', ctypes.c_uint32),
        ('reserved0', ctypes.c_uint8),
        ('preferred_pm_profile', ctypes.c_uint8),
        ('sci_int', ctypes.c_uint16),
        ('smi_cmd', ctypes.c_uint32),
        ('acpi_enable', ctypes.c_uint8),
        ('acpi_disable', ctypes.c_uint8),
        ('s4bios_req', ctypes.c_uint8),
        ('pstate_cnt', ctypes.c_uint8),
        ('pm1a_evt_blk', ctypes.c_uint32),
        ('pm1b_evt_blk', ctypes.c_uint32),
        ('pm1a_cnt_blk', ctypes.c_uint32),
        ('pm1b_cnt_blk', ctypes.c_uint32),
        ('pm2_cnt_blk', ctypes.c_uint32),
        ('pm_tmr_blk', ctypes.c_uint32),
        ('gpe0_blk', ctypes.c_uint32),
        ('gpe1_blk', ctypes.c_uint32),
        ('pm1_evt_len', ctypes.c_uint8),
        ('pm1_cnt_len', ctypes.c_uint8),
        ('pm2_cnt_len', ctypes.c_uint8),
        ('pm_tmr_len', ctypes.c_uint8),
        ('gpe0_blk_len', ctypes.c_uint8),
        ('gpe1_blk_len', ctypes.c_uint8),
        ('gpe1_base', ctypes.c_uint8),
        ('cst_cnt', ctypes.c_uint8),
        ('p_lvl2_lat', ctypes.c_uint16),
        ('p_lvl3_lat', ctypes.c_uint16),
        ('flush_size', ctypes.c_uint16),
        ('flush_stride', ctypes.c_uint16),
        ('duty_offset', ctypes.c_uint8),
        ('duty_width', ctypes.c_uint8),
        ('day_alrm', ctypes.c_uint8),
        ('mon_alrm', ctypes.c_uint8),
        ('century', ctypes.c_uint8),
        ('iapc_boot_arch', facp_iapc_arch_v4),
        ('reserved1', ctypes.c_uint8),
        ('flags', facp_flags_v3),
        ('reset_reg', GAS),
        ('reset_value', ctypes.c_uint8),
        ('reserved2', ctypes.c_uint8 * 3),
        ('x_firmware_ctrl', ctypes.c_uint64),
        ('x_dsdt', ctypes.c_uint64),
        ('x_pm1a_evt_blk', GAS),
        ('x_pm1b_evt_blk', GAS),
        ('x_pm1a_cnt_blk', GAS),
        ('x_pm1b_cnt_blk', GAS),
        ('x_pm2_cnt_blk', GAS),
        ('x_pm_tmr_blk', GAS),
        ('x_gpe0_blk', GAS),
        ('x_gpe1_blk', GAS),
    ]

    _formats = {
        'preferred_pm_profile': unpack.format_table("{}", _preferred_pm_profile),
    }

class facp_flags_bits_v5(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(facp_flags_bits_v3._fields_) + [
        ('hw_reduced_acpi', ctypes.c_uint32, 1),
        ('low_power_s0_idle_capable', ctypes.c_uint32, 1),
    ]

class facp_flags_v5(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint32),
        ('bits', facp_flags_bits_v5),
    ]

class facp_iapc_arch_bits_v5(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(facp_iapc_arch_bits_v4._fields_) + [
        ('cmos_rtc_not_present', ctypes.c_uint16, 1),
    ]

class facp_iapc_arch_v5(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint16),
        ('bits', facp_iapc_arch_bits_v5),
    ]

class FACP_v5(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('header', TableHeader),
        ('firmware_ctrl', ctypes.c_uint32),
        ('dsdt', ctypes.c_uint32),
        ('reserved0', ctypes.c_uint8),
        ('preferred_pm_profile', ctypes.c_uint8),
        ('sci_int', ctypes.c_uint16),
        ('smi_cmd', ctypes.c_uint32),
        ('acpi_enable', ctypes.c_uint8),
        ('acpi_disable', ctypes.c_uint8),
        ('s4bios_req', ctypes.c_uint8),
        ('pstate_cnt', ctypes.c_uint8),
        ('pm1a_evt_blk', ctypes.c_uint32),
        ('pm1b_evt_blk', ctypes.c_uint32),
        ('pm1a_cnt_blk', ctypes.c_uint32),
        ('pm1b_cnt_blk', ctypes.c_uint32),
        ('pm2_cnt_blk', ctypes.c_uint32),
        ('pm_tmr_blk', ctypes.c_uint32),
        ('gpe0_blk', ctypes.c_uint32),
        ('gpe1_blk', ctypes.c_uint32),
        ('pm1_evt_len', ctypes.c_uint8),
        ('pm1_cnt_len', ctypes.c_uint8),
        ('pm2_cnt_len', ctypes.c_uint8),
        ('pm_tmr_len', ctypes.c_uint8),
        ('gpe0_blk_len', ctypes.c_uint8),
        ('gpe1_blk_len', ctypes.c_uint8),
        ('gpe1_base', ctypes.c_uint8),
        ('cst_cnt', ctypes.c_uint8),
        ('p_lvl2_lat', ctypes.c_uint16),
        ('p_lvl3_lat', ctypes.c_uint16),
        ('flush_size', ctypes.c_uint16),
        ('flush_stride', ctypes.c_uint16),
        ('duty_offset', ctypes.c_uint8),
        ('duty_width', ctypes.c_uint8),
        ('day_alrm', ctypes.c_uint8),
        ('mon_alrm', ctypes.c_uint8),
        ('century', ctypes.c_uint8),
        ('iapc_boot_arch', facp_iapc_arch_v5),
        ('reserved1', ctypes.c_uint8),
        ('flags', facp_flags_v5),
        ('reset_reg', GAS),
        ('reset_value', ctypes.c_uint8),
        ('reserved2', ctypes.c_uint8 * 3),
        ('x_firmware_ctrl', ctypes.c_uint64),
        ('x_dsdt', ctypes.c_uint64),
        ('x_pm1a_evt_blk', GAS),
        ('x_pm1b_evt_blk', GAS),
        ('x_pm1a_cnt_blk', GAS),
        ('x_pm1b_cnt_blk', GAS),
        ('x_pm2_cnt_blk', GAS),
        ('x_pm_tmr_blk', GAS),
        ('x_gpe0_blk', GAS),
        ('x_gpe1_blk', GAS),
        ('sleep_control_reg', GAS),
        ('sleep_status_reg', GAS),
    ]

    _formats = {
        'preferred_pm_profile': unpack.format_table("{}", _preferred_pm_profile),
    }

def FACP(val):
    """Create class based on decode of an FACP table from filename."""
    data = open(val, mode='rb').read()
    buf = ctypes.create_string_buffer(data, len(data))
    addr = ctypes.addressof(buf)
    hdr = TableHeader.from_address(addr)
    if hdr.revision < 3:
        cls = FACP_v1
    elif hdr.revision == 3:
        cls = FACP_v3
    elif hdr.revision == 4:
        cls = FACP_v4
    else:
        cls = FACP_v5
    return cls.from_buffer_copy(data)
