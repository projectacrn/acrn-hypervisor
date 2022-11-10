#!/usr/bin/env python3
#
# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os, re
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import acrn_config_utilities, lib.error, lib.lib, math
from collections import namedtuple
from acrn_config_utilities import get_node

# VMSIX devices list
TSN_DEVS = [("0x8086", "0x4b30"), ("0x8086", "0x4b31"), ("0x8086", "0x4b32"), ("0x8086", "0x4ba0"),
            ("0x8086", "0x4ba1"), ("0x8086", "0x4ba2"), ("0x8086", "0x4bb0"), ("0x8086", "0x4bb1"),
            ("0x8086", "0x4bb2"), ("0x8086", "0xa0ac"), ("0x8086", "0x43ac"), ("0x8086", "0x43a2")]
GPIO_DEVS = [("0x8086", "0x4b88"), ("0x8086", "0x4b89")]

KNOWN_CAPS_PCI_DEVS_DB = {
    "VMSIX": TSN_DEVS + GPIO_DEVS,
}

# Constants for device name prefix
IVSHMEM = "IVSHMEM"
VUART = "VUART"
PTDEV = "PTDEV"
LEGACY_VUART = "LEGACY_VUART"

# A bar in pci hole must be above this threshold
# A bar's address below this threshold is for special purpose and should be preserved
PCI_HOLE_THRESHOLD = 0x100000

# IO port address common constants
# The valid io port address range will fall into [0x0, 0xFFFF]
# The address below 0xD00 are usually reserved for particular usage.
# For example: Configuration Space Address and Configuration Space Data
IO_PORT_MAX_ADDRESS = 0xFFFF
IO_PORT_THRESHOLD = 0xD00

# Common memory size units
SIZE_K = 1024
SIZE_M = SIZE_K * 1024
SIZE_G = SIZE_M * 1024

# Bar base alignment constant
VBAR_ALIGNMENT = 4 * SIZE_K

# Memory bar bits
PREFETCHABLE_BIT = 0x8
MEMORY_BAR_LOCATABLE_64BITS = 0x4

# Pre-launched VM MMIO windows constant
PRE_LAUNCHED_VM_LOW_MEM_START = 2 * SIZE_G
PRE_LAUNCHED_VM_LOW_MEM_END = 3.5 * SIZE_G
PRE_LAUNCHED_VM_HIGH_MEM_START = 256 * SIZE_G
PRE_LAUNCHED_VM_HIGH_MEM_END = 512 * SIZE_G

# Constants for ivshmem
BAR0_SHEMEM_SIZE = 4 * SIZE_K
BAR1_SHEMEM_SIZE = 4 * SIZE_K
BAR2_SHEMEM_ALIGNMENT = 2 * acrn_config_utilities.SIZE_M

# Constants for pci vuart
PCI_VUART_VBAR0_SIZE = 4 * SIZE_K
PCI_VUART_VBAR1_SIZE = 4 * SIZE_K

# Constants for legacy vuart
LEGACY_VUART_IO_PORT_SIZE = 0x10

# Constants for vmsix bar
VMSIX_VBAR_SIZE = 4 * SIZE_K

# Constant for VIRT_ACPI_NVS_ADDR
"""
VIRT_ACPI_NVS_ADDR and PRE_RTVM_SW_SRAM_END_GPA
need to be consistant with the layout of hypervisor\arch\x86\guest\ve820.c
"""
VIRT_ACPI_NVS_ADDR = 0x7FF00000
RESERVED_NVS_AREA = 0xB0000

PRE_RTVM_SW_SRAM_END_GPA = (0x7FDFB000 - 1)

class AddrWindow(namedtuple(
        "AddrWindow", [
            "start",
            "end"])):

    PATTERN = re.compile(r"\s*(?P<start>[0-9a-f]+)-(?P<end>[0-9a-f]+) ")

    @classmethod
    def from_str(cls, value):
        if not isinstance(value, str):
            raise ValueError("value must be a str: {}".format(type(value)))

        match = cls.PATTERN.fullmatch(value)
        if match:
            return AddrWindow(
                start=int(match.group("start"), 16),
                end=int(match.group("end"), 16))
        else:
            raise ValueError("not an address window: {!r}".format(value))

    def overlaps(self, other):
        if not isinstance(other, AddrWindow):
            raise TypeError('overlaps() other must be an AddrWindow: {}'.format(type(other)))
        if other.end < self.start:
            return False
        if self.end < other.start:
            return False
        return True

    def contains(self, other):
        if not isinstance(other, AddrWindow):
            raise TypeError('contains() other must be an AddrWindow: {}'.format(type(other)))
        if other.start >= self.start and other.end <= self.end:
            return True
        return False

def insert_vuart_to_dev_dict(scenario_etree, vm_id, devdict_32bits):

    console_vuart =  scenario_etree.xpath(f"./console_vuart[base != 'INVALID_PCI_BASE']/@id")
    for vuart_id in console_vuart:
        devdict_32bits[(f"{VUART}_{vuart_id}", "bar0")] = PCI_VUART_VBAR0_SIZE
        devdict_32bits[(f"{VUART}_{vuart_id}", "bar1")] = PCI_VUART_VBAR1_SIZE

    vm_name = get_node(f"//vm[@id = '{vm_id}']/name/text()", scenario_etree)
    communication_vuarts = scenario_etree.xpath(f"//vuart_connection[endpoint/vm_name/text() = '{vm_name}']")
    for vuart_id, vuart in enumerate(communication_vuarts, start=1):
        connection_type = get_node(f"./type/text()", vuart)
        if connection_type == "pci":
            devdict_32bits[(f"{VUART}_{vuart_id}", "bar0")] = PCI_VUART_VBAR0_SIZE
            devdict_32bits[(f"{VUART}_{vuart_id}", "bar1")] = PCI_VUART_VBAR1_SIZE

def insert_legacy_vuart_to_dev_dict(vm_node, devdict_io_port):
    legacy_vuart =  vm_node.xpath(f".//legacy_vuart[base = 'CONFIG_COM_BASE']/@id")
    for vuart_id in legacy_vuart:
        devdict_io_port[(f"{LEGACY_VUART}_{vuart_id}", "base")] = LEGACY_VUART_IO_PORT_SIZE

def insert_ivsheme_to_dev_dict(scenario_etree, devdict_32bits, devdict_64bits, vm_id):
    shmem_regions = lib.lib.get_ivshmem_regions_by_tree(scenario_etree)
    if vm_id not in shmem_regions:
        return
    shmems = shmem_regions.get(vm_id)
    for shm in shmems.values():
        try:
            int_size = int(shm.get('size')) * SIZE_M
        except:
            continue
        idx = shm.get('id')
        devdict_32bits[(f"{IVSHMEM}_{idx}", "bar0")] = BAR0_SHEMEM_SIZE
        devdict_32bits[(f"{IVSHMEM}_{idx}", "bar1")] = BAR1_SHEMEM_SIZE
        devdict_64bits[(f"{IVSHMEM}_{idx}", "bar2")] = int_size

def insert_pt_devs_to_dev_dict(board_etree, vm_node_etree, devdict_32bits, devdict_64bits):
    pt_devs = vm_node_etree.xpath(f".//pci_dev/text()")
    for pt_dev in pt_devs:
        bdf = pt_dev.split()[0]
        bus = int(bdf.split(':')[0], 16)
        dev = int(bdf.split(":")[1].split('.')[0], 16)
        func = int(bdf.split(":")[1].split('.')[1], 16)
        bdf = lib.lib.BusDevFunc(bus=bus, dev=dev, func=func)
        pt_dev_node = get_node(f"//bus[@type = 'pci' and @address = '{hex(bus)}']/device[@address = '{hex((dev << 16) | func)}']", board_etree)
        if pt_dev_node is not None:
            insert_vmsix_to_dev_dict(pt_dev_node, devdict_32bits)
            pt_dev_resources = pt_dev_node.xpath(".//resource[@type = 'memory' and @len != '0x0' and @id and @width]")
            for pt_dev_resource in pt_dev_resources:
                if int(pt_dev_resource.get('min'), 16) < PCI_HOLE_THRESHOLD:
                    continue
                dev_name = str(bdf)
                bar_len = pt_dev_resource.get('len')
                bar_region = pt_dev_resource.get('id')
                bar_width = pt_dev_resource.get('width')
                if bar_width == "32":
                    devdict_32bits[(f"{dev_name}", f"{bar_region}")] = int(bar_len, 16)
                else:
                    devdict_64bits[(f"{dev_name}", f"{bar_region}")] = int(bar_len, 16)

def get_pt_devs_io_port(board_etree, vm_node_etree):
    pt_devs = vm_node_etree.xpath(f".//pci_dev/text()")
    devdict = {}
    for pt_dev in pt_devs:
        bdf = pt_dev.split()[0]
        bus = int(bdf.split(':')[0], 16)
        dev = int(bdf.split(":")[1].split('.')[0], 16)
        func = int(bdf.split(":")[1].split('.')[1], 16)
        bdf = lib.lib.BusDevFunc(bus=bus, dev=dev, func=func)
        pt_dev_node = get_node(f"//bus[@type = 'pci' and @address = '{hex(bus)}']/device[@address = '{hex((dev << 16) | func)}']", board_etree)
        if pt_dev_node is not None:
            pt_dev_resources = pt_dev_node.xpath(".//resource[@type = 'io_port' and @id[starts-with(., 'bar')]]")
            for pt_dev_resource in pt_dev_resources:
                dev_name = str(bdf)
                bar_region = pt_dev_resource.get('id')
                devdict[(f"{dev_name}", f"{bar_region}")] = int(pt_dev_resource.get('min'), 16)
    return devdict

def insert_vmsix_to_dev_dict(pt_dev_node, devdict):
    """
    Allocate an unused mmio window for the first free bar region of a vmsix supported passthrough device.
    1. Check if this passtrhough device is in the list "KNOWN_CAPS_PCI_DEVS_DB" of "VMSIX" suppoeted device.
    2. Find the first unused region index for the vmsix bar.
    3. Allocate an unused mmio window for this bar.
    """
    vendor = get_node("./vendor/text()", pt_dev_node)
    identifier = get_node("./identifier/text()", pt_dev_node)
    if vendor is None or identifier is None:
        return

    if (vendor, identifier) in KNOWN_CAPS_PCI_DEVS_DB.get('VMSIX'):
        bar_regions = pt_dev_node.xpath(".//resource[@type = 'memory' and @width]")
        bar_32bits = [bar_region.get('id') for bar_region in bar_regions if bar_region.get('width') == '32']
        bar_32bits_idx_list = [int(bar.split('bar')[-1]) for bar in bar_32bits]
        bar_64bits = [bar_region.get('id') for bar_region in bar_regions if bar_region.get('width') == '64']
        bar_64bits_idx_list_1 = [int(bar.split('bar')[-1]) for bar in bar_64bits]
        bar_64bits_idx_list_2 = [idx + 1 for idx in bar_64bits_idx_list_1]

        bar_regions_io_port = pt_dev_node.xpath(".//resource[@type = 'io_port' and @id[starts-with(., 'bar')]]/@id")
        bar_io_port_idx_list = [int(bar.split('bar')[-1]) for bar in bar_regions_io_port]

        used_bar_index = set(bar_32bits_idx_list + bar_64bits_idx_list_1 + bar_64bits_idx_list_2 + bar_io_port_idx_list)
        unused_bar_index = [i for i in range(6) if i not in used_bar_index]
        try:
            next_bar_region = unused_bar_index.pop(0)
        except IndexError:
            raise lib.error.ResourceError(f"Cannot allocate a bar index for vmsix supported device: {vendor}:{identifier}, used bar idx list: {used_bar_index}")
        address = get_node("./@address", pt_dev_node)
        bus = get_node(f"../@address", pt_dev_node)
        if bus is not None and address is not None:
            bdf = lib.lib.BusDevFunc(bus=int(bus, 16), dev=int(address, 16) >> 16, func=int(address, 16) & 0xffff)
            dev_name = str(bdf)
            devdict[(f"{dev_name}", f"bar{next_bar_region}")] = VMSIX_VBAR_SIZE

def get_devs_mem_native(board_etree, mems):
    nodes = board_etree.xpath(f"//resource[@type = 'memory' and @len != '0x0' and @id and @width and @min and @max]")
    secondary_pci_nodes = board_etree.xpath(f"//resource[../bus[@type = 'pci'] and @type = 'memory' and @len != '0x0' and @min and @max]")
    secondary_pci_windows = list(set(AddrWindow(int(node.get('min'), 16), int(node.get('max'), 16)) for node in secondary_pci_nodes))
    dev_list = []

    for node in nodes:
        start = node.get('min')
        end = node.get('max')
        node_window = AddrWindow(int(start, 16), int(end, 16))
        if all(not(w.contains(node_window)) for w in secondary_pci_windows):
            dev_list.append(node_window)

    # check if there is any nested window
    for i in range(len(secondary_pci_windows)):
        secondary_pci_window = secondary_pci_windows[i]
        if all(not(w.contains(secondary_pci_window)) for w in (secondary_pci_windows[:i] + secondary_pci_windows[i + 1:])):
            dev_list.append(secondary_pci_window)

    # check if all the mmio window of dev_list fall into pci hole
    return_dev_list = [d for d in dev_list if any(mem.contains(d) for mem in mems)]
    return sorted(return_dev_list)

def get_devs_io_port_native(board_etree, io_port_range):
    nodes = board_etree.xpath(f"//device/resource[@type = 'io_port' and @len != '0x0' and @id]")
    dev_list = []
    for node in nodes:
        start = node.get('min')
        end = node.get('max')
        if start is not None and end is not None:
            window = AddrWindow(int(start, 16), int(end, 16))
            for range in io_port_range:
                if window.start >= range.start and window.end <= range.end:
                    dev_list.append(window)
                    break
    return sorted(dev_list)

def get_devs_mem_passthrough(board_etree, scenario_etree):
    """
    Get all pre-launched vms' passthrough devices' mmio windows in native environment.
    return: list of passtrhough devices' mmio windows.
    """
    dev_list = []
    pt_devs = scenario_etree.xpath(f"//vm[load_order = 'PRE_LAUNCHED_VM']/pci_devs/pci_dev/text()")
    for pt_dev in pt_devs:
        bdf = pt_dev.split()[0]
        bus = int(bdf.split(':')[0], 16)
        dev = int(bdf.split(":")[1].split('.')[0], 16)
        func = int(bdf.split(":")[1].split('.')[1], 16)
        resources = board_etree.xpath(f"//bus[@address = '{hex(bus)}']/device[@address = '{hex((dev << 16) | func)}'] \
                        /resource[@type = 'memory' and @len != '0x0' and @width]")
        for resource in resources:
            start = resource.get('min')
            end = resource.get('max')
            dev_list.append(AddrWindow(int(start, 16), int(end, 16)))
    return dev_list

def get_pt_devs_io_port_passthrough_per_vm(board_etree, vm_node):
    """
    Get all pre-launched vms' passthrough devices' io port addresses in native environment.
    return: list of passtrhough devices' io port addresses.
    """
    dev_list = []
    pt_devs = vm_node.xpath(f".//pci_devs/pci_dev/text()")
    for pt_dev in pt_devs:
        bdf = pt_dev.split()[0]
        bus = int(bdf.split(':')[0], 16)
        dev = int(bdf.split(":")[1].split('.')[0], 16)
        func = int(bdf.split(":")[1].split('.')[1], 16)
        resources = board_etree.xpath(f"//bus[@address = '{hex(bus)}']/device[@address = '{hex((dev << 16) | func)}'] \
                        /resource[@type = 'io_port' and @len != '0x0']")
        for resource in resources:
            start = resource.get('min')
            end = resource.get('max')
            dev_list.append(AddrWindow(int(start, 16), int(end, 16)))
    return dev_list

def get_pt_devs_io_port_passthrough(board_etree, scenario_etree):
    """
    Get all pre-launched vms' passthrough devices' io port addresses in native environment.
    return: list of passtrhough devices' io port addresses.
    """
    dev_list = []
    vm_nodes = scenario_etree.xpath(f"//vm[load_order = 'PRE_LAUNCHED_VM']")
    for vm_node in vm_nodes:
        dev_list_per_vm = get_pt_devs_io_port_passthrough_per_vm(board_etree, vm_node)
        dev_list = dev_list + dev_list_per_vm
    return dev_list

def get_pci_hole_native(board_etree):
    resources_hostbridge =  board_etree.xpath("//bus/resource[@type = 'memory' and @len != '0x0' and not(starts-with(@id, 'bar')) and not(@width)]")
    low_mem = set()
    high_mem = set()
    for resource_hostbridge in resources_hostbridge:
        start = resource_hostbridge.get('min')
        end = resource_hostbridge.get('max')
        if start is not None and end is not None and int(start, 16) >= PCI_HOLE_THRESHOLD:
            if int(end,16) < 4 * SIZE_G:
                low_mem.add(AddrWindow(int(start, 16), int(end, 16)))
            else:
                high_mem.add(AddrWindow(int(start, 16), int(end, 16)))
    return list(sorted(low_mem)), list(sorted(high_mem))

def get_io_port_range_native(board_etree):
    resources_hostbridge =  board_etree.xpath("//bus[@address = '0x0']/resource[@type = 'io_port' and @len != '0x0']")
    io_port_range_list = set()
    for resource_hostbridge in resources_hostbridge:
        start = resource_hostbridge.get('min')
        end = resource_hostbridge.get('max')
        if start is not None and end is not None and \
             int(start, 16) >= IO_PORT_THRESHOLD and int(end, 16) <= IO_PORT_MAX_ADDRESS:
                io_port_range_list.add(AddrWindow(int(start, 16), int(end, 16)))
    return list(sorted(io_port_range_list))

def create_device_node(allocation_etree, vm_id, devdict):
    for dev in devdict:
        dev_name = dev[0]
        bar_region = dev[1].split('bar')[-1]
        bar_base = devdict.get(dev)

        vm_node = get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
        if vm_node is None:
            vm_node = acrn_config_utilities.append_node("/acrn-config/vm", None, allocation_etree, id = vm_id)
        dev_node = get_node(f"./device[@name = '{dev_name}']", vm_node)
        if dev_node is None:
            dev_node = acrn_config_utilities.append_node("./device", None, vm_node, name = dev_name)
        if get_node(f"./bar[@id='{bar_region}']", dev_node) is None:
            acrn_config_utilities.append_node(f"./bar", hex(bar_base), dev_node, id = bar_region)
        if IVSHMEM in dev_name and bar_region == '2':
            acrn_config_utilities.update_text(f"./bar[@id = '2']", hex(bar_base | PREFETCHABLE_BIT | MEMORY_BAR_LOCATABLE_64BITS), dev_node, True)

def create_vuart_node(allocation_etree, vm_id, devdict):
    for dev in devdict:
        vuart_id = dev[0][-1]
        bar_base = devdict.get(dev)

        vm_node = get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
        if vm_node is None:
            vm_node = acrn_config_utilities.append_node("/acrn-config/vm", None, allocation_etree, id = vm_id)
        vuart_node = get_node(f"./legacy_vuart[@id = '{vuart_id}']", vm_node)
        if vuart_node is None:
            vuart_node = acrn_config_utilities.append_node("./legacy_vuart", None, vm_node, id = vuart_id)
        if get_node(f"./base", vuart_node) is None:
            acrn_config_utilities.append_node(f"./base", hex(bar_base), vuart_node)

def create_native_pci_hole_node(allocation_etree, low_mem, high_mem):
    acrn_config_utilities.append_node("/acrn-config/hv/MMIO/MMIO32_START", hex(low_mem[0].start).upper(), allocation_etree)
    acrn_config_utilities.append_node("/acrn-config/hv/MMIO/MMIO32_END", hex(low_mem[-1].end + 1).upper(), allocation_etree)
    if len(high_mem):
        acrn_config_utilities.append_node("/acrn-config/hv/MMIO/MMIO64_START", hex(high_mem[0].start).upper(), allocation_etree)
        acrn_config_utilities.append_node("/acrn-config/hv/MMIO/MMIO64_END", hex(high_mem[-1].end + 1).upper(), allocation_etree)
        acrn_config_utilities.append_node("/acrn-config/hv/MMIO/HI_MMIO_START", hex(high_mem[0].start).upper(), allocation_etree)
        acrn_config_utilities.append_node("/acrn-config/hv/MMIO/HI_MMIO_END", hex(high_mem[0].end + 1).upper(), allocation_etree)
    else:
        acrn_config_utilities.append_node("/acrn-config/hv/MMIO/MMIO64_START", "~0".upper(), allocation_etree)
        acrn_config_utilities.append_node("/acrn-config/hv/MMIO/MMIO64_END", "~0", allocation_etree)
        acrn_config_utilities.append_node("/acrn-config/hv/MMIO/HI_MMIO_START", "~0".upper(), allocation_etree)
        acrn_config_utilities.append_node("/acrn-config/hv/MMIO/HI_MMIO_END", "0", allocation_etree)

def get_free_addr(windowslist, used, size, alignment):
    if not size:
        raise ValueError(f"allocate size cannot be: {size}")
    if not windowslist:
        raise ValueError(f"No address range is specified:{windowslist}")

    alignment = max(alignment, size)
    for w in windowslist:
        new_w_start = acrn_config_utilities.round_up(w.start, alignment)
        window = AddrWindow(start = new_w_start, end = new_w_start + size - 1)
        for u in used:
            if window.overlaps(u):
                new_u_end = acrn_config_utilities.round_up(u.end + 1, alignment)
                window = AddrWindow(start = new_u_end, end = new_u_end + size - 1)
                continue
        if window.overlaps(w):
            return window
    raise lib.error.ResourceError(f"Not enough address window for a device size: {size}, free address windows: {windowslist}, used address windos{used}")

def alloc_addr(mems, devdict, used_mem, alignment):
    devdict_list = sorted(devdict.items(), key = lambda t : t[1], reverse = True)
    devdict_base = {}
    for dev_bar in devdict_list:
        bar_name = dev_bar[0]
        bar_length = dev_bar[1]
        bar_window = get_free_addr(mems, used_mem, bar_length, alignment)
        bar_end_addr = bar_window.start + bar_length - 1
        used_mem.append(AddrWindow(bar_window.start, bar_end_addr))
        used_mem.sort()
        devdict_base[bar_name] = bar_window.start
    return devdict_base

def allocate_pci_bar(board_etree, scenario_etree, allocation_etree):
    native_low_mem, native_high_mem = get_pci_hole_native(board_etree)
    create_native_pci_hole_node(allocation_etree, native_low_mem, native_high_mem)

    vm_nodes = scenario_etree.xpath("//vm")
    for vm_node in vm_nodes:
        vm_id = vm_node.get('id')

        devdict_32bits = {}
        devdict_64bits = {}
        insert_vuart_to_dev_dict(scenario_etree, vm_id, devdict_32bits)
        insert_ivsheme_to_dev_dict(scenario_etree, devdict_32bits, devdict_64bits, vm_id)
        insert_pt_devs_to_dev_dict(board_etree, vm_node, devdict_32bits, devdict_64bits)

        low_mem = []
        high_mem = []
        used_low_mem = []
        used_high_mem = []

        load_order = get_node("./load_order/text()", vm_node)
        if load_order is not None and lib.lib.is_pre_launched_vm(load_order):
            low_mem = [AddrWindow(start = PRE_LAUNCHED_VM_LOW_MEM_START, end = PRE_LAUNCHED_VM_LOW_MEM_END - 1)]
            high_mem = [AddrWindow(start = PRE_LAUNCHED_VM_HIGH_MEM_START, end = PRE_LAUNCHED_VM_HIGH_MEM_END - 1)]
        elif load_order is not None and lib.lib.is_service_vm(load_order):
            low_mem = native_low_mem
            high_mem = native_high_mem
            mem_passthrough = get_devs_mem_passthrough(board_etree, scenario_etree)
            used_low_mem_native = get_devs_mem_native(board_etree, low_mem)
            used_high_mem_native = get_devs_mem_native(board_etree, high_mem)
            # release the passthrough devices mmio windows from Service VM
            used_low_mem = [mem for mem in used_low_mem_native if mem not in mem_passthrough]
            used_high_mem = [mem for mem in used_high_mem_native if mem not in mem_passthrough]
        else:
            # fall into else when the load_order is post-launched vm, no mmio allocation is needed
            continue

        devdict_base_32_bits = alloc_addr(low_mem, devdict_32bits, used_low_mem, VBAR_ALIGNMENT)
        devdict_base_64_bits = alloc_addr(low_mem + high_mem, devdict_64bits, used_low_mem + used_high_mem, VBAR_ALIGNMENT)
        create_device_node(allocation_etree, vm_id, devdict_base_32_bits)
        create_device_node(allocation_etree, vm_id, devdict_base_64_bits)

def allocate_io_port(board_etree, scenario_etree, allocation_etree):
    io_port_range_list_native = get_io_port_range_native(board_etree)

    vm_nodes = scenario_etree.xpath("//vm")
    for vm_node in vm_nodes:
        vm_id = vm_node.get('id')

        devdict_io_port = {}
        insert_legacy_vuart_to_dev_dict(vm_node, devdict_io_port)

        io_port_range_list = []
        used_io_port_list = []

        load_order = get_node("./load_order/text()", vm_node)
        if load_order is not None and lib.lib.is_service_vm(load_order):
            io_port_range_list = io_port_range_list_native
            io_port_passthrough = get_pt_devs_io_port_passthrough(board_etree, scenario_etree)
            used_io_port_list_native = get_devs_io_port_native(board_etree, io_port_range_list_native)
            # release the passthrough devices io port address from Service VM
            used_io_port_list = [io_port for io_port in used_io_port_list_native if io_port not in io_port_passthrough]
        else:
            io_port_range_list = [AddrWindow(start = IO_PORT_THRESHOLD, end = IO_PORT_MAX_ADDRESS)]
            used_io_port_list = get_pt_devs_io_port_passthrough_per_vm(board_etree, vm_node)

        devdict_base_io_port = alloc_addr(io_port_range_list, devdict_io_port, used_io_port_list, 0)
        create_vuart_node(allocation_etree, vm_id, devdict_base_io_port)

def allocate_ssram_region(board_etree, scenario_etree, allocation_etree):
    # Guest physical address of the SW SRAM allocated to a pre-launched VM
    ssram_area_max_size = 0
    enabled = get_node("//SSRAM_ENABLED/text()", scenario_etree)
    if enabled == "y":
        pre_rt_vms = get_node("//vm[load_order = 'PRE_LAUNCHED_VM' and vm_type = 'RTVM']", scenario_etree)
        if pre_rt_vms is not None:
            vm_id = pre_rt_vms.get("id")
            l3_sw_sram = board_etree.xpath("//cache[@level='3']/capability[@id='Software SRAM']")
            if l3_sw_sram:
                # Calculate SSRAM area size. Containing all cache parts
                top = 0
                base = 0
                for ssram in board_etree.xpath("//cache/capability[@id='Software SRAM']"):
                    entry_base = int(get_node("./start/text()", ssram), 16)
                    entry_size = int(get_node("./size/text()", ssram))
                    top = (entry_base + entry_size) if top < (entry_base + entry_size) else top
                    base = entry_base if base == 0 or entry_base < base else base
                ssram_area_max_size = math.ceil((top - base)/0x1000) * 0x1000

            allocation_vm_node = get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
            if allocation_vm_node is None:
                allocation_vm_node = acrn_config_utilities.append_node("/acrn-config/vm", None, allocation_etree, id = vm_id)
            acrn_config_utilities.append_node("./ssram/start_gpa", hex(PRE_RTVM_SW_SRAM_END_GPA - ssram_area_max_size + 1), allocation_vm_node)
            acrn_config_utilities.append_node("./ssram/end_gpa", hex(PRE_RTVM_SW_SRAM_END_GPA), allocation_vm_node)
            acrn_config_utilities.append_node("./ssram/max_size", str(ssram_area_max_size), allocation_vm_node)

def allocate_log_area(board_etree, scenario_etree, allocation_etree):
    tpm2_enabled = get_node(f"//vm[@id = '0']/mmio_resources/TPM2/text()", scenario_etree)
    if tpm2_enabled is None or tpm2_enabled == 'n':
        return

    if get_node("//capability[@id='log_area']", board_etree) is not None:
        log_area_min_len_native = int(get_node(f"//log_area_minimum_length/text()", board_etree), 16)
        log_area_start_address = acrn_config_utilities.round_up(VIRT_ACPI_NVS_ADDR, 0x10000) + RESERVED_NVS_AREA
        allocation_vm_node = get_node(f"/acrn-config/vm[@id = '0']", allocation_etree)
        if allocation_vm_node is None:
            allocation_vm_node = acrn_config_utilities.append_node("/acrn-config/vm", None, allocation_etree, id = '0')
        acrn_config_utilities.append_node("./log_area_start_address", hex(log_area_start_address).upper(), allocation_vm_node)
        acrn_config_utilities.append_node("./log_area_minimum_length", hex(log_area_min_len_native).upper(), allocation_vm_node)

def pt_dev_io_port_passthrough(board_etree, scenario_etree, allocation_etree):
    vm_nodes = scenario_etree.xpath("//vm")
    for vm_node in vm_nodes:
        vm_id = vm_node.get('id')
        devdict_io_port = get_pt_devs_io_port(board_etree, vm_node)
        create_device_node(allocation_etree, vm_id, devdict_io_port)

"""
            Pre-launched VM gpa layout:
 +--------------------------------------------------+ <--End of VM high pci hole
 |      64 bits vbar of emulated PCI devices        |    Offset 0x8000000000
 +--------------------------------------------------+ <--Start of VM high pci hole
 |                                                  |    Offset 0x4000000000
...                                                ...
 |                                                  |
 +--------------------------------------------------+ <--End of VM low pci hole
 |   32 and 64 bits vbar of emulated PCI devices    |    Offset 0xE0000000
 +--------------------------------------------------+ <--Start of VM low pci hole
 |                                                  |    Offset 0x80000000
...                                                ...
 |            TPM2 log area at  0x7FFB0000          |
...                                                ...
 +--------------------------------------------------+ <--End of SSRAM area, at Offset 0x7FDFB000
 |            SSRAM area                            |
 +--------------------------------------------------+ <--Start of SSRAM area
 |                                                  |    (Depends on the host SSRAM area size)
...                                                ...
 |                                                  |
 +--------------------------------------------------+ <--Offset 0

                  Service VM gpa layout:
 +--------------------------------------------------+ <--End of native high pci hole
 |      64 bits vbar of emulated PCI devices        |
 +--------------------------------------------------+ <--Start of native high pci hole
 |                                                  |
...                                                ...
 |                                                  |
 +--------------------------------------------------+ <--End of native low pci hole
 |   32 and 64 bits vbar of emulated PCI devices    |
 +--------------------------------------------------+ <--Start of native low pci hole
 |                                                  |
...                                                ...
 |                                                  |
 |                                                  |
 |                                                  |
 +--------------------------------------------------+ <--Offset 0

"""
def fn(board_etree, scenario_etree, allocation_etree):
    allocate_ssram_region(board_etree, scenario_etree, allocation_etree)
    allocate_log_area(board_etree, scenario_etree, allocation_etree)
    allocate_pci_bar(board_etree, scenario_etree, allocation_etree)
    allocate_io_port(board_etree, scenario_etree, allocation_etree)
    pt_dev_io_port_passthrough(board_etree, scenario_etree, allocation_etree)
