# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import logging

from acpiparser import parse_dsdt, parse_tpm2
from acpiparser.aml.interpreter import ConcreteInterpreter
from acpiparser.aml.exception import UndefinedSymbol, FutureWork
from acpiparser.rdt import *

from extractors.helpers import add_child, get_node

def parse_eisa_id(eisa_id):
    chars = [
        (eisa_id & 0x7c) >> 2,                                # Bit 6:2 of the first byte
        ((eisa_id & 0x3) << 3) | ((eisa_id & 0xe000) >> 13),  # Bit 1:0 of the first byte and bit 7:5 of the second
        (eisa_id & 0x1F00) >> 8,                              # Bit 4:0 of the second byte
        (eisa_id & 0x00F00000) >> 20,                         # Bit 7:4 of the third byte
        (eisa_id & 0x000F0000) >> 16,                         # Bit 3:0 of the third byte
        (eisa_id & 0xF0000000) >> 28,                         # Bit 7:4 of the fourth byte
        (eisa_id & 0x0F000000) >> 24,                         # Bit 3:0 of the fourth byte
    ]
    if all(map(lambda x:x <= (ord('Z') - 0x40), chars[:3])):
        manufacturer = ''.join(map(lambda x: chr(x + 0x40), chars[:3]))
        product = ''.join(map(lambda x: "%X" % x, chars[3:6]))
        revision = "%X" % chars[6]
        return manufacturer + product + revision
    else:
        return None

predefined_nameseg = {
    "_SB_": ("bus", "system"),
    "_TZ_": ("thermalzone", None),
}

buses = {
    "PNP0A03": "pci",
    "PNP0A08": "pci",
}

def get_device_element(devices_node, namepath, hid):
    assert namepath.startswith("\\")
    namesegs = namepath[1:].split(".")

    element = devices_node
    for i,nameseg in enumerate(namesegs):
        buspath = f"\\{'.'.join(namesegs[:(i+1)])}"
        tag, typ = "device", None
        if nameseg in predefined_nameseg.keys():
            tag, typ = predefined_nameseg[nameseg]
        next_element = None
        for child in element:
            acpi_object = get_node(child, "acpi_object")
            if acpi_object is not None and acpi_object.text == buspath:
                next_element = child
                break
        if next_element is None:
            next_element = add_child(element, tag, None)
            add_child(next_element, "acpi_object", buspath)
            if typ:
                next_element.set("type", typ)
        element = next_element

    if hid:
        element.set("id", hid)
    return element

def parse_irq(item, elem):
    add_child(elem, "resource", type="irq", int=hex(item._INT))

def parse_io_port(item, elem):
    add_child(elem, "resource", type="io_port", min=hex(item._MIN), max=hex(item._MAX), len=hex(item._LEN))

def parse_fixed_io_port(item, elem):
    add_child(elem, "resource", type="io_port", min=hex(item._BAS), max=hex(item._BAS + item._LEN - 1), len=hex(item._LEN))

def parse_fixed_memory_range(item, elem):
    add_child(elem, "resource", type="memory", min=hex(item._BAS), max=hex(item._BAS + item._LEN - 1), len=hex(item._LEN))

def parse_address_space_resource(item, elem):
    if item._TYP == 0:
        typ = "memory"
    elif item._TYP == 1:
        typ = "io_port"
    elif item._TYP == 2:
        typ = "bus_number"
    else:
        typ = "custom"
    add_child(elem, "resource", type=typ, min=hex(item._MIN), max=hex(item._MAX), len=hex(item._LEN))

def parse_extended_irq(item, elem):
    for irq in item._INT:
        add_child(elem, "resource", type="irq", int=hex(irq))

def parse_tpm(elem):
    try:
        tpm2 = parse_tpm2()

        control_area = add_child(elem, "capability", None, id="control_area")
        add_child(control_area, "address_of_control_area", hex(tpm2.address_of_control_area))
        start_method = add_child(elem, "capability", None, id="start_method")
        add_child(start_method, "value", hex(tpm2.start_method))
        for parameter in tpm2.start_method_specific_parameters:
            add_child(start_method, "parameter", hex(parameter))
        if hasattr(tpm2, "log_area_minimum_length"):
            add_child(elem, "capability", None, id="log_area")
    except Exception as e:
        logging.info(f"Parse ACPI TPM2 failed: {str(e)}")
        logging.info(f"Will not extract information from ACPI TPM2")
        return

resource_parsers = {
    (0, SMALL_RESOURCE_ITEM_IRQ_FORMAT): parse_irq,
    (0, SMALL_RESOURCE_ITEM_IO_PORT): parse_io_port,
    (0, SMALL_RESOURCE_ITEM_FIXED_LOCATION_IO_PORT): parse_fixed_io_port,
    (0, SMALL_RESOURCE_ITEM_END_TAG): (lambda x,y: None),
    (1, LARGE_RESOURCE_ITEM_32BIT_FIXED_MEMORY_RANGE): parse_fixed_memory_range,
    (1, LARGE_RESOURCE_ITEM_ADDRESS_SPACE_RESOURCE): parse_address_space_resource,
    (1, LARGE_RESOURCE_ITEM_WORD_ADDRESS_SPACE): parse_address_space_resource,
    (1, LARGE_RESOURCE_ITEM_EXTENDED_INTERRUPT): parse_extended_irq,
    (1, LARGE_RESOURCE_ITEM_QWORD_ADDRESS_SPACE): parse_address_space_resource,
    (1, LARGE_RESOURCE_ITEM_EXTENDED_ADDRESS_SPACE): parse_address_space_resource,
}

def fetch_device_info(devices_node, interpreter, namepath):
    logging.info(f"Fetch information about device object {namepath}")

    try:
        # Check if an _INI method exists
        try:
            interpreter.interpret_method_call(namepath + "._INI")
        except UndefinedSymbol:
            pass

        sta = None
        if interpreter.context.has_symbol(f"{namepath}._STA"):
            sta = interpreter.interpret_method_call(f"{namepath}._STA").get()
            if sta & 0x1 == 0:
                return

        # Hardware ID
        hid = ""
        if interpreter.context.has_symbol(f"{namepath}._HID"):
            hid = interpreter.interpret_method_call(f"{namepath}._HID").get()
            if isinstance(hid, str):
                pass
            elif isinstance(hid, int):
                eisa_id = parse_eisa_id(hid)
                if eisa_id:
                    hid = eisa_id
                else:
                    hid = hex(hid)
            else:
                hid = "<unknown>"

        # Create the XML element for the device and create its ancestors if necessary
        element = get_device_element(devices_node, namepath, hid)
        if hid in buses.keys():
            element.tag = "bus"
            element.set("type", buses[hid])

        if hid == "MSFT0101":
            parse_tpm(element)

        # Address
        if interpreter.context.has_symbol(f"{namepath}._ADR"):
            adr = interpreter.interpret_method_call(f"{namepath}._ADR").get()
            if isinstance(adr, int):
                adr = hex(adr)
            if len(element.xpath(f"../*[@address='{adr}']")) > 0:
                logging.warning(f"{namepath} has siblings with duplicated address {adr}.")
            else:
                element.set("address", hex(adr) if isinstance(adr, int) else adr)

        # Status
        if sta is not None:
            status = add_child(element, "status")

            add_child(status, "present", "y" if sta & 0x1 != 0 else "n")
            add_child(status, "enabled", "y" if sta & 0x2 != 0 else "n")
            add_child(status, "functioning", "y" if sta & 0x8 != 0 else "n")

        # Resources
        if interpreter.context.has_symbol(f"{namepath}._CRS"):
            data = interpreter.interpret_method_call(f"{namepath}._CRS").get()
            rdt = parse_resource_data(data)

            for item in rdt.items:
                p = (item.type, item.name)
                if p in resource_parsers.keys():
                    resource_parsers[p](item, element)
                else:
                    add_child(element, "resource", type=item.__class__.__name__)

        # PCI interrupt routing
        if interpreter.context.has_symbol(f"{namepath}._PRT"):
            interpreter.interpret_method_call(f"{namepath}._PRT")

    except FutureWork:
        pass

def extract(board_etree):
    devices_node = get_node(board_etree, "//devices")

    try:
        namespace = parse_dsdt()
    except Exception as e:
        logging.info(f"Parse ACPI DSDT/SSDT failed: {str(e)}")
        logging.info(f"Will not extract information from ACPI DSDT/SSDT")
        return

    interpreter = ConcreteInterpreter(namespace)
    for device in sorted(namespace.devices, key=lambda x:x.name):
        try:
            fetch_device_info(devices_node, interpreter, device.name)
        except Exception as e:
            logging.info(f"Fetch information about device object {device.name} failed: {str(e)}")

advanced = True
