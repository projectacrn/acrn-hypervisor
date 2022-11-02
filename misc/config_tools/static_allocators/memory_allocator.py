#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import logging
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import acrn_config_utilities, math, logging
from acrn_config_utilities import get_node

class RamRange():
    ram_range = {}

    @classmethod
    def import_memory_info(cls, board_etree, allocation_etree):
        hv_start = int(get_node("/acrn-config/hv/MEMORY/HV_RAM_START/text()", allocation_etree), 16)
        hv_size = int(get_node("/acrn-config/hv/MEMORY/HV_RAM_SIZE/text()", allocation_etree), 16)
        for memory_range in board_etree.xpath("/acrn-config/memory/range[not(@id) or @id = 'RAM']"):
            start = int(memory_range.get("start"), base=16)
            size = int(memory_range.get("size"), base=10)
            if start < hv_start and start + size > hv_start + hv_size:
                cls.ram_range[start] = hv_start - start
                cls.ram_range[hv_start + hv_size] = start + size - hv_start - hv_size
            else:
                cls.ram_range[start] = size

        return cls.ram_range

    @classmethod
    def check_hpa(cls, vm_node_info):
        hpa_node_list = vm_node_info.xpath("./memory/hpa_region/*")
        hpa_node_list_new = []
        for hpa_node in hpa_node_list:
            if int(hpa_node.text, base=16) != 0:
                hpa_node_list_new.append(hpa_node)

        return hpa_node_list_new

    @classmethod
    def get_memory_info(cls, vm_node_info):
        start_hpa = []
        size_hpa = []
        hpa_info = {}

        size_node = get_node("./memory/size", vm_node_info)
        if size_node is not None:
            size_byte = int(size_node.text) * 0x100000
            hpa_info[0] = size_byte
        hpa_node_list = RamRange().check_hpa(vm_node_info)
        if len(hpa_node_list) != 0:
            for hpa_node in hpa_node_list:
                if hpa_node.tag == "start_hpa":
                    start_hpa.append(int(hpa_node.text, 16))
                elif hpa_node.tag == "size_hpa":
                    size_byte = int(hpa_node.text) * 0x100000
                    size_hpa.append(size_byte)
        if len(start_hpa) != 0 and len(start_hpa) == len(start_hpa):
            for i in range(len(start_hpa)):
                hpa_info[start_hpa[i]] = size_hpa[i]

        return hpa_info

def alloc_memory(scenario_etree, ram_range_info):
    vm_node_list = scenario_etree.xpath("/acrn-config/vm[load_order = 'PRE_LAUNCHED_VM']")
    mem_info_list = []
    vm_node_index_list = []
    dic_key = sorted(ram_range_info)
    for key in dic_key:
        if key < 0x100000000:
            ram_range_info.pop(key)

    for vm_node in vm_node_list:
        mem_info = RamRange().get_memory_info(vm_node)
        mem_info_list.append(mem_info)
        vm_node_index_list.append(vm_node.attrib["id"])

    ram_range_info = alloc_hpa_region(ram_range_info, mem_info_list, vm_node_index_list)
    ram_range_info, mem_info_list = alloc_whole_size(ram_range_info, mem_info_list, vm_node_index_list)

    return ram_range_info, mem_info_list, vm_node_index_list

def alloc_hpa_region(ram_range_info, mem_info_list, vm_node_index_list):
    for vm_index in range(len(vm_node_index_list)):
        hpa_key = sorted(mem_info_list[vm_index])
        mem_key = sorted(ram_range_info)
        for mem_start in mem_key:
            mem_size = ram_range_info[mem_start]
            mem_end = mem_start + mem_size
            for hpa_start in hpa_key:
                hpa_size = mem_info_list[vm_index][hpa_start]
                hpa_end = hpa_start + hpa_size
                if hpa_start != 0:
                    if mem_start < hpa_start and mem_end > hpa_end:
                        ram_range_info[mem_start] = hpa_start - mem_start
                        ram_range_info[hpa_end] = mem_end - hpa_end
                    elif mem_start == hpa_start and mem_end > hpa_end:
                        del ram_range_info[mem_start]
                        ram_range_info[hpa_end] = mem_end - hpa_end
                    elif mem_start < hpa_start and mem_end == hpa_end:
                        ram_range_info[mem_start] = hpa_start - mem_start
                    elif mem_start == hpa_start and mem_end == hpa_end:
                        del ram_range_info[mem_start]
                    elif mem_start > hpa_start or mem_end < hpa_end:
                        logging.warning(f"Start address of HPA is out of available memory range: vm id: {vm_index}, hpa_start: {hpa_start}.")
                    elif mem_size < hpa_size:
                        logging.warning(f"Size of HPA is out of available memory range: vm id: {vm_index}, hpa_size: {hpa_size}.")

    return ram_range_info

def alloc_whole_size(ram_range_info, mem_info_list, vm_node_index_list):
    for vm_index in range(len(vm_node_index_list)):
        if 0 in mem_info_list[vm_index].keys() and mem_info_list[vm_index][0] != 0:
            remain_size = mem_info_list[vm_index][0]
            hpa_info = mem_info_list[vm_index]
            mem_key = sorted(ram_range_info)
            for mem_start in mem_key:
                mem_size = ram_range_info[mem_start]
                if remain_size != 0 and remain_size <= mem_size:
                    del ram_range_info[mem_start]
                    hpa_info[mem_start] = remain_size
                    del hpa_info[0]
                    if mem_size > remain_size:
                        ram_range_info[mem_start + remain_size] = mem_size - remain_size
                    remain_size = 0
                elif remain_size > mem_size:
                    hpa_info[mem_start] = mem_size
                    del ram_range_info[mem_start]
                    hpa_info[0] = remain_size - mem_size
                    remain_size = hpa_info[0]

    return ram_range_info, mem_info_list

def write_hpa_info(allocation_etree, mem_info_list, vm_node_index_list):
    for i in range(len(vm_node_index_list)):
        vm_id = vm_node_index_list[i]
        hpa_info = mem_info_list[i]
        vm_node = get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
        if vm_node is None:
            vm_node = acrn_config_utilities.append_node("/acrn-config/vm", None, allocation_etree, id=vm_id)
        memory_node = get_node("./memory", vm_node)
        if memory_node is None:
            memory_node = acrn_config_utilities.append_node(f"./memory", None, vm_node)
        region_index = 1
        start_key = sorted(hpa_info)
        for start_hpa in start_key:
            hpa_region_node = get_node(f"./hpa_region[@id='{region_index}']", memory_node)
            if hpa_region_node is None:
                hpa_region_node = acrn_config_utilities.append_node("./hpa_region", None, memory_node, id=str(region_index).encode('UTF-8'))

                start_hpa_node = get_node("./start_hpa", hpa_region_node)
                if start_hpa_node is None:
                    acrn_config_utilities.append_node("./start_hpa", hex(start_hpa), hpa_region_node)

                size_hpa_node = get_node("./size_hpa", hpa_region_node)
                if size_hpa_node is None:
                    acrn_config_utilities.append_node("./size_hpa", hex(hpa_info[start_hpa]), hpa_region_node)
            region_index = region_index + 1

def alloc_vm_memory(board_etree, scenario_etree, allocation_etree):
    ram_range_info = RamRange().import_memory_info(board_etree, allocation_etree)
    ram_range_info, mem_info_list, vm_node_index_list = alloc_memory(scenario_etree, ram_range_info)
    write_hpa_info(allocation_etree, mem_info_list, vm_node_index_list)

def allocate_hugepages(board_etree, scenario_etree, allocation_etree):
    hugepages_1gb = 0
    hugepages_2mb = 0
    ram_range_info = RamRange().ram_range
    total_hugepages = int(sum(ram_range_info[i] for i in ram_range_info if i >= 0x100000000)*0.98/(1024*1024*1024) \
                      - sum(int(i) for i in scenario_etree.xpath("//vm[load_order = 'PRE_LAUNCHED_VM']/memory/hpa_region/size_hpa/text()"))/1024 \
                      - 5 - 300/1024 * len(scenario_etree.xpath("//virtio_devices/gpu")))

    post_launch_vms = scenario_etree.xpath("//vm[load_order = 'POST_LAUNCHED_VM']")
    if len(post_launch_vms) > 0:
        for post_launch_vm in post_launch_vms:
            size = get_node("./memory/size/text()", post_launch_vm)
            if size is not None:
                mb, gb = math.modf(int(size)/1024)
                hugepages_1gb = int(hugepages_1gb + gb)
                hugepages_2mb = int(hugepages_2mb + math.ceil(mb * 1024 / 2))

    post_vms_memory = sum(int(i) for i in scenario_etree.xpath("//vm[load_order = 'POST_LAUNCHED_VM']/memory/size/text()")) / 1024
    if total_hugepages - post_vms_memory < 0:
        logging.debug(f"The sum {post_vms_memory} of memory configured in post launch VMs should not be larger than " \
        f"the calculated total hugepages {total_hugepages} of service VMs. Please update the configuration in post launch VMs")

    allocation_service_vm_node = get_node("/acrn-config/vm[load_order = 'SERVICE_VM']", allocation_etree)
    if allocation_service_vm_node is not None:
        acrn_config_utilities.append_node("./hugepages/gb", int(hugepages_1gb), allocation_service_vm_node)
        acrn_config_utilities.append_node("./hugepages/mb", int(hugepages_2mb), allocation_service_vm_node)

def fn(board_etree, scenario_etree, allocation_etree):
    alloc_vm_memory(board_etree, scenario_etree, allocation_etree)
    allocate_hugepages(board_etree, scenario_etree, allocation_etree)
