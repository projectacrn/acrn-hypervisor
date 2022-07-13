# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import copy

import inspectorlib.cdata as cdata
from acpiparser._utils import TableHeader

class ASFSubtable(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('record_type', ctypes.c_uint8, 7),
        ('last_record', ctypes.c_uint8, 1),
        ('reserved', ctypes.c_uint8),
        ('record_length', ctypes.c_uint16),
    ]

def ASF_subtable_unknown_factory(data_len):
    class ASFSubtableUnknown(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(ASFSubtable._fields_) + [
            ('data', ctypes.c_uint8 * data_len),
        ]
    return ASFSubtableUnknown

class ASF_info_flags_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('smbus_support', ctypes.c_uint8, 1),
    ]

class ASF_info_flags(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint8),
        ('bits', ASF_info_flags_bits),
    ]

class fixed_smbus_address(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('ASF_compliant_device', ctypes.c_uint8, 1),
        ('address', ctypes.c_uint8, 7),
    ]

class ASF_info_record(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(ASFSubtable._fields_) + [
        ('min_watchdog_reset_value', ctypes.c_uint8),
        ('min_pollng_interval', ctypes.c_uint8),
        ('system_id', ctypes.c_uint16),
        ('iana_manufacturer_id', ctypes.c_uint8 * 4),
        ('flags', ASF_info_flags),
        ('reserved2', ctypes.c_uint8 * 3),
    ]

class ASF_ALERTDATA(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('device_address', ctypes.c_uint8),
        ('command', ctypes.c_uint8),
        ('data_mask', ctypes.c_uint8),
        ('compare_value', ctypes.c_uint8),
        ('event_sensor_type', ctypes.c_uint8),
        ('event_type', ctypes.c_uint8),
        ('event_offset', ctypes.c_uint8),
        ('event_source_type', ctypes.c_uint8),
        ('event_severity', ctypes.c_uint8),
        ('sendor_number', ctypes.c_uint8),
        ('entity', ctypes.c_uint8),
        ('entity_instance', ctypes.c_uint8),
    ]

def ASF_alrt_factory(num_alerts):
    class ASF_ALRT(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(ASFSubtable._fields_) + [
            ('assertion_event_mask', ctypes.c_uint8),
            ('deassertion_event_mask', ctypes.c_uint8),
            ('number_alerts', ctypes.c_uint8),
            ('array_element_length', ctypes.c_uint8),
            ('device_array', ASF_ALERTDATA * num_alerts),
        ]
    return ASF_ALRT

class ASF_CONTROLDATA(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('function', ctypes.c_uint8),
        ('device_address', ctypes.c_uint8),
        ('command', ctypes.c_uint8),
        ('data_value', ctypes.c_uint8),
    ]

def ASF_rctl_factory(num_controls):
    class ASF_RCTL(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(ASFSubtable._fields_) + [
            ('number_controls', ctypes.c_uint8),
            ('array_element_length', ctypes.c_uint8),
            ('reserved2', ctypes.c_uint16),
            ('control_array', ASF_CONTROLDATA * num_controls),
        ]
    return ASF_RCTL

class ASF_boot_options_capabilities_1_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('firmware_verbosity_screen_blank', ctypes.c_uint8, 1),
        ('power_button_lock', ctypes.c_uint8, 1),
        ('reset_button_lock', ctypes.c_uint8, 1),
        ('reserved_4_3', ctypes.c_uint8, 2),
        ('lock_keyboard', ctypes.c_uint8, 1),
        ('sleep_button_lock', ctypes.c_uint8, 1),
        ('reserved_7', ctypes.c_uint8, 1),
    ]

class ASF_boot_options_capabilities_1(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint8),
        ('bits', ASF_boot_options_capabilities_1_bits),
    ]

class ASF_boot_options_capabilities_2_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('reserved_2_0', ctypes.c_uint8, 3),
        ('user_password_bypass', ctypes.c_uint8, 1),
        ('forced_progress_events', ctypes.c_uint8, 1),
        ('firmware_verbosity_verbose', ctypes.c_uint8, 1),
        ('firmware_verbosity_quiet', ctypes.c_uint8, 1),
        ('configuration_data_reset', ctypes.c_uint8, 1),
    ]

class ASF_boot_options_capabilities_2(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint8),
        ('bits', ASF_boot_options_capabilities_2_bits),
    ]

class ASF_special_commands_2_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('force_pxe_boot', ctypes.c_uint8, 1),
        ('force_hard_drive_boot', ctypes.c_uint8, 1),
        ('force_hard_drive_safe_mode_boot', ctypes.c_uint8, 1),
        ('force_diagnostic_boot', ctypes.c_uint8, 1),
        ('force_cd_dvd_boot', ctypes.c_uint8, 1),
        ('reserved', ctypes.c_uint8, 3),
    ]

class ASF_special_commands_2(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint8),
        ('bits', ASF_special_commands_2_bits),
    ]

class ASF_system_capabilities_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('power_cycle_reset_only_on_secure_port', ctypes.c_uint8, 1),
        ('power_down_only_on_secure_port', ctypes.c_uint8, 1),
        ('power_on_only_on_secure_port', ctypes.c_uint8, 1),
        ('reset_only_on_secure_port', ctypes.c_uint8, 1),
        ('power_cycle_reset_on_compat_or_secure_port', ctypes.c_uint8, 1),
        ('power_down_on_compat_or_secure_port', ctypes.c_uint8, 1),
        ('power_on_via_compat_or_secure_port', ctypes.c_uint8, 1),
        ('reset_only_on_compat_or_secure_port', ctypes.c_uint8, 1),
    ]

class ASF_system_capabilities(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint8),
        ('bits', ASF_system_capabilities_bits),
    ]

class ASF_rmcp(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(ASFSubtable._fields_) + [
        ('boot_options_capabilities_1', ASF_boot_options_capabilities_1),
        ('boot_options_capabilities_2', ASF_boot_options_capabilities_2),
        ('boot_options_capabilities_3', ctypes.c_uint8),
        ('boot_options_capabilities_4', ctypes.c_uint8),
        ('special_commands_1', ctypes.c_uint8),
        ('special_commands_2', ASF_special_commands_2),
        ('system_capabilities', ASF_system_capabilities),
        ('completion_code', ctypes.c_uint8),
        ('iana', ctypes.c_uint8 * 4),
        ('special_command', ctypes.c_uint8),
        ('special_command_parameter', ctypes.c_uint8 * 2),
        ('boot_options', ctypes.c_uint8 * 2),
        ('oem_parameters', ctypes.c_uint8 * 2),
    ]

def ASF_addr_record_factory(num_devices):

    class ASF_addr_record(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(ASFSubtable._fields_) + [
            ('seeprom_address', ctypes.c_uint8),
            ('num_devices', ctypes.c_uint8),
            ('fixed_smbus_addresses', fixed_smbus_address * num_devices),
        ]
    return ASF_addr_record

def ASF_factory(field_list):
    class subtables(cdata.Struct):
        _pack_ = 1
        _fields_ = field_list

        def __iter__(self):
            for f in self._fields_:
                yield getattr(self, f[0])

    class ASF_v1(cdata.Struct):
        _pack_ = 1
        _fields_ = [
            ('header', TableHeader),
            ('information_records', subtables),
        ]

    return ASF_v1

ASF_INFO = 0
ASF_ALRT = 1
ASF_RCTL = 2
ASF_RMCP = 3
ASF_ADDR = 4

def ASF_subtable_list(addr, length):
    end = addr + length
    field_list = list()
    subtable_num = 0
    ASF_addr_record_base_len = ctypes.sizeof(ASF_addr_record_factory(0))
    ASF_alrt_base = ASF_alrt_factory(0)
    ASF_rctl_base = ASF_rctl_factory(0)
    while addr < end:
        subtable_num += 1
        subtable = ASFSubtable.from_address(addr)
        if subtable.record_type == ASF_INFO:
            cls = ASF_info_record
        elif subtable.record_type == ASF_ALRT:
            num_alerts = ASF_alrt_base.from_address(addr).number_alerts
            cls = ASF_alrt_factory(num_alerts)
        elif subtable.record_type == ASF_RCTL:
            num_controls = ASF_rctl_base.from_address(addr).number_controls
            cls = ASF_rctl_factory(num_controls)
        elif subtable.record_type == ASF_RMCP:
            cls = ASF_rmcp
        elif subtable.record_type == ASF_ADDR:
            cls = ASF_addr_record_factory(subtable.record_length - ASF_addr_record_base_len)
        else:
            cls = (subtable.record_length - ctypes.sizeof(ASFSubtable))
        addr += subtable.record_length
        field_list.append( ('subtable{}'.format(subtable_num), cls) )
    return field_list

def ASF(val):
    """Create class based on decode of an ASF! table from filename."""
    base_length = ctypes.sizeof(ASF_factory(list()))
    data = open(val, mode='rb').read()
    buf = ctypes.create_string_buffer(data, len(data))
    addr = ctypes.addressof(buf)
    hdr = TableHeader.from_address(addr)
    field_list = ASF_subtable_list(addr + base_length, hdr.length - base_length)
    return ASF_factory(field_list).from_buffer_copy(data)
