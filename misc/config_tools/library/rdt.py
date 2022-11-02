#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import acrn_config_utilities
import re
from collections import defaultdict
from collections import namedtuple
from acrn_config_utilities import get_node

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
            cache_level = get_node("../CACHE_LEVEL/text()", policy)
            cache_id = get_node("../CACHE_ID/text()", policy)
            clos_mask = get_node("./CLOS_MASK/text()", policy)
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

class CdpPolicy():
    def __init__(self,data_list, code_list, owner):
        self.data_policy = RdtPolicy(data_list, policy_owner(owner.vm_name, owner.vcpu, "Data"))
        self.code_policy = RdtPolicy(code_list, policy_owner(owner.vm_name, owner.vcpu, "Code"))

    def merge_policy(self, src):
        if self.code_policy.match_policy(src.code_policy) and self.data_policy.match_policy(src.data_policy):
            self.code_policy.merge_policy(src.code_policy)
            self.data_policy.merge_policy(src.data_policy)
            return True
        return False

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
        vm_name = get_node("./text()", vm)
        vcpu = get_node("../VCPU/text()", vm)
        cache_type = get_node("../TYPE/text()", vm)
        policy_owner_list.append(policy_owner(vm_name, int(vcpu), cache_type))
    return policy_owner_list

def vm_vcat_enable(scenario_etree, vm_name):
    vcat_enable = get_node(f"//VCAT_ENABLED/text()", scenario_etree)
    virtual_cat_support = get_node(f"//vm[name = '{vm_name}']/virtual_cat_support/text()", scenario_etree)
    return (vcat_enable == "y") and (virtual_cat_support == "y")

def cdp_enable(scenario_etree):
    cdp_enable = get_node(f"//CDP_ENABLED/text()", scenario_etree)
    return cdp_enable == "y"

def convert_cdp_to_normal(cdp_policy_list):
    policy_list = []
    for cdp_policy in cdp_policy_list:
        policy_list.append(cdp_policy.data_policy)
        policy_list.append(cdp_policy.code_policy)
    return policy_list

def get_policy_list(scenario_etree):
    init_cache2_id_list(scenario_etree)
    policy_owner_list = gen_policy_owner_list(scenario_etree)

    result_list = []
    for policy_owner in policy_owner_list:
        dict_tmp = {}
        policy_list = scenario_etree.xpath(f"//POLICY[VM = '{policy_owner.vm_name}' and VCPU = '{policy_owner.vcpu}']")
        if cdp_enable(scenario_etree):
            data_list = scenario_etree.xpath(f"//POLICY[VM = '{policy_owner.vm_name}' and VCPU = '{policy_owner.vcpu}' and TYPE = 'Data']")
            code_list = scenario_etree.xpath(f"//POLICY[VM = '{policy_owner.vm_name}' and VCPU = '{policy_owner.vcpu}' and TYPE = 'Code']")
            if policy_owner.cache_type == "Code":
                continue
            elif policy_owner.cache_type == "Data":
                result_list.append(CdpPolicy(data_list, code_list, policy_owner))
        elif vm_vcat_enable(scenario_etree, policy_owner.vm_name):
            result_list.append(vCatPolicy(policy_list, policy_owner))
        else:
            result_list.append(RdtPolicy(policy_list, policy_owner))
    result_list = merge_policy_list(result_list)

    if cdp_enable(scenario_etree):
        result_list = convert_cdp_to_normal(result_list)

    return result_list

def init_cache2_id_list(scenario_etree):
    cache2_id_list = scenario_etree.xpath("//CACHE_ALLOCATION[CACHE_LEVEL = 2]/CACHE_ID/text()")
    cache2_id_list.sort()
    L2Policy.cache2_id_list = cache2_id_list
