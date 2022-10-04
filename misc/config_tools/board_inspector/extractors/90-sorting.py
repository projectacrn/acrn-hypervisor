# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import lxml.etree

def getkey(child):
    def resource_subkey(node):
        typ = node.get("type")
        if typ in ["memory", "io_port"]:
            return int(node.get("min"), base=16)
        elif typ == "irq":
            first_irq = node.get("int").split(", ")[0]
            if first_irq:
                return int(node.get("int").split(", ")[0])
            else:
                return 0
        else:
            return 0

    def device_subkey(node):
        adr = node.get("address")
        if adr is not None:
            return int(adr, base=16)
        else:
            return 0xFFFFFFFF

    tags = ["vendor", "identifier", "subsystem_vendor", "subsystem_identifier", "class",
            "acpi_object", "compatible_id", "acpi_uid", "aml_template", "status",
            "resource", "capability", "interrupt_pin_routing", "dependency", "bus", "device",
            "physfn", "display"]

    if child.tag == "resource":
        return (tags.index(child.tag), child.get("type"), resource_subkey(child))
    elif child.tag == "device":
        return (tags.index(child.tag), device_subkey(child))
    else:
        return (tags.index(child.tag),)

def extract(args, board_etree):
    # Sort children of bus and device nodes
    bus_nodes = board_etree.xpath("//bus")
    for bus_node in bus_nodes:
        bus_node[:] = sorted(bus_node, key=getkey)
    device_nodes = board_etree.xpath("//device")
    for device_node in device_nodes:
        device_node[:] = sorted(device_node, key=getkey)
