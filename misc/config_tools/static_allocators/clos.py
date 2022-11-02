#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import acrn_config_utilities
import rdt
import re
from collections import defaultdict
from itertools import combinations
from collections import namedtuple
from acrn_config_utilities import get_node

def create_clos_node(scenario_etree, vm_id, index_list):
    allocation_vm_node = get_node(f"/acrn-config/vm[@id = '{vm_id}']", scenario_etree)
    if allocation_vm_node is None:
        allocation_vm_node = acrn_config_utilities.append_node("/acrn-config/vm", None, scenario_etree, id = vm_id)
    if get_node("./clos", allocation_vm_node) is None:
        clos_node = acrn_config_utilities.append_node("./clos", None, allocation_vm_node)
        for index in index_list:
            acrn_config_utilities.append_node(f"./vcpu_clos", str(index), clos_node)

def get_clos_id(rdt_list, policy_owner):
    for index,rdt in enumerate(rdt_list):
        if rdt.find_policy_owner(policy_owner):
            return index
    return 0

def alloc_clos_index(board_etree, scenario_etree, allocation_etree, mask_list):
    vm_node_list = scenario_etree.xpath("//vm")
    for vm_node in vm_node_list:
        vm_name = get_node("./name/text()", vm_node)
        vcpu_list = scenario_etree.xpath(f"//POLICY[VM = '{vm_name}']/VCPU/text()")
        index_list = []
        for vcpu in sorted([int(x) for x in set(vcpu_list)]):
            if rdt.cdp_enable(scenario_etree):
                index = get_clos_id(mask_list, rdt.policy_owner(vm_name, vcpu, "Data")) // 2
            else:
                index = get_clos_id(mask_list, rdt.policy_owner(vm_name, vcpu, "Unified"))
            index_list.append(index)
        create_clos_node(allocation_etree, get_node("./@id", vm_node), index_list)

def create_mask_list_node(board_etree, scenario_etree, allocation_etree, rdt_policy_list):
    allocation_hv_node = get_node(f"//hv", allocation_etree)
    if allocation_hv_node is None:
        allocation_hv_node = acrn_config_utilities.append_node(f"/acrn-config/hv", None, allocation_etree)

    if get_node("./clos_mask[@id = l3]", allocation_hv_node) is None:
        clos_mask = acrn_config_utilities.append_node("./clos_mask", None, allocation_hv_node, id="l3")
        length = get_node(f"//cache[@level='3']/capability/capacity_mask_length/text()", board_etree)
        if length is not None:
            default_l3_value = hex((1 << int(length)) - 1)
        else:
            default_l3_value = "0xffff"
        for i in range(0, len(rdt_policy_list)):
            if rdt_policy_list[i].l3policy.get_clos_mask() is not None:
                value = str(rdt_policy_list[i].l3policy.get_clos_mask())
            else:
                value = default_l3_value
            acrn_config_utilities.append_node(f"./clos", value, clos_mask)
        for index,cache2 in enumerate(rdt.L2Policy.cache2_id_list):
            length = get_node(f"//cache[@level='2' and @id = '{cache2}']/capability/capacity_mask_length/text()", board_etree)
            if length is not None:
                default_l2_value = hex((1 << int(length)) - 1)
            else:
                default_l2_value = "0xffff"

            if get_node("./clos_mask[@id = '{cache2}']", allocation_hv_node) is None:
                clos_mask = acrn_config_utilities.append_node("./clos_mask", None, allocation_hv_node, id=cache2)
            for i in range(0, len(rdt_policy_list)):
                if rdt_policy_list[i].l2policy.get_clos_mask(index) is not None:
                    value = str(rdt_policy_list[i].l2policy.get_clos_mask(index))
                else:
                    value = default_l2_value
                acrn_config_utilities.append_node(f"./clos", value, clos_mask)

def fn(board_etree, scenario_etree, allocation_etree):
    policy_list = rdt.get_policy_list(scenario_etree)
    create_mask_list_node(board_etree, scenario_etree, allocation_etree, policy_list)
    alloc_clos_index(board_etree, scenario_etree, allocation_etree, policy_list)
