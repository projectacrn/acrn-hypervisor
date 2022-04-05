# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import re
import logging
import lxml.etree

from pcieparser import parse_config_space
from pcieparser.header import IOBar, MemoryBar32, MemoryBar64
from extractors.helpers import add_child, get_node

SYS_DEVICES_PATH = "/sys/devices"
PCI_ROOT_PATH = "/sys/devices/pci0000:00"
bdf_regex = re.compile(r"^([0-9a-f]{4}):([0-9a-f]{2}):([0-9a-f]{2}).([0-7]{1})$")

interrupt_pin_names = {
    1: "INTA#",
    2: "INTB#",
    3: "INTC#",
    4: "INTD#",
}

def collect_hostbridge_resources(bus_node, bus_number):
    with open("/proc/iomem", "r") as f:
        for line in f.readlines():
            fields = line.strip().split(" : ")
            if fields[1] == f"PCI Bus 0000:{bus_number:02x}":
                begin, end = tuple(map(lambda x: int(f"0x{x}", base=16), fields[0].split("-")))
                add_child(bus_node, "resource", type="memory", min=hex(begin), max=hex(end), len=hex(end - begin + 1))

def parse_msi(cap_node, cap_struct):
    add_child(cap_node, "count", str(1 << cap_struct.multiple_message_capable))
    if cap_struct.multiple_message_capable > 0:
        add_child(cap_node, "capability", id="multiple-message")

    if cap_struct.address_64bit:
        add_child(cap_node, "capability", id="64-bit address")

    if cap_struct.per_vector_masking_capable:
        add_child(cap_node, "capability", id="per-vector masking")

def parse_msix(cap_node, cap_struct):
    add_child(cap_node, "table_size", str(cap_struct.table_size))
    add_child(cap_node, "table_bir", str(cap_struct.table_bir))
    add_child(cap_node, "table_offset", hex(cap_struct.table_offset_z))
    add_child(cap_node, "pba_bir", str(cap_struct.pba_bir))
    add_child(cap_node, "pba_offset", hex(cap_struct.pba_offset_z))

cap_parsers = {
    "MSI": parse_msi,
    "MSI-X": parse_msix,
}

def parse_device(bus_node, device_path):
    device_name = os.path.basename(device_path)
    cfg = parse_config_space(device_path)

    # There are cases where Linux creates device-like nodes without a file named "config", e.g. when there is a PCIe
    # non-transparent bridge (NTB) on the physical platform.
    if cfg is None:
        return None

    if device_name == "0000:00:00.0":
        device_node = bus_node
    else:
        m = bdf_regex.match(device_name)
        device, function = int(m.group(3), base=16), int(m.group(4), base=16)
        adr = hex((device << 16) + function)
        device_node = get_node(bus_node, f"./device[@address='{adr}']")
        if device_node is None:
            device_node = add_child(bus_node, "device", None, address=adr)

    for cap in cfg.caps:
        # If the device is not in D0, power it on and reparse its configuration space.
        if cap.name == "Power Management" and cap.power_state != 0:
            logging.info(f"Try resuming {device_path}")
            try:
                with open(os.path.join(device_path, "power", "control"), "w") as f:
                    f.write("on")
                cfg = parse_config_space(device_path)
            except Exception as e:
                logging.info(f"Resuming {device_path} failed: {str(e)}")

    # Device identifiers
    vendor_id = "0x{:04x}".format(cfg.header.vendor_id)
    device_id = "0x{:04x}".format(cfg.header.device_id)
    class_code = "0x{:06x}".format(cfg.header.class_code)
    if device_node.get("id") is None:
        device_node.set("id", device_id)
    add_child(device_node, "vendor", vendor_id)
    add_child(device_node, "identifier", device_id)
    add_child(device_node, "class", class_code)
    if cfg.header.header_type == 0:
        subvendor_id = "0x{:04x}".format(cfg.header.subsystem_vendor_id)
        subdevice_id = "0x{:04x}".format(cfg.header.subsystem_device_id)
        add_child(device_node, "subsystem_vendor", subvendor_id)
        add_child(device_node, "subsystem_identifier", subdevice_id)

    # BARs
    idx = 0
    for bar in cfg.header.bars:
        resource_path = os.path.join(device_path, f"resource{idx}")
        resource_type = bar.resource_type
        base = bar.base
        if os.path.exists(resource_path):
            if bar.base == 0:
                logging.debug(f"PCI {device_name}: BAR {idx} exists but is programmed with all 0. This device cannot be passed through to any VM.")
            else:
                resource_node = get_node(device_node, f"./resource[@type = '{resource_type}' and @min = '{hex(base)}']")
                if resource_node is None:
                    size = os.path.getsize(resource_path)
                    resource_node = add_child(device_node, "resource", None, type=resource_type, min=hex(base), max=hex(base + size - 1), len=hex(size))
                resource_node.set("id", f"bar{idx}")
                if isinstance(bar, MemoryBar32):
                    resource_node.set("width", "32")
                    resource_node.set("prefetchable", str(bar.prefetchable))
                elif isinstance(bar, MemoryBar64):
                    resource_node.set("width", "64")
                    resource_node.set("prefetchable", str(bar.prefetchable))
        elif bar.base != 0:
            logging.debug(f"PCI {device_name}: Cannot detect the size of BAR {idx}")
        if isinstance(bar, MemoryBar64):
            idx += 2
        else:
            idx += 1

    # Capabilities
    for cap in cfg.caps:
        cap_node = add_child(device_node, "capability", id=cap.name)
        if cap.name in cap_parsers:
            cap_parsers[cap.name](cap_node, cap)

    for cap in cfg.extcaps:
        cap_node = add_child(device_node, "capability", id=cap.name)
        if cap.name in cap_parsers:
            cap_parsers[cap.name](cap_node, cap)

    # Interrupt pin
    pin = cfg.header.interrupt_pin
    if pin > 0 and pin <= 4:
        pin_name = interrupt_pin_names[pin]
        res_node = add_child(device_node, "resource", type="interrupt_pin", pin=pin_name)

        prt_address = hex(int(device_node.get("address"), 16) | 0xffff)
        mapping = device_node.xpath(f"../interrupt_pin_routing/routing[@address='{prt_address}']/mapping[@pin='{pin_name}']")
        if len(mapping) > 0:
            res_node.set("source", mapping[0].get("source"))
            if "index" in mapping[0].keys():
                res_node.set("index", mapping[0].get("index"))

    # Secondary bus
    if cfg.header.header_type == 1:
        # According to section 3.2.5.6, PCI to PCI Bridge Architecture Specification, the I/O Limit register contains a
        # value smaller than the I/O Base register if there are no I/O addresses on the secondary side.
        io_base = (cfg.header.io_base_upper_16_bits << 16) | ((cfg.header.io_base >> 4) << 12)
        io_end = (cfg.header.io_limit_upper_16_bits << 16) | ((cfg.header.io_limit >> 4) << 12) | 0xfff
        if io_base <= io_end:
            add_child(device_node, "resource", type="io_port",
                      min=hex(io_base), max=hex(io_end), len=hex(io_end - io_base + 1))

        # According to section 3.2.5.8, PCI to PCI Bridge Architecture Specification, the Memory Limit register contains
        # a value smaller than the Memory Base register if there are no memory-mapped I/O addresses on the secondary
        # side.
        if cfg.header.memory_base <= cfg.header.memory_limit:
            memory_base = (cfg.header.memory_base >> 4) << 20
            memory_end = ((cfg.header.memory_limit >> 4) << 20) | 0xfffff
            add_child(device_node, "resource", type="memory",
                      min=hex(memory_base), max=hex(memory_end), len=hex(memory_end - memory_base + 1))

        secondary_bus_node = add_child(device_node, "bus", type="pci", address=hex(cfg.header.secondary_bus_number))

        # If a PCI routing table is provided for the root port / switch, move the routing table down to the bus node, in
        # order to align the relative position of devices and routing tables.
        prt = device_node.find("interrupt_pin_routing")
        if prt is not None:
            device_node.remove(prt)
            secondary_bus_node.append(prt)

        return secondary_bus_node

    return device_node

def enum_devices(bus_node, root_path):
    device_names = sorted(filter(lambda x:bdf_regex.match(x) != None, os.listdir(root_path)))
    for device_name in device_names:
        p = os.path.join(root_path, device_name)
        device_node = parse_device(bus_node, p)
        if device_node is not None:
            enum_devices(device_node, p)

def extract(args, board_etree):
    # Assume we only care about PCI devices under domain 0, as the hypervisor only uses BDF (without domain) for device
    # identification.
    root_regex = re.compile("pci0000:([0-9a-f]{2})")
    for root in filter(lambda x: x.startswith("pci"), os.listdir(SYS_DEVICES_PATH)):
        m = root_regex.match(root)
        if m:
            bus_number = int(m.group(1), 16)
            bus_node = get_node(board_etree, f"//bus[@type='pci' and @address='{hex(bus_number)}']")
            if bus_node is None:
                devices_node = get_node(board_etree, "//devices")
                bus_node = add_child(devices_node, "bus", type="pci", address=hex(bus_number))
                collect_hostbridge_resources(bus_node, bus_number)
            enum_devices(bus_node, os.path.join(SYS_DEVICES_PATH, root))
