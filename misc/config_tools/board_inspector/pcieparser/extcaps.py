# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import copy
import inspectorlib.cdata as cdata
from .header import MemoryBar32, MemoryBar64, IOBar, \
    PCIE_BAR_SPACE_MASK, PCIE_BAR_MEMORY_SPACE, PCIE_BAR_IO_SPACE, \
    PCIE_BAR_TYPE_MASK, PCIE_BAR_TYPE_32_BIT, PCIE_BAR_TYPE_64_BIT

class ExtendedCapability:
    # Capability names from PCI Express Base Specification, mostly Table 9-23
    _cap_names_ = {
        0x01: "Advanced Error Reporting",
        0x02: "Virtual Channel",
        0x03: "Device Serial Number",
        0x04: "Power Budgeting",
        0x05: "Root Complex Link Declaration",
        0x06: "Root Complex Internal Link Control",
        0x07: "Root Complex Event Collector Endpoint Association",
        0x08: "Multi-Function Virtual Channel",
        0x09: "Virtual Channel",
        0x0a: "RCRB Header",
        0x0b: "Vendor-Specific Extended",
        0x0c: "Configuration Access Correlation",
        0x0d: "ACS",
        0x0e: "ARI",
        0x0f: "ATS",
        0x10: "SR-IOV",
        0x11: "MR-IOV",
        0x12: "Multicast",
        0x13: "PRI",
        0x15: "Resizable BAR",
        0x16: "DPA",
        0x17: "TPH Requester",
        0x18: "LTR",
        0x19: "Secondary PCI Express",
        0x1a: "PMUX",
        0x1b: "PASID",
        0x1c: "LNR",
        0x1d: "DPC",
        0x1e: "L1 PM Substates",
        0x1f: "TPM",
        0x20: "M-PCIe",
        0x21: "FRS Queueing",
        0x22: "Readiness Time Reporting",
        0x23: "Designated Vendor-Specific",
        0x24: "VF Resizable BAR",
        0x25: "Data Link Feature",
        0x26: "Physical Layer 16.0 GT/s",
        0x27: "Lane Margining at the Receiver",
        0x28: "Hierarchy ID",
        0x29: "NPEM",
        0x2a: "Physical Layer 32.0 GT/s",
        0x2b: "Alternate Protocol",
        0x2c: "SFI",
    }

    @property
    def name(self):
        if self.id in self._cap_names_.keys():
            return self._cap_names_[self.id]
        else:
            return f"Reserved Extended ({hex(self.id)})"

    @property
    def next_cap_ptr(self):
        # Classes inherit ExtendedCapability must implement the attribute next_cap_ptr_raw
        return self.next_cap_ptr_raw & 0xffc

class ExtendedCapabilityListRegister(cdata.Struct, ExtendedCapability):
    _pack_ = 1
    _fields_ = [
        ('id', ctypes.c_uint32, 16),
        ('version', ctypes.c_uint32, 4),
        ('next_cap_ptr_raw', ctypes.c_uint32, 12),
    ]

# SR-IOV (0x10)

class SRIOVBase(cdata.Struct, ExtendedCapability):
    _pack_ = 1
    _fields_ = copy.copy(ExtendedCapabilityListRegister._fields_) + [
        # SR-IOV Capabilities Register
        ('vf_migration_capable', ctypes.c_uint32, 1),
        ('ari_capable_hierarchy_preserved', ctypes.c_uint32, 1),
        ('vf_10_bit_tag_requester_supported', ctypes.c_uint32, 1),
        ('reserved1', ctypes.c_uint32, 18),
        ('vf_migration_interrupt_message_number', ctypes.c_uint32, 11),

        # SR-IOV Control Register
        ('vf_enable', ctypes.c_uint32, 1),
        ('vf_migration_enable', ctypes.c_uint32, 1),
        ('vf_migration_interrupt_enable', ctypes.c_uint32, 1),
        ('vf_mse', ctypes.c_uint32, 1),
        ('ari_capable_hierarchy', ctypes.c_uint32, 1),
        ('vf_10_bit_tag_requester_enable', ctypes.c_uint32, 1),
        ('reserved2', ctypes.c_uint32, 10),

        # SR-IOV Status Register
        ('vf_migration_status', ctypes.c_uint32, 1),
        ('reserved3', ctypes.c_uint32, 15),

        ('initial_vfs', ctypes.c_uint16),
        ('total_vfs', ctypes.c_uint16),
        ('num_vfs', ctypes.c_uint16),
        ('function_dependency_link', ctypes.c_uint8),
        ('reserved4', ctypes.c_uint8),
        ('first_vf_offset', ctypes.c_uint16),
        ('vf_stride', ctypes.c_uint16),
        ('reserved5', ctypes.c_uint16),
        ('vf_device_id', ctypes.c_uint16),

        ('supported_page_sizes', ctypes.c_uint32),
        ('system_page_size', ctypes.c_uint32),
    ]

def SRIOV_factory(addr):
    vf_bars_list = list()
    bar_base = addr + ctypes.sizeof(SRIOVBase)
    bar_addr = bar_base
    bar_end = bar_base + 0x18
    while bar_addr < bar_end:
        bar = ctypes.c_uint32.from_address(bar_addr).value
        idx = int((bar_addr - bar_base) / 4)
        if (bar & PCIE_BAR_SPACE_MASK) == PCIE_BAR_MEMORY_SPACE:
            if (bar & PCIE_BAR_TYPE_MASK) == PCIE_BAR_TYPE_64_BIT:
                vf_bars_list.append((f"vf_bar{idx}", MemoryBar64))
                bar_addr += 0x8
            else:
                vf_bars_list.append((f"vf_bar{idx}", MemoryBar32))
                bar_addr += 0x4
        else:
            vf_bars_list.append((f"vf_bar{idx}", IOBar))
            bar_addr += 0x4

    class SRIOV(cdata.Struct, ExtendedCapability):
        class VFBars(cdata.Struct):
            _pack_ = 1
            _fields_ = vf_bars_list

            def __iter__(self):
                for f in self._fields_:
                    yield getattr(self, f[0])

        _pack_ = 1
        _fields_ = copy.copy(SRIOVBase._fields_) + [
            ('vf_bars', VFBars),
            ('vf_migration_state_array_offset', ctypes.c_uint32),
        ]

    return SRIOV

def parse_sriov(buf, cap_ptr):
    return SRIOV_factory(ctypes.addressof(buf) + cap_ptr).from_buffer_copy(buf, cap_ptr)

# Module API

capability_parsers = {
    0x10: parse_sriov,
}

def extended_capabilities(data):
    buf = ctypes.create_string_buffer(data, len(data))
    cap_ptr = 0x100

    acc = list()
    while cap_ptr != 0:
        caplist = ExtendedCapabilityListRegister.from_buffer_copy(buf, cap_ptr)
        if caplist.id in capability_parsers.keys():
            acc.append(capability_parsers[caplist.id](buf, cap_ptr))
        elif caplist.id != 0:
            acc.append(caplist)
        cap_ptr = caplist.next_cap_ptr

    return acc
