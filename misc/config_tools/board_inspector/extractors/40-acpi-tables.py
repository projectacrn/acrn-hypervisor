# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#


import logging
import re, os, fcntl, errno
import lxml.etree
from extractors.helpers import add_child, get_node

import acpiparser

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
            add_child(ioapic_node, "gsi_number", str(DEFAULT_MAX_IOAPIC_LINES))
            break
    os.close(fd)

def extract_topology(ioapics_node, tables):
    for subtable in tables.interrupt_controller_structures:
        if subtable.subtype == acpiparser.apic.MADT_TYPE_IO_APIC:
            apic_id = subtable.io_apic_id
            ioapic_node = add_child(ioapics_node, "ioapic", None, id=hex(apic_id))
            add_child(ioapic_node, "address", hex(subtable.io_apic_addr))
            add_child(ioapic_node, "gsi_base", hex(subtable.global_sys_int_base))
            extract_gsi_number(ioapic_node, apic_id)

def extract_tcc_capabilities_from_rtct_v1(board_etree, rtct):
    caches_node = get_node(board_etree, "//caches")

    for entry in rtct.entries:
        if entry.type == acpiparser.rtct.ACPI_RTCT_V1_TYPE_SoftwareSRAM:
            cache_node = get_node(caches_node, f"cache[@level='{entry.cache_level}' and processors/processor='{hex(entry.apic_id_tbl[0])}']")
            if cache_node is None:
                logging.debug(f"Cannot find the level {entry.cache_level} cache of physical processor with apic ID {entry.apic_id_tbl[0]}")
                continue
            cap = add_child(cache_node, "capability", None, id="Software SRAM")
            add_child(cap, "start", "0x{:08x}".format(entry.base))
            add_child(cap, "end", "0x{:08x}".format(entry.base + entry.size - 1))
            add_child(cap, "size", str(entry.size))

def extract_tcc_capabilities_from_rtct_v2(board_etree, rtct):
    def get_or_create_ssram_node(cache_node):
        ssram_node = get_node(cache_node, "capability[@id = 'Software SRAM']")
        if ssram_node is None:
            ssram_node = add_child(cache_node, "capability", None, id="Software SRAM")
        return ssram_node

    caches_node = get_node(board_etree, "//caches")
    memory_node = get_node(board_etree, "//memory")
    devices_node = get_node(board_etree, "//devices")

    for entry in rtct.entries:
        cache_node = None
        if hasattr(entry, "level") and hasattr(entry, "cache_id"):
            cache_node = get_node(caches_node, f"cache[@level='{entry.level}' and @id='{hex(entry.cache_id)}']")
            if cache_node is None:
                logging.debug(f"Cannot find the level {entry.level} cache with cache ID {entry.cache_id}")
                continue

        if entry.type == acpiparser.rtct.ACPI_RTCT_TYPE_COMPATIBILITY:
            rtct_version_node = add_child(caches_node, "parameter", None, id="RTCT Version")
            add_child(rtct_version_node, "major", str(entry.rtct_version_major))
            add_child(rtct_version_node, "minor", str(entry.rtct_version_minor))
            rtcd_version_node = add_child(caches_node, "parameter", None, id="RTCD Version")
            add_child(rtcd_version_node, "major", str(entry.rtcd_version_major))
            add_child(rtcd_version_node, "minor", str(entry.rtcd_version_minor))
        elif entry.type == acpiparser.rtct.ACPI_RTCT_V2_TYPE_RTCD_Limits:
            add_child(caches_node, "parameter", str(entry.total_ia_l2_clos), id="Total IA L2 CLOS")
            add_child(caches_node, "parameter", str(entry.total_ia_l3_clos), id="Total IA L3 CLOS")
            add_child(caches_node, "parameter", str(entry.total_l2_instances), id="Total IA L2 Instances")
            add_child(caches_node, "parameter", str(entry.total_l3_instances), id="Total IA L2 Instances")
            add_child(caches_node, "parameter", str(entry.total_gt_clos), id="Total GT CLOS")
            add_child(caches_node, "parameter", str(entry.total_wrc_clos), id="Total WRC CLOS")
            add_child(devices_node, "parameter", str(entry.max_tcc_streams), id="Maximum TCC Streams")
            add_child(devices_node, "parameter", str(entry.max_tcc_registers), id="Maximum TCC Registers")
        elif entry.type == acpiparser.rtct.ACPI_RTCT_V2_TYPE_CRL_Binary:
            start = "0x{:016x}".format(entry.address)
            end = "0x{:016x}".format(entry.address + entry.size - 1)
            size = str(entry.size)
            region = add_child(memory_node, "range", None, id="CRL Binary", start=start, end=end, size=size)
        elif entry.type == acpiparser.rtct.ACPI_RTCT_V2_TYPE_IA_WayMasks:
            cap = add_child(cache_node, "parameter", None, id="IA Waymask")
            for waymask in entry.waymask:
                add_child(cap, "waymask", hex(waymask))
        elif entry.type == acpiparser.rtct.ACPI_RTCT_V2_TYPE_WRC_WayMasks:
            cap = add_child(cache_node, "parameter", None, id="WRC Waymask")
            add_child(cap, "waymask", hex(entry.waymask))
        elif entry.type == acpiparser.rtct.ACPI_RTCT_V2_TYPE_GT_WayMasks:
            cap = add_child(cache_node, "parameter", None, id="GT Waymask")
            for waymask in entry.waymask:
                add_child(cap, "waymask", hex(waymask))
        elif entry.type == acpiparser.rtct.ACPI_RTCT_V2_TYPE_SSRAM_WayMask:
            ssram_node = get_or_create_ssram_node(cache_node)
            add_child(ssram_node, "waymask", hex(entry.waymask))
        elif entry.type == acpiparser.rtct.ACPI_RTCT_V2_TYPE_SoftwareSRAM:
            ssram_node = get_or_create_ssram_node(cache_node)
            add_child(ssram_node, "start", "0x{:08x}".format(entry.base))
            add_child(ssram_node, "end", "0x{:08x}".format(entry.base + entry.size - 1))
            add_child(ssram_node, "size", str(entry.size))
        elif entry.type == acpiparser.rtct.ACPI_RTCT_V2_TYPE_MemoryHierarchyLatency:
            for cache_id in entry.cache_id:
                cache_node = get_node(caches_node, f"cache[@level='{entry.hierarchy}' and @id='{hex(cache_id)}']")
                if cache_node is None:
                    logging.debug(f"Cannot find the level {entry.hierarchy} cache with cache ID {cache_id}")
                    continue
                param = add_child(cache_node, "parameter", None, id="Worst Case Access Latency")
                add_child(param, "native", str(entry.clock_cycles))
        elif entry.type == acpiparser.rtct.ACPI_RTCT_V2_TYPE_ErrorLogAddress:
            start = "0x{:016x}".format(entry.address)
            end = "0x{:016x}".format(entry.address + entry.size - 1)
            size = str(entry.size)
            region = add_child(memory_node, "range", None, id="TCC Error Log", start=start, end=end, size=size)

def extract_tcc_capabilities(board_etree):
    try:
        rtct = acpiparser.parse_rtct()
        if rtct.version == 1:
            extract_tcc_capabilities_from_rtct_v1(board_etree, rtct)
        elif rtct.version == 2:
            extract_tcc_capabilities_from_rtct_v2(board_etree, rtct)
    except FileNotFoundError:
        pass

def extract(args, board_etree):
    try:
        tables = acpiparser.parse_apic()
    except Exception as e:
        logging.warning(f"Parse ACPI tables failed: {str(e)}")
        logging.warning(f"Will not extract information from ACPI tables")
        return

    ioapics_node = get_node(board_etree, "//ioapics")
    extract_topology(ioapics_node, tables)

    extract_tcc_capabilities(board_etree)
