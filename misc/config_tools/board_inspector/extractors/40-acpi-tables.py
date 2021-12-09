# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#


import logging
import re, os, fcntl, errno
import lxml.etree
from extractors.helpers import add_child, get_node

from acpiparser import parse_apic, apic

DEFAULT_MAX_IOAPIC_LINES = 240

def extract_gsi_number(ioapic_node, apic_id):
    f = open("/dev/kmsg", 'r')
    fd = os.dup(f.fileno())
    f.close()

    ret = fcntl.fcntl(fd, fcntl.F_SETFL, os.O_NONBLOCK)
    if ret != 0:
        os.close(fd)
        add_child(ioapic_node, "gsi_number", DEFAULT_MAX_IOAPIC_LINES)
        return

    while True:
        try:
            line =  os.read(fd, 512).decode("utf-8")
            m = re.match(r"\s*\d+,\d+,\d+,-;IOAPIC\[[\d+]\]:\s+apic_id\s+{},\s+version\s+\d+,\s+address\s+0x[0-9a-f]+,\s+GSI\s+(\d+)-(\d+)".format(apic_id), line)
            if m:
                previous_max = int(m.group(1))
                current_max = int(m.group(2)) + 1
                add_child(ioapic_node, "gsi_number", str(current_max - previous_max))
                break
        except:
            add_child(ioapic_node, "gsi_number", DEFAULT_MAX_IOAPIC_LINES)
            break
    os.close(fd)

def extract_topology(ioapics_node, tables):
    for subtable in tables.interrupt_controller_structures:
        if subtable.subtype == apic.MADT_TYPE_IO_APIC:
            apic_id = subtable.io_apic_id
            ioapic_node = add_child(ioapics_node, "ioapic", None, id=hex(apic_id))
            add_child(ioapic_node, "address", hex(subtable.io_apic_addr))
            add_child(ioapic_node, "gsi_base", hex(subtable.global_sys_int_base))
            extract_gsi_number(ioapic_node, apic_id)

def extract(args, board_etree):
    try:
        tables = parse_apic()
    except Exception as e:
        logging.warning(f"Parse ACPI tables failed: {str(e)}")
        logging.warning(f"Will not extract information from ACPI tables")
        return

    ioapics_node = get_node(board_etree, "//ioapics")
    extract_topology(ioapics_node, tables)
