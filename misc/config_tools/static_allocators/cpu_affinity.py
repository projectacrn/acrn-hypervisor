#!/usr/bin/env python3
#
# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, board_cfg_lib

def sos_cpu_affinity(etree):
    if common.get_node("//vm[load_order = 'SERVICE_VM']", etree) is None:
        return None

    if common.get_node("//vm[load_order = 'SERVICE_VM' and count(cpu_affinity//pcpu_id)]", etree) is not None:
        return None

    sos_extend_all_cpus = board_cfg_lib.get_processor_info()
    pre_all_cpus = etree.xpath("//vm[load_order = 'PRE_LAUNCHED_VM']/cpu_affinity//pcpu_id/text()")

    cpus_for_sos = list(set(sos_extend_all_cpus) - set(pre_all_cpus))
    return sorted(cpus_for_sos)

def fn(board_etree, scenario_etree, allocation_etree):
    cpus_for_sos = sos_cpu_affinity(scenario_etree)
    if cpus_for_sos:
        if common.get_node("//vm[load_order = 'SERVICE_VM']", scenario_etree) is not None:
            vm_id = common.get_node("//vm[load_order = 'SERVICE_VM']/@id", scenario_etree)
            allocation_sos_vm_node = common.get_node(f"/acrn-config/vm[@id='{vm_id}']", allocation_etree)
            if allocation_sos_vm_node is None:
                allocation_sos_vm_node = common.append_node("/acrn-config/vm", None, allocation_etree, id = vm_id)
            if common.get_node("./load_order", allocation_sos_vm_node) is None:
                common.append_node("./load_order", "SERVICE_VM", allocation_sos_vm_node)
        for pcpu_id in sorted([int(x) for x in cpus_for_sos]):
            common.append_node("./cpu_affinity/pcpu_id", str(pcpu_id), allocation_sos_vm_node)
