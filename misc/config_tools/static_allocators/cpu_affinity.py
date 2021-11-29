#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, board_cfg_lib

def service_vm_cpu_affinity(etree):
    if common.get_node("//vm[vm_type = 'SERVICE_VM']", etree) is None:
        return None

    if common.get_node("//vm[vm_type = 'SERVICE_VM' and count(cpu_affinity)]", etree) is not None:
        return None

    service_vm_extend_all_cpus = board_cfg_lib.get_processor_info()
    pre_all_cpus = etree.xpath("//vm[vm_type = 'PRE_RT_VM' or vm_type = 'PRE_STD_VM' or vm_type = 'SAFETY_VM']/cpu_affinity/pcpu_id/text()")

    cpus_for_service_vm = list(set(service_vm_extend_all_cpus) - set(pre_all_cpus))
    return sorted(cpus_for_service_vm)

def fn(board_etree, scenario_etree, allocation_etree):
    cpus_for_service_vm = service_vm_cpu_affinity(scenario_etree)
    if cpus_for_service_vm:
        if common.get_node("//vm[vm_type = 'SERVICE_VM']", scenario_etree) is not None:
            vm_id = common.get_node("//vm[vm_type = 'SERVICE_VM']/@id", scenario_etree)
            allocation_service_vm_node = common.get_node(f"/acrn-config/vm[@id='{vm_id}']", allocation_etree)
            if allocation_service_vm_node is None:
                allocation_service_vm_node = common.append_node("/acrn-config/vm", None, allocation_etree, id = vm_id)
            if common.get_node("./vm_type", allocation_service_vm_node) is None:
                common.append_node("./vm_type", "SERVICE_VM", allocation_service_vm_node)
        for pcpu_id in cpus_for_service_vm:
            common.append_node("./cpu_affinity/pcpu_id", str(pcpu_id), allocation_service_vm_node)
