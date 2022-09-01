# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import logging
import lxml.etree
from extractors.helpers import add_child, get_node

from cpuparser import parse_cpuid
import cpuparser.msr as msr
from acpiparser import parse_rtct
import acpiparser.rtct

known_cbms = {
    # From 11th Gen Intel(R) Core(TM) Processors Real-Time Tuning Guide, document number 640980-1.4
    "11th Gen Intel(R) Core(TM) i3-1115GRE": 12,
    "11th Gen Intel(R) Core(TM) i5-1145GRE": 8,
    "11th Gen Intel(R) Core(TM) i7-1185GRE": 12,
}

def infer_l3_cat(cpu_id, processor_model_node, cache_node):
    # First of all, existence of L3 CAT is indicated by the presence of IA32_L3_MASK_0 at C90H
    try:
        ia32_l3_mask_0 = msr.MSR_IA32_L3_MASK_n(0).rdmsr(cpu_id)
    except IOError:
        return

    # If L3 CAT does exist, try inferring its parameters:
    #
    #   - For capacity mask length, detect in an trial-and-error way starting from:
    #     a. the capacity mask length documented in any public real-time tuning guide, if any.
    #     b. or, the number of ways of the L3 cache.
    #
    #   - For the number of CLOS IDs available, detect by searching the last programmable IA32_L3_MASK_n register within
    #     the C90H - D0FH range which is the architecturally defined MSR space for those registers.
    #
    #   - For CDP, try setting the enable bit in IA32_L3_QOS_CFG. CDP is available if and only if that MSR is present
    #     and its bit 0 can be set.

    # Initial guess of the capacity mask length
    capacity_mask_length = int(cache_node.find("ways").text)
    processor_model = processor_model_node.get("description")
    for k, v in known_cbms.items():
        if processor_model.startswith(k):
            capacity_mask_length = v
            break

    # Verify our guess. If the verification fails, decrease by 1 and guess again.
    while capacity_mask_length > 0:
        ia32_l3_mask_0.bit_mask = (1 << capacity_mask_length) - 1
        try:
            ia32_l3_mask_0.wrmsr()
            break
        except IOError:
            capacity_mask_length = capacity_mask_length - 1
            continue
    else:
        logging.debug("All writes to IA32_L3_MASK_0 failed. Cannot guess the capacity mask length of L3 CAT.")
        return

    # Binary search of the number of CLOS available
    known_good = 1
    known_bad = 129
    while known_good + 1 < known_bad:
        mid = (known_good + known_bad) // 2
        try:
            msr.MSR_IA32_L3_MASK_n(mid - 1).rdmsr(cpu_id)
            known_good = mid
        except IOError:
            known_bad = mid
    clos_number = known_good

    # Detect availability of CDP by trying to write the enable bit.
    try:
        l3_qos_cfg = msr.MSR_IA32_L3_QOS_CFG.rdmsr(cpu_id)
        l3_qos_cfg.cdp_enable = 1
        l3_qos_cfg.wrmsr()
        has_cdp = True
    except IOError:
        has_cdp = False

    cap = add_child(cache_node, "capability", None, id="CAT")
    add_child(cap, "capacity_mask_length", str(capacity_mask_length))
    add_child(cap, "clos_number", str(clos_number))
    if has_cdp:
        add_child(cap, "capability", None, id="CDP")

def extract_topology(args, root_node, caches_node):
    threads = root_node.xpath("//processors//*[cpu_id]")
    for thread in threads:
        subleaf = 0
        while True:
            cpu_id = int(get_node(thread, "cpu_id/text()"))
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

                    # Inform the user if L3 CAT capability is specified manually.
                    if args.add_llc_cat:
                        logging.warning(r"The last level cache (cache ID: {cache_id}) already reports CAT capability. The explicit settings from the command line options are ignored.")
                elif cache_level == 3:
                    if args.add_llc_cat:
                        # Inject L3 CAT capability specified by the user
                        cap = add_child(n, "capability", None, id="CAT")
                        add_child(cap, "capacity_mask_length", str(args.add_llc_cat.capacity_mask_length))
                        add_child(cap, "clos_number", str(args.add_llc_cat.clos_number))
                        if args.add_llc_cat.has_CDP:
                            add_child(cap, "capability", None, id="CDP")
                    else:
                        # Try inferring L3 CAT according to the methods described in section 7.2.3, 11th Gen Intel(R)
                        # Core(TM) Processors Real-Time Tuning Guide (document number: 640980-1.4).
                        family_id = thread.find("family_id").text
                        model_id = thread.find("model_id").text
                        core_type = thread.find("core_type").text
                        native_model_id = thread.find("native_model_id").text
                        processor_model_node = get_node(root_node, f"//processors/model[family_id='{family_id}' and model_id='{model_id}' and core_type='{core_type}' and native_model_id='{native_model_id}']")
                        infer_l3_cat(cpu_id, processor_model_node, n)

            add_child(get_node(n, "processors"), "processor", get_node(thread, "apic_id/text()"))

            subleaf += 1

    def getkey(n):
        level = int(n.get("level"))
        id = int(n.get("id"), base=16)
        type = int(n.get("type"))
        return (level, id, type)
    caches_node[:] = sorted(caches_node, key=getkey)

def extract(args, board_etree):
    root_node = board_etree.getroot()
    caches_node = get_node(board_etree, "//caches")
    extract_topology(args, root_node, caches_node)
