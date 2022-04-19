# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import logging
import lxml.etree
from extractors.helpers import add_child, get_node

from cpuparser import parse_cpuid
from acpiparser import parse_rtct
import acpiparser.rtct

def extract_topology(root_node, caches_node):
    threads = root_node.xpath("//processors//*[cpu_id]")
    for thread in threads:
        subleaf = 0
        while True:
            cpu_id = int(get_node(thread, "cpu_id/text()"), base=16)
            leaf_4 = parse_cpuid(4, subleaf, cpu_id)
            cache_type = leaf_4.cache_type
            if cache_type == 0:
                break

            cache_level = leaf_4.cache_level
            shift_width = leaf_4.max_logical_processors_sharing_cache.bit_length() - 1
            cache_id = hex(int(get_node(thread, "apic_id/text()"), base=16) >> shift_width)

            n = get_node(caches_node, f"cache[@id='{cache_id}' and @type='{cache_type}' and @level='{cache_level}']")
            if n is None:
                n = add_child(caches_node, "cache", None, level=str(cache_level), id=cache_id, type=str(cache_type))
                add_child(n, "cache_size", str(leaf_4.cache_size))
                add_child(n, "line_size", str(leaf_4.line_size))
                add_child(n, "ways", str(leaf_4.ways))
                add_child(n, "sets", str(leaf_4.sets))
                add_child(n, "partitions", str(leaf_4.partitions))
                add_child(n, "self_initializing", str(leaf_4.self_initializing))
                add_child(n, "fully_associative", str(leaf_4.fully_associative))
                add_child(n, "write_back_invalidate", str(leaf_4.write_back_invalidate))
                add_child(n, "cache_inclusiveness", str(leaf_4.cache_inclusiveness))
                add_child(n, "complex_cache_indexing", str(leaf_4.complex_cache_indexing))
                add_child(n, "processors")

                # Check support of Cache Allocation Technology
                leaf_10 = parse_cpuid(0x10, 0, cpu_id)
                if cache_level == 2:
                    leaf_10 = parse_cpuid(0x10, 2, cpu_id) if leaf_10.l2_cache_allocation == 1 else None
                elif cache_level == 3:
                    leaf_10 = parse_cpuid(0x10, 1, cpu_id) if leaf_10.l3_cache_allocation == 1 else None
                else:
                    leaf_10 = None
                if leaf_10 is not None:
                    cap = add_child(n, "capability", None, id="CAT")
                    add_child(cap, "capacity_mask_length", str(leaf_10.capacity_mask_length))
                    add_child(cap, "clos_number", str(leaf_10.clos_number))
                    if leaf_10.code_and_data_prioritization == 1:
                        add_child(n, "capability", None, id="CDP")

            add_child(get_node(n, "processors"), "processor", get_node(thread, "apic_id/text()"))

            subleaf += 1

    def getkey(n):
        level = int(n.get("level"))
        id = int(n.get("id"), base=16)
        type = int(n.get("type"))
        return (level, id, type)
    caches_node[:] = sorted(caches_node, key=getkey)

def extract_tcc_capabilities(caches_node):
    try:
        rtct = parse_rtct()
        if rtct.version == 1:
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
        elif rtct.version == 2:
            for entry in rtct.entries:
                if entry.type == acpiparser.rtct.ACPI_RTCT_V2_TYPE_SoftwareSRAM:
                    cache_node = get_node(caches_node, f"cache[@level='{entry.level}' and @id='{hex(entry.cache_id)}']")
                    if cache_node is None:
                        logging.debug(f"Cannot find the level {entry.level} cache with cache ID {entry.cache_id}")
                        continue
                    cap = add_child(cache_node, "capability", None, id="Software SRAM")
                    add_child(cap, "start", "0x{:08x}".format(entry.base))
                    add_child(cap, "end", "0x{:08x}".format(entry.base + entry.size - 1))
                    add_child(cap, "size", str(entry.size))
    except FileNotFoundError:
        pass

def extract(args, board_etree):
    root_node = board_etree.getroot()
    caches_node = get_node(board_etree, "//caches")
    extract_topology(root_node, caches_node)
    extract_tcc_capabilities(caches_node)

    # Inject the explicitly specified CAT capability if exists
    if args.add_llc_cat:
        llc_node = get_node(root_node, "//caches/cache[@level='3']")
        llc_cat_node = get_node(llc_node, "capability[@id='CAT']")
        if llc_cat_node is None:
            llc_cat_node = add_child(llc_node, "capability", None, id="CAT")
            add_child(llc_cat_node, "capacity_mask_length", str(args.add_llc_cat.capacity_mask_length))
            add_child(llc_cat_node, "clos_number", str(args.add_llc_cat.clos_number))
            if args.add_llc_cat.has_CDP:
                add_child(llc_node, "capability", None, id="CDP")
        else:
            logging.warning("The last level cache already reports CAT capability. The explicit settings from the command line options are ignored.")
