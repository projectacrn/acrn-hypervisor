# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import logging
import lxml.etree

from cpuparser import parse_cpuid, get_online_cpu_ids
from extractors.helpers import add_child, get_node

level_types = {
    1: "thread",
    2: "core",
    3: "module",
    4: "tile",
    5: "die",
}

def get_parent(processors_node, topo_level, topo_id):
    n = get_node(processors_node, f"//{topo_level}[@id='{topo_id}']")
    return n

def get_or_create_parent(processors_node, topo_level, topo_id):
    n = get_parent(processors_node, topo_level, topo_id)
    if n is None:
        n = lxml.etree.Element(topo_level)
        n.set("id", topo_id)
        return (n, True)
    return (n, False)

def extract_model(processors_node, cpu_id, family_id, model_id, core_type, native_model_id):
    n = get_node(processors_node, f"//model[family_id='{family_id}' and model_id='{model_id}' and core_type='{core_type}' and native_model_id='{native_model_id}']")
    if n is None:
        n = add_child(processors_node, "model")

        add_child(n, "family_id", family_id)
        add_child(n, "model_id", model_id)
        add_child(n, "core_type", core_type)
        add_child(n, "native_model_id", native_model_id)

        brandstring = b""
        for leaf in [0x80000002, 0x80000003, 0x80000004]:
            leaf_data = parse_cpuid(leaf, 0, cpu_id)
            brandstring += leaf_data.brandstring
        n.set("description", brandstring.decode())

        leaves = [(1, 0), (7, 0), (0x80000001, 0), (0x80000007, 0)]
        for leaf in leaves:
            leaf_data = parse_cpuid(leaf[0], leaf[1], cpu_id)
            for cap in leaf_data.capability_bits:
                if getattr(leaf_data, cap) == 1:
                    add_child(n, "capability", id=cap)

def extract_topology(processors_node):
    cpu_ids = get_online_cpu_ids()
    for cpu_id in cpu_ids:
        subleaf = 0
        last_shift = 0
        last_node = None

        leaf_0 = parse_cpuid(0, 0, cpu_id)
        if leaf_0.max_leaf >= 0x1f:
            topo_leaf = 0x1f
        else:
            topo_leaf = 0xb

        while True:
            leaf_topo = parse_cpuid(topo_leaf, subleaf, cpu_id)
            if leaf_topo.level_type == 0:
                if last_node.tag != "package":
                    n, _ = get_or_create_parent(processors_node, "die", "0")
                    n.append(last_node)
                    last_node = n
                processors_node.append(last_node)
                break

            topo_level = level_types[leaf_topo.level_type]
            topo_id = hex(leaf_topo.x2apic_id >> last_shift)
            n, created = get_or_create_parent(processors_node, topo_level, topo_id)

            if last_node is None:
                leaf_1 = parse_cpuid(1, 0, cpu_id)
                family_id = hex(leaf_1.display_family)
                model_id = hex(leaf_1.display_model)
                if leaf_0.max_leaf >= 0x1a:
                    leaf_1a = parse_cpuid(0x1a, 0, cpu_id)
                    core_type = leaf_1a.core_type
                    native_model_id = hex(leaf_1a.native_model_id)
                else:
                    core_type = ""
                    native_model_id = ""

                add_child(n, "cpu_id", text=str(cpu_id))
                add_child(n, "apic_id", text=hex(leaf_1.initial_apic_id))
                add_child(n, "x2apic_id", text=hex(leaf_topo.x2apic_id))
                add_child(n, "family_id", text=family_id)
                add_child(n, "model_id", text=model_id)
                add_child(n, "stepping_id", text=hex(leaf_1.stepping))
                add_child(n, "core_type", text=core_type)
                add_child(n, "native_model_id", text=native_model_id)

                extract_model(processors_node, cpu_id, family_id, model_id, core_type, native_model_id)
            else:
                n.append(last_node)

            if not created:
                break

            last_node = n
            last_shift = leaf_topo.num_bit_shift
            subleaf += 1

def extract(args, board_etree):
    processors_node = get_node(board_etree, "//processors")
    extract_topology(processors_node)
