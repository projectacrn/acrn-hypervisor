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

def create_clos_node(etree, vm_id, index_list):
    allocation_vm_node = common.get_node(f"/acrn-config/vm[@id = '{vm_id}']", etree)
    if allocation_vm_node is None:
        allocation_vm_node = common.append_node("/acrn-config/vm", None, etree, id = vm_id)
    if common.get_node("./clos", allocation_vm_node) is None:
        clos_node = common.append_node("./clos", None, allocation_vm_node)
        for index in index_list:
            common.append_node(f"./vcpu_clos", str(index), clos_node)

def find_cache2_id(mask, cache2_id_list):
    for cache2 in cache2_id_list:
        if mask[cache2] != "None":
            return cache2
    return "None"

def merge_policy_list(mask_list, cache2_id_list):
    index = 0
    result_list = []
    for index,mask in enumerate(mask_list):
        merged = 0
        if index == 0:
            result_list.append(mask)
            continue
        for result in result_list:
            if result["l3"] != mask["l3"]:
                continue
            else:
                cache2_id = find_cache2_id(mask, cache2_id_list)
                if cache2_id == "None" or result[cache2_id] == mask[cache2_id]:
                    merged = 1
                    break
                if result[cache2_id] == "None":
                    merged = 1
                    result[cache2_id] = mask[cache2_id]
                    break
        if merged == 0:
            result_list.append(mask)
    return result_list

def gen_all_clos_index(board_etree, scenario_etree, allocation_etree):
    policy_list = []
    allocation_list = scenario_etree.xpath(f"//POLICY")
    cache2_id_list = scenario_etree.xpath("//CACHE_ALLOCATION[CACHE_LEVEL = 2]/CACHE_ID/text()")
    cache2_id_list.sort()

    for policy in allocation_list:
        cache_level = common.get_node("../CACHE_LEVEL/text()", policy)
        cache_id = common.get_node("../CACHE_ID/text()", policy)
        vcpu = common.get_node("./VCPU/text()", policy)
        mask = common.get_node("./CLOS_MASK/text()", policy)
        tmp = (cache_level, cache_id, vcpu, mask)
        policy_list.append(tmp)

    vCPU_list = scenario_etree.xpath(f"//POLICY/VCPU/text()")
    l3_mask_list = scenario_etree.xpath(f"//CACHE_ALLOCATION[CACHE_LEVEL = 3]/POLICY/CLOS_MASK")
    mask_list = []
    for vCPU in vCPU_list:
        dict_tmp = {}
        l3_mask = l2_mask = "None"
        l3_mask_list = scenario_etree.xpath(f"//CACHE_ALLOCATION[CACHE_LEVEL = 3]/POLICY[VCPU = '{vCPU}']/CLOS_MASK/text()")
        if len(l3_mask_list) > 0:
            l3_mask = l3_mask_list[0]
        dict_tmp["l3"] = l3_mask

        l2_mask_list = scenario_etree.xpath(f"//CACHE_ALLOCATION[CACHE_LEVEL = 2]/POLICY[VCPU = '{vCPU}']/CLOS_MASK")
        if len(l2_mask_list) > 0:
            l2_mask = l2_mask_list[0].text
            cache_id = scenario_etree.xpath(f"//CACHE_ALLOCATION[CACHE_LEVEL = 2 and POLICY/VCPU = '{vCPU}']/CACHE_ID/text()")[0]
        for cache2 in cache2_id_list:
            if cache2 == cache_id:
                dict_tmp[cache_id] = l2_mask
            else:
                dict_tmp[cache2] = "None"
        mask_list.append(dict_tmp)
    mask_list = merge_policy_list(mask_list, cache2_id_list)
    return mask_list

def get_clos_index(cache_level, cache_id, clos_mask):
    mask_list = common.get_mask_list(cache_level, cache_id)
    idx = 0
    for mask in mask_list:
        idx += 1
        if mask == clos_mask:
            break
    return idx
def get_clos_id(mask_list, l2_id, l2_mask, l3_mask):
    for mask in mask_list:
        if mask[l2_id] == l2_mask and mask["l3"] == l3_mask:
            return mask_list.index(mask)
    return 0

def alloc_clos_index(board_etree, scenario_etree, allocation_etree, mask_list):
    vm_node_list = scenario_etree.xpath("//vm")
    for vm_node in vm_node_list:
        vmname = common.get_node("./name/text()", vm_node)
        allocation_list = scenario_etree.xpath(f"//CACHE_ALLOCATION[POLICY/VM = '{vmname}']")
        for allocation in allocation_list:
            index_list = []
            cache_level = common.get_node("./CACHE_LEVEL/text()", allocation)
            cache_id = common.get_node("./CACHE_ID/text()", allocation)
            clos_mask_list = allocation.xpath(f".//POLICY[VM = '{vmname}']/CLOS_MASK/text()")

            for clos_mask in clos_mask_list:
                index = get_clos_id(mask_list, cache_id, clos_mask, "None")
                index_list.append(index)
            create_clos_node(allocation_etree, common.get_node("./@id", vm_node), index_list)

def creat_mask_list_node(board_etree, scenario_etree, allocation_etree, mask_list):
    allocation_hv_node = common.get_node(f"//hv", allocation_etree)
    if allocation_hv_node is None:
        allocation_hv_node = common.append_node("//hv", None, allocation_etree, id = vm_id)
    cache2_id_list = scenario_etree.xpath("//CACHE_ALLOCATION[CACHE_LEVEL = 2]/CACHE_ID/text()")
    cache2_id_list.sort()
    if common.get_node("./clos_mask[@id = l3]", allocation_hv_node) is None:
        clos_mask = common.append_node("./clos_mask", None, allocation_hv_node, id="l3")
        for i in range(0, len(mask_list)):
            if mask_list[i]["l3"] == "None":
                value = "0xffff"
            else:
                value = str(mask_list[i]["l3"])
            common.append_node(f"./clos", value, clos_mask)

        for cache2 in cache2_id_list:
            if common.get_node("./clos_mask[@id = '{cache2}']", allocation_hv_node) is None:
                clos_mask = common.append_node("./clos_mask", None, allocation_hv_node, id=cache2)
            for i in range(0, len(mask_list)):
                if mask_list[i][cache2] == "None":
                    value = "0xffff"
                else:
                    value = str(mask_list[i][cache2] )
                common.append_node(f"./clos", value, clos_mask)

def fn(board_etree, scenario_etree, allocation_etree):
    mask_list = gen_all_clos_index(board_etree, scenario_etree, allocation_etree)
    creat_mask_list_node(board_etree, scenario_etree, allocation_etree, mask_list)
    alloc_clos_index(board_etree, scenario_etree, allocation_etree, mask_list)
