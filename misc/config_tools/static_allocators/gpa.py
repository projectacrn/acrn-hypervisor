#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common

def allocate_ssram_region(board_etree, scenario_etree, allocation_etree):
    # Guest physical address of the SW SRAM allocated to a pre-launched VM
    enabled = common.get_node("//PSRAM_ENABLED/text()", scenario_etree)
    if enabled == "y":
        pre_rt_vms = common.get_node("//vm[vm_type ='PRE_RT_VM']", scenario_etree)
        if pre_rt_vms is not None:
            vm_id = pre_rt_vms.get("id")
            l3_sw_sram = board_etree.xpath("//cache[@level='3']/capability[@id='Software SRAM']")
            if l3_sw_sram:
                start = min(map(lambda x: int(x.find("start").text, 16), l3_sw_sram))
                end = max(map(lambda x: int(x.find("end").text, 16), l3_sw_sram))

                allocation_vm_node = common.get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
                if allocation_vm_node is None:
                    allocation_vm_node = common.append_node("/acrn-config/vm", None, allocation_etree, id = vm_id)
                common.append_node("./ssram/start_gpa", hex(start), allocation_vm_node)
                common.append_node("./ssram/end_gpa", hex(end), allocation_vm_node)

def fn(board_etree, scenario_etree, allocation_etree):
    allocate_ssram_region(board_etree, scenario_etree, allocation_etree)
