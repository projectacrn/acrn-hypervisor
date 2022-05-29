#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common
import re
from collections import defaultdict
from itertools import combinations
from collections import namedtuple


policy_owner = namedtuple("policy_owner", ["vm_name", "vcpu", "cache_type"])

class Policy:
    def __init__(self, cache_id, clos_mask):
        self.cache_id = cache_id
        self.clos_mask = clos_mask

    def get_cache_id(self):
        return self.cache_id

    def get_clos_mask(self):
        return self.clos_mask

    def match_policy(self, src):
        return (self.clos_mask == None) or (src.clos_mask == None) or (self.clos_mask == src.clos_mask)

class L3Policy(Policy):
    def __init__(self, policy):
        self.cache_id = policy.get_cache_id()
        self.clos_mask = policy.get_clos_mask()

    def merge_policy(self, src):
        return self.match_policy(src)

class L2Policy:
    cache2_id_list = []

    def __init__(self, policy):
        self.policy_list = []
        for index,cache2_id in enumerate(self.cache2_id_list):
            if cache2_id == policy.cache_id:
                self.policy_list.append(policy)
            else:
                self.policy_list.append(Policy(None, None))

    def get_cache_id(self, index):
        return self.policy_list[index].get_cache_id()

    def get_clos_mask(self, index):
        return self.policy_list[index].get_clos_mask()

    def match_policy(self, src):
        for index in range(0, len(self.policy_list)):
            if not self.policy_list[index].match_policy(src.policy_list[index]):
                return False
        return True

    def merge_policy(self, src):
        if self.match_policy(src):
            for index in range(0, len(self.policy_list)):
                if self.policy_list[index].clos_mask is None:
                    self.policy_list[index].clos_mask = src.policy_list[index].clos_mask
            return True
        return False

class RdtPolicy:
    def __init__(self, policy_list, policy_owner):
        cache2_id = None
        cache3_id = None
        l2_mask = None
        l3_mask = None
        for policy in policy_list:
            cache_level = common.get_node("../CACHE_LEVEL/text()", policy)
            cache_id = common.get_node("../CACHE_ID/text()", policy)
            clos_mask = common.get_node("./CLOS_MASK/text()", policy)
            if cache_level == "2":
                l2_mask = clos_mask
                cache2_id = cache_id
            else:
                l3_mask = clos_mask
                cache3_id = cache_id
        self.l2policy = L2Policy(Policy(cache2_id, l2_mask))
        self.l3policy = L3Policy(Policy(cache3_id, l3_mask))

        # a list stored the vCPU or VM info
        self.policy_owner_list = [policy_owner]

    def match_policy(self, src):
        return self.l2policy.match_policy(src.l2policy) and self.l3policy.match_policy(src.l3policy)

    #check whether the src could be merged, if yes, add the src owner to policy_owner_list list and return True
    def merge_policy(self, src):
        if self.match_policy(src):
            self.l2policy.merge_policy(src.l2policy)
            self.l3policy.merge_policy(src.l3policy)
            self.policy_owner_list += src.policy_owner_list
            return True
        return False

    #check whether a VM/vCPU could use this policy
    def find_policy_owner(self, policy_owner):
        return policy_owner in self.policy_owner_list

class vCatPolicy(RdtPolicy):
    def merge_policy(self, src):
        return False

def create_clos_node(scenario_etree, vm_id, index_list):
    allocation_vm_node = common.get_node(f"/acrn-config/vm[@id = '{vm_id}']", scenario_etree)
    if allocation_vm_node is None:
        allocation_vm_node = common.append_node("/acrn-config/vm", None, scenario_etree, id = vm_id)
    if common.get_node("./clos", allocation_vm_node) is None:
        clos_node = common.append_node("./clos", None, allocation_vm_node)
        for index in index_list:
            common.append_node(f"./vcpu_clos", str(index), clos_node)

def merge_policy_list(policy_list):
    result_list = []
    for index,p in enumerate(policy_list):
        merged = False
        for result in result_list:
            if result.merge_policy(p):
                merged = True
                break;
        if not merged:
            result_list.append(p)
    return result_list

def gen_policy_owner_list(scenario_etree):
    policy_owner_list = []
    vm_list = scenario_etree.xpath("//POLICY/VM")
    for vm in vm_list:
        vm_name = common.get_node("./text()", vm)
        vcpu = common.get_node("../VCPU/text()", vm)
        cache_type = common.get_node("../TYPE/text()", vm)
        policy_owner_list.append(policy_owner(vm_name, vcpu, cache_type))
    return policy_owner_list

def vm_vcat_enable(scenario_etree, vm_name):
    vcat_enable = common.get_node(f"//VCAT_ENABLED/text()", scenario_etree)
    virtual_cat_support = common.get_node(f"//vm[name = '{vm_name}']/virtual_cat_support/text()", scenario_etree)
    return (vcat_enable == "y") and (virtual_cat_support == "y")

def get_policy_list(board_etree, scenario_etree, allocation_etree):
    policy_owner_list = gen_policy_owner_list(scenario_etree)

    result_list = []
    for policy_owner in policy_owner_list:
        dict_tmp = {}
        policy_list = scenario_etree.xpath(f"//POLICY[VM = '{policy_owner.vm_name}' and VCPU = '{policy_owner.vcpu}' and TYPE = '{policy_owner.cache_type}']")
        if vm_vcat_enable(scenario_etree, policy_owner.vm_name):
            result_list.append(vCatPolicy(policy_list, policy_owner))
        else:
            result_list.append(RdtPolicy(policy_list, policy_owner))
    return merge_policy_list(result_list)

def get_clos_id(rdt_list, policy_owner):
    for index,rdt in enumerate(rdt_list):
        if rdt.find_policy_owner(policy_owner):
            return index
    return 0

def alloc_clos_index(board_etree, scenario_etree, allocation_etree, mask_list):
    vm_node_list = scenario_etree.xpath("//vm")
    for vm_node in vm_node_list:
        vm_name = common.get_node("./name/text()", vm_node)
        vcpu_list = scenario_etree.xpath(f"//POLICY[VM = '{vm_name}']/VCPU/text()")
        index_list = []
        for vcpu in sorted(list(set(vcpu_list))):
            type_list = scenario_etree.xpath(f"//POLICY[VM = '{vm_name}' and VCPU = '{vcpu}']/TYPE/text()")
            for cache_type in sorted(list(set(type_list))):
                if cache_type == "Data":
                    continue
                index = get_clos_id(mask_list, policy_owner(vm_name, vcpu, cache_type))
                index_list.append(index)
        create_clos_node(allocation_etree, common.get_node("./@id", vm_node), index_list)

def create_mask_list_node(board_etree, scenario_etree, allocation_etree, rdt_policy_list):
    allocation_hv_node = common.get_node(f"//hv", allocation_etree)
    if allocation_hv_node is None:
        allocation_hv_node = common.append_node(f"/acrn-config/hv", None, allocation_etree)

    if common.get_node("./clos_mask[@id = l3]", allocation_hv_node) is None:
        clos_mask = common.append_node("./clos_mask", None, allocation_hv_node, id="l3")
        length = common.get_node(f"//cache[@level='3']/capability/capacity_mask_length/text()", board_etree)
        if length is not None:
            value = hex((1 << int(length)) - 1)
        else:
            value = "0xffff"
        for i in range(0, len(rdt_policy_list)):
            if rdt_policy_list[i].l3policy.get_clos_mask() is not None:
                value = str(rdt_policy_list[i].l3policy.get_clos_mask())
            common.append_node(f"./clos", value, clos_mask)
        for index,cache2 in enumerate(L2Policy.cache2_id_list):
            length = common.get_node(f"//cache[@level='2' and @id = '{cache2}']/capability/capacity_mask_length/text()", board_etree)
            value = hex((1 << int(length)) - 1)
            if common.get_node("./clos_mask[@id = '{cache2}']", allocation_hv_node) is None:
                clos_mask = common.append_node("./clos_mask", None, allocation_hv_node, id=cache2)
            for i in range(0, len(rdt_policy_list)):
                if rdt_policy_list[i].l2policy.get_clos_mask(index) is not None:
                    value = str(rdt_policy_list[i].l2policy.get_clos_mask(index))
                common.append_node(f"./clos", value, clos_mask)

def init_cache2_id_list(scenario_etree):
    cache2_id_list = scenario_etree.xpath("//CACHE_ALLOCATION[CACHE_LEVEL = 2]/CACHE_ID/text()")
    cache2_id_list.sort()
    L2Policy.cache2_id_list = cache2_id_list

def fn(board_etree, scenario_etree, allocation_etree):
    init_cache2_id_list(scenario_etree)
    policy_list = get_policy_list(board_etree, scenario_etree, allocation_etree)
    create_mask_list_node(board_etree, scenario_etree, allocation_etree, policy_list)
    alloc_clos_index(board_etree, scenario_etree, allocation_etree, policy_list)
