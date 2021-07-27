#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os, re
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, lib.error, lib.lib
from collections import namedtuple

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

# A bar in pci hole must be above this threshold
# A bar's address below this threshold is for special purpose and should be preserved
PCI_HOLE_THRESHOLD = 0x100000

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
BAR2_SHEMEM_ALIGNMENT = 2 * common.SIZE_M

# Constants for pci vuart
PCI_VUART_VBAR0_SIZE = 4 * SIZE_K
PCI_VUART_VBAR1_SIZE = 4 * SIZE_K

# Constants for vmsix bar
VMSIX_VBAR_SIZE = 4 * SIZE_K

# Constants for tpm2 log area minimum length
LOG_AREA_MIN_LEN = 256 * SIZE_K

class MmioWindow(namedtuple(
        "MmioWindow", [
            "start",
            "end"])):

    PATTERN = re.compile(r"\s*(?P<start>[0-9a-f]+)-(?P<end>[0-9a-f]+) ")

    @classmethod
    def from_str(cls, value):
        if not isinstance(value, str):
            raise ValueError("value must be a str: {}".format(type(value)))

        match = cls.PATTERN.fullmatch(value)
        if match:
            return MmioWindow(
                start=int(match.group("start"), 16),
                end=int(match.group("end"), 16))
        else:
            raise ValueError("not an mmio window: {!r}".format(value))

    def overlaps(self, other):
        if not isinstance(other, MmioWindow):
            raise TypeError('overlaps() other must be an MmioWindow: {}'.format(type(other)))
        if other.end < self.start:
            return False
        if self.end < other.start:
            return False
        return True

def insert_vuart_to_dev_dict(scenario_etree, devdict_32bits):
    console_vuart =  scenario_etree.xpath(f"./console_vuart[base != 'INVALID_PCI_BASE']/@id")
    communication_vuarts = scenario_etree.xpath(f".//communication_vuart[base != 'INVALID_PCI_BASE']/@id")
    for vuart_id in console_vuart:
        devdict_32bits[(f"{VUART}_{vuart_id}", "bar0")] = PCI_VUART_VBAR0_SIZE
        devdict_32bits[(f"{VUART}_{vuart_id}", "bar1")] = PCI_VUART_VBAR1_SIZE
    for vuart_id in communication_vuarts:
        devdict_32bits[(f"{VUART}_{vuart_id}", "bar0")] = PCI_VUART_VBAR0_SIZE
        devdict_32bits[(f"{VUART}_{vuart_id}", "bar1")] = PCI_VUART_VBAR1_SIZE

def insert_ivsheme_to_dev_dict(scenario_etree, devdict_32bits, devdict_64bits, vm_id):
    shmem_regions = lib.lib.get_shmem_regions(scenario_etree)
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
        pt_dev_node = common.get_node(f"//bus[@type = 'pci' and @address = '{hex(bus)}']/device[@address = '{hex((dev << 16) | func)}']", board_etree)
        if pt_dev_node is not None:
            insert_vmsix_to_dev_dict(pt_dev_node, devdict_32bits)
            pt_dev_resources = pt_dev_node.xpath(".//resource[@type = 'memory' and @len != '0x0' and @id and @width]")
            for pt_dev_resource in pt_dev_resources:
                if int(pt_dev_resource.get('min'), 16) < PCI_HOLE_THRESHOLD:
                    continue
                dev_name = f"{PTDEV}_{bus:#04x}_{((dev << 16) | func):#08x}".upper()
                bar_len = pt_dev_resource.get('len')
                bar_region = pt_dev_resource.get('id')
                bar_width = pt_dev_resource.get('width')
                if bar_width == "32":
                    devdict_32bits[(f"{dev_name}", f"{bar_region}")] = int(bar_len, 16)
                else:
                    devdict_64bits[(f"{dev_name}", f"{bar_region}")] = int(bar_len, 16)

def insert_vmsix_to_dev_dict(pt_dev_node, devdict):
    """
    Allocate an unused mmio window for the first free bar region of a vmsix supported passthrough device.
    1. Check if this passtrhough device is in the list "KNOWN_CAPS_PCI_DEVS_DB" of "VMSIX" suppoeted device.
    2. Find the first unused region index for the vmsix bar.
    3. Allocate an unused mmio window for this bar.
    """
    vendor = common.get_node("./vendor/text()", pt_dev_node)
    identifier = common.get_node("./identifier/text()", pt_dev_node)
    if vendor is None or identifier is None:
        return

    if (vendor, identifier) in KNOWN_CAPS_PCI_DEVS_DB.get('VMSIX'):
        bar_regions = pt_dev_node.xpath(".//resource[@type = 'memory' and @width]")
        bar_32bits = [bar_region.get('id') for bar_region in bar_regions if bar_region.get('width') == '32']
        bar_32bits_idx_list = [int(bar.split('bar')[-1]) for bar in bar_32bits]
        bar_64bits = [bar_region.get('id') for bar_region in bar_regions if bar_region.get('width') == '64']
        bar_64bits_idx_list_1 = [int(bar.split('bar')[-1]) for bar in bar_64bits]
        bar_64bits_idx_list_2 = [idx + 1 for idx in bar_64bits_idx_list_1]
        used_bar_index = set(bar_32bits_idx_list + bar_64bits_idx_list_1 + bar_64bits_idx_list_2)
        unused_bar_index = [i for i in range(6) if i not in used_bar_index]
        try:
            next_bar_region = unused_bar_index.pop(0)
        except IndexError:
            raise lib.error.ResourceError(f"Cannot allocate a bar index for vmsix supported device: {vendor}:{identifier}, used bar idx list: {used_bar_index}")
        address = common.get_node("./@address", pt_dev_node)
        bus = common.get_node(f"../@address", pt_dev_node)
        if bus is not None and address is not None:
            dev_name = f"{PTDEV}_{int(bus, 16):#04x}_{int(address, 16):#08x}".upper()
            devdict[(f"{dev_name}", f"bar{next_bar_region}")] = VMSIX_VBAR_SIZE

def get_devs_mem_native(board_etree, mems):
    nodes = board_etree.xpath(f"//resource[@type = 'memory' and @len != '0x0' and @id and @width]")
    dev_list = []
    for node in nodes:
        start = node.get('min')
        end = node.get('max')
        if start is not None and end is not None:
            window = MmioWindow(int(start, 16), int(end, 16))
            for mem in mems:
                if window.start >= mem.start and window.end <= mem.end:
                    dev_list.append(window)
                    break
    return sorted(dev_list)

def get_devs_mem_passthrough(board_etree, scenario_etree):
    """
    Get all pre-launched vms' passthrough devices' mmio windows in native environment.
    return: list of passtrhough devices' mmio windows.
    """
    dev_list = []
    for vm_type in lib.lib.PRE_LAUNCHED_VMS_TYPE:
        pt_devs = scenario_etree.xpath(f"//vm[vm_type = '{vm_type}']/pci_devs/pci_dev/text()")
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
                dev_list.append(MmioWindow(int(start, 16), int(end, 16)))
    return dev_list

def get_pci_hole_native(board_etree):
    resources = board_etree.xpath(f"//bus[@type = 'pci']/device[@address]/resource[@type = 'memory' and @len != '0x0']")
    resources_hostbridge =  board_etree.xpath("//bus[@address = '0x0']/resource[@type = 'memory' and @len != '0x0' and not(@id) and not(@width)]")
    low_mem = set()
    high_mem = set()
    for resource_hostbridge in resources_hostbridge:
        start = resource_hostbridge.get('min')
        end = resource_hostbridge.get('max')
        if start is not None and end is not None and int(start, 16) >= PCI_HOLE_THRESHOLD:
            for resource in resources:
                resource_start = int(resource.get('min'), 16)
                resource_end = int(resource.get('max'), 16)
                if resource_start >= int(start, 16) and resource_end <= int(end, 16):
                    if resource_end < 4 * SIZE_G:
                        low_mem.add(MmioWindow(int(start, 16), int(end, 16)))
                        break
                    else:
                        high_mem.add(MmioWindow(int(start, 16), int(end, 16)))
                        break
    return list(sorted(low_mem)), list(sorted(high_mem))

def create_device_node(allocation_etree, vm_id, devdict):
    for dev in devdict:
        dev_name = dev[0]
        bar_region = dev[1].split('bar')[-1]
        bar_base = devdict.get(dev)

        vm_node = common.get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
        if vm_node is None:
            vm_node = common.append_node("/acrn-config/vm", None, allocation_etree, id = vm_id)
        dev_node = common.get_node(f"./device[@name = '{dev_name}']", vm_node)
        if dev_node is None:
            dev_node = common.append_node("./device", None, vm_node, name = dev_name)
        if common.get_node(f"./bar[@id='{bar_region}']", dev_node) is None:
            common.append_node(f"./bar", hex(bar_base), dev_node, id = bar_region)
        if IVSHMEM in dev_name and bar_region == '2':
            common.update_text(f"./bar[@id = '2']", hex(bar_base | PREFETCHABLE_BIT | MEMORY_BAR_LOCATABLE_64BITS), dev_node, True)

def create_native_pci_hole_node(allocation_etree, low_mem, high_mem):
    common.append_node("/acrn-config/hv/MMIO/MMIO32_START", hex(low_mem[0].start).upper(), allocation_etree)
    common.append_node("/acrn-config/hv/MMIO/MMIO32_END", hex(low_mem[0].end + 1).upper(), allocation_etree)
    if len(high_mem):
        common.append_node("/acrn-config/hv/MMIO/MMIO64_START", hex(high_mem[0].start).upper(), allocation_etree)
        common.append_node("/acrn-config/hv/MMIO/MMIO64_END", hex(high_mem[0].end + 1).upper(), allocation_etree)
        common.append_node("/acrn-config/hv/MMIO/HI_MMIO_START", hex(high_mem[0].start).upper(), allocation_etree)
        common.append_node("/acrn-config/hv/MMIO/HI_MMIO_END", hex(high_mem[0].end + 1).upper(), allocation_etree)
    else:
        common.append_node("/acrn-config/hv/MMIO/MMIO64_START", "~0".upper(), allocation_etree)
        common.append_node("/acrn-config/hv/MMIO/MMIO64_END", "~0", allocation_etree)
        common.append_node("/acrn-config/hv/MMIO/HI_MMIO_START", "~0".upper(), allocation_etree)
        common.append_node("/acrn-config/hv/MMIO/HI_MMIO_END", "0", allocation_etree)

def get_free_mmio(windowslist, used, size):
    if not size:
        raise ValueError(f"allocate size cannot be: {size}")
    if not windowslist:
        raise ValueError(f"No mmio range is specified:{windowslist}")

    alignment = max(VBAR_ALIGNMENT, size)
    for w in windowslist:
        new_w_start = common.round_up(w.start, alignment)
        window = MmioWindow(start = new_w_start, end = new_w_start + size - 1)
        for u in used:
            if window.overlaps(u):
                new_u_end = common.round_up(u.end + 1, alignment)
                window = MmioWindow(start = new_u_end, end = new_u_end + size - 1)
                continue
        if window.overlaps(w):
            return window
    raise lib.error.ResourceError(f"Not enough mmio window for a device size: {size}, free mmio windows: {windowslist}, used mmio windos{used}")

def alloc_mmio(mems, devdict, used_mem):
    devdict_list = sorted(devdict.items(), key = lambda t : t[1], reverse = True)
    devdict_base = {}
    for dev_bar in devdict_list:
        bar_name = dev_bar[0]
        bar_length = dev_bar[1]
        bar_window = get_free_mmio(mems, used_mem, bar_length)
        bar_end_addr = bar_window.start + bar_length - 1
        used_mem.append(MmioWindow(bar_window.start, bar_end_addr))
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
        insert_vuart_to_dev_dict(vm_node, devdict_32bits)
        insert_ivsheme_to_dev_dict(scenario_etree, devdict_32bits, devdict_64bits, vm_id)
        insert_pt_devs_to_dev_dict(board_etree, vm_node, devdict_32bits, devdict_64bits)

        low_mem = []
        high_mem = []
        used_low_mem = []
        used_high_mem = []

        vm_type = common.get_node("./vm_type/text()", vm_node)
        if vm_type is not None and lib.lib.is_pre_launched_vm(vm_type):
            low_mem = [MmioWindow(start = PRE_LAUNCHED_VM_LOW_MEM_START, end = PRE_LAUNCHED_VM_LOW_MEM_END - 1)]
            high_mem = [MmioWindow(start = PRE_LAUNCHED_VM_HIGH_MEM_START, end = PRE_LAUNCHED_VM_HIGH_MEM_END - 1)]
        elif vm_type is not None and lib.lib.is_sos_vm(vm_type):
            low_mem = native_low_mem
            high_mem = native_high_mem
            mem_passthrough = get_devs_mem_passthrough(board_etree, scenario_etree)
            used_low_mem_native = get_devs_mem_native(board_etree, low_mem)
            used_high_mem_native = get_devs_mem_native(board_etree, high_mem)
            # release the passthrough devices mmio windows from SOS
            used_low_mem = [mem for mem in used_low_mem_native if mem not in mem_passthrough]
            used_high_mem = [mem for mem in used_high_mem_native if mem not in mem_passthrough]
        else:
            # fall into else when the vm_type is post-launched vm, no mmio allocation is needed
            continue

        devdict_base_32_bits = alloc_mmio(low_mem, devdict_32bits, used_low_mem)
        devdict_base_64_bits = alloc_mmio(low_mem + high_mem, devdict_64bits, used_low_mem + used_high_mem)
        create_device_node(allocation_etree, vm_id, devdict_base_32_bits)
        create_device_node(allocation_etree, vm_id, devdict_base_64_bits)

def allocate_ssram_region(board_etree, scenario_etree, allocation_etree):
    # Guest physical address of the SW SRAM allocated to a pre-launched VM
    enabled = common.get_node("//PSRAM_ENABLED/text()", scenario_etree)
    if enabled == "y":
        pre_rt_vms = common.get_node("//vm[vm_type ='PRE_RT_VM']", scenario_etree)
        if pre_rt_vms is not None:
            vm_id = pre_rt_vms.get("id")
            l3_sw_sram = board_etree.xpath("//cache[@level='3']/capability[@id='Software SRAM']")
            if l3_sw_sram:
                start = min(map(lambda x: int(x.find("start").text, 16), l3_sw_sram))
                end = max(map(lambda x: int(x.find("end").text, 16), l3_sw_sram))

                allocation_vm_node = common.get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
                if allocation_vm_node is None:
                    allocation_vm_node = common.append_node("/acrn-config/vm", None, allocation_etree, id = vm_id)
                common.append_node("./ssram/start_gpa", hex(start), allocation_vm_node)
                common.append_node("./ssram/end_gpa", hex(end), allocation_vm_node)

def allocate_log_area(board_etree, scenario_etree, allocation_etree):
    tpm2_enabled = common.get_node(f"//vm[@id = '0']/mmio_resources/TPM2/text()", scenario_etree)
    if tpm2_enabled is None or tpm2_enabled == 'n':
        return

    if common.get_node("//capability[@id='log_area']", board_etree) is not None:
        # VIRT_ACPI_DATA_ADDR
        log_area_end_address = 0x7FFF0000
        log_area_start_address = log_area_end_address - LOG_AREA_MIN_LEN
        allocation_vm_node = common.get_node(f"/acrn-config/vm[@id = '0']", allocation_etree)
        if allocation_vm_node is None:
            allocation_vm_node = common.append_node("/acrn-config/vm", None, allocation_etree, id = '0')
        common.append_node("./log_area_start_address", hex(log_area_start_address).upper(), allocation_vm_node)
        common.append_node("./log_area_minimum_length", hex(LOG_AREA_MIN_LEN).upper(), allocation_vm_node)

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
 |                                                  |
 +--------------------------------------------------+ <--Offset 0

                  SOS VM gpa layout:
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
