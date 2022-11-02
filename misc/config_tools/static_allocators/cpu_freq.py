#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import acrn_config_utilities, board_cfg_lib
from acrn_config_utilities import get_node

# CPU frequency dependency
# Some CPU cores may share the same clock domain/group with others, which makes them always run at
# the same frequency of the highest on in the group. Including those known conditions:
#   1. CPU in the clock domain described in ACPI _PSD.
#      Like _PSS, board_inspector extracted this data from Linux cpufreq driver
#      (see Linux document 'sysfs-devices-system-cpu' about freqdomain_cpus)
#   2. CPU hyper threads sharing the same physical core.
#      The data is extracted form apic id.
#   3. E-cores residents in the same topological group.
#      The data is extracted form CPU model type and apic id.
# CPU frequency dependency may have some impacts on our frequency limits.
#
# Returns a list that contains each CPU's "dependency data". The "dependency data" is also a list
# containing CPU_IDs that share frequency with the current one.
# e.g. CPU 8 is sharing with CPU 9,10,11, so dependency_data[8] = ['8', '9', '10', '11']
def get_dependency(board_etree):
    cpus = board_etree.xpath("//processors//thread")
    dep_ret = []
    for cpu in cpus:
        cpu_id = get_node("./cpu_id/text()", cpu)
        psd_cpus = [cpu_id]
        psd_cpus_list = get_node("./freqdomain_cpus/text()", cpu)
        if psd_cpus_list != None:
            psd_cpus = psd_cpus_list.split(' ')
        apic_id = int(get_node("./apic_id/text()", cpu)[2:], base=16)
        is_hybrid = (len(board_etree.xpath("//processors//capability[@id='hybrid']")) != 0)
        core_type = get_node("./core_type/text()", cpu)
        for other_cpu in cpus:
            other_cpu_id = get_node("./cpu_id/text()", other_cpu)
            if cpu_id != other_cpu_id:
                other_apic_id = int(get_node("./apic_id/text()", other_cpu)[2:], base=16)
                other_core_type = get_node("./core_type/text()", other_cpu)
                # threads at same core
                if (apic_id & ~1) == (other_apic_id & ~1):
                    psd_cpus.append(other_cpu_id)
                # e-cores in the same group. Infered from Atom cores share the same L2 cache
                share_cache = 0
                if is_hybrid and core_type == 'Atom' and other_core_type == 'Atom':
                    l2cache_nodes = board_etree.xpath("//caches/cache[@level='2']")
                    for l2cache in l2cache_nodes:
                        processors = l2cache.xpath("./processors/processor/text()")
                        if '{:#x}'.format(apic_id) in processors and '{:#x}'.format(other_apic_id) in processors:
                            share_cache = 1
                if share_cache == 1:
                    psd_cpus.append(other_cpu_id)

        if psd_cpus != None:
            psd_cpus = list(set(psd_cpus))
            psd_cpus.sort()
            dep_ret.insert(int(cpu_id), psd_cpus)
        else:
            dep_ret.insert(int(cpu_id), None)
    return dep_ret

# CPU frequency limits:
#
# Frequency limits is a per CPU data type. Hypervisor uses this data to quickly decide what performance
# level/p-state range it should apply.
#
# Those limits are decided by hardware and scenario config.
#
# When the CPU is assigned to a RTVM, we want to set its frequency fixed.(to get more certainty
# in latency). To do this, we just let highest_lvl = lowest_lvl.
# Some CPU cores' frequency may be linked to each other in a frequency domain or group(eg. e-cores in a group).
# In this condition, RTVM's CPU frequency might be influenced by other VMs. So we fix all of them to the value of
# the RTVM's CPU frequence.
#
# Both HWP and ACPI p-state are supported in ACRN CPU performance management. So here we generate two sets of
# data:
#
#   - 'limit_guaranteed_lvl', 'limit_highest_lvl' and 'limit_lowest_lvl' are for HWP. The values represent
#     HWP performance level used in IA32_HWP_CAPABILITIES and IA32_HWP_REQUEST.
#
#   - 'limit_nominal_pstate', 'limit_highest_pstate' and 'limit_lowest_pstate' are for ACPI p-state.
#     Those values represent the performance state's index P(x).
#     ACPI p-state does not define a 'guaranteed p-state' or a 'base p-state'. Here the 'nominal p-state' refers
#     to a state whose frequency is closest to the max none-turbo frequency.
def alloc_limits(board_etree, scenario_etree, allocation_etree):
    cpu_has_eist = (len(board_etree.xpath("//processors//capability[@id='est']")) != 0)
    cpu_has_hwp = (len(board_etree.xpath("//processors//capability[@id='hwp_supported']")) != 0)
    cpu_has_turbo = (len(board_etree.xpath("//processors//capability[@id='turbo_boost_available']")) != 0)
    rtvm_cpus = scenario_etree.xpath(f"//vm[vm_type = 'RTVM']//cpu_affinity//pcpu_id/text()")
    cpus = board_etree.xpath("//processors//thread")

    for cpu in cpus:
        cpu_id = get_node("./cpu_id/text()", cpu)
        if cpu_has_hwp:
            guaranteed_performance_lvl = get_node("./guaranteed_performance_lvl/text()", cpu)
            highest_performance_lvl = get_node("./highest_performance_lvl/text()", cpu)
            lowest_performance_lvl = get_node("./lowest_performance_lvl/text()", cpu)
            if cpu_id in rtvm_cpus:
                # for CPUs in RTVM, fix to base performance
                limit_lowest_lvl = guaranteed_performance_lvl
                limit_highest_lvl = guaranteed_performance_lvl
                limit_guaranteed_lvl = guaranteed_performance_lvl
            elif cpu_has_turbo:
                limit_lowest_lvl = lowest_performance_lvl
                limit_highest_lvl = highest_performance_lvl
                limit_guaranteed_lvl = guaranteed_performance_lvl
            else:
                limit_lowest_lvl = lowest_performance_lvl
                limit_highest_lvl = guaranteed_performance_lvl
                limit_guaranteed_lvl = guaranteed_performance_lvl
        else:
                limit_lowest_lvl = 1
                limit_highest_lvl = 0xff
                limit_guaranteed_lvl = 0xff

        cpu_node = acrn_config_utilities.append_node(f"//hv/cpufreq/CPU", None, allocation_etree, id = cpu_id)
        limit_node = acrn_config_utilities.append_node("./limits", None, cpu_node)
        acrn_config_utilities.append_node("./limit_guaranteed_lvl", limit_guaranteed_lvl, limit_node)
        acrn_config_utilities.append_node("./limit_highest_lvl", limit_highest_lvl, limit_node)
        acrn_config_utilities.append_node("./limit_lowest_lvl", limit_lowest_lvl, limit_node)

        limit_highest_pstate = 0
        limit_nominal_pstate = 0
        limit_lowest_pstate = 0
        if cpu_has_eist:
            mntr = board_etree.xpath("//processors//attribute[@id='max_none_turbo_ratio']/text()")
            none_turbo_p = 0
            p_count = board_cfg_lib.get_p_state_count()
            if len(mntr) != 0:
                none_turbo_p = board_cfg_lib.get_p_state_index_from_ratio(int(mntr[0]))
            if p_count != 0:
                # P0 is the highest stat
                if cpu_id in rtvm_cpus:
                    # for CPUs in RTVM, fix to nominal performance(max none turbo frequency if turbo on)
                    if cpu_has_turbo:
                        limit_highest_pstate = none_turbo_p
                        limit_nominal_pstate = none_turbo_p
                        limit_lowest_pstate = none_turbo_p
                    else:
                        limit_highest_pstate = 0
                        limit_nominal_pstate = 0
                        limit_lowest_pstate = 0
                else:
                    if cpu_has_turbo:
                        limit_highest_pstate = 0
                        limit_nominal_pstate = none_turbo_p
                        limit_lowest_pstate = p_count -1
                    else:
                        limit_highest_pstate = 0
                        limit_nominal_pstate = 0
                        limit_lowest_pstate = p_count -1

        acrn_config_utilities.append_node("./limit_nominal_pstate", str(limit_nominal_pstate), limit_node)
        acrn_config_utilities.append_node("./limit_highest_pstate", str(limit_highest_pstate), limit_node)
        acrn_config_utilities.append_node("./limit_lowest_pstate", str(limit_lowest_pstate), limit_node)

    # Let CPUs in the same frequency dependency group have the same limits. So that RTVM's frequency can be fixed
    dep_info = get_dependency(board_etree)
    for alloc_cpu in allocation_etree.xpath("//cpufreq/CPU"):
        dependency_cpus = dep_info[int(alloc_cpu.attrib['id'])]
        if get_node("./limits", alloc_cpu) != None:
            highest_lvl = int(get_node(".//limit_highest_lvl/text()", alloc_cpu))
            lowest_lvl = int(get_node(".//limit_lowest_lvl/text()", alloc_cpu))
            highest_pstate = int(get_node(".//limit_highest_pstate/text()", alloc_cpu))
            lowest_pstate = int(get_node(".//limit_lowest_pstate/text()", alloc_cpu))

            for dep_cpu_id in dependency_cpus:
                dep_highest_lvl = int(get_node(f"//cpufreq/CPU[@id={dep_cpu_id}]//limit_highest_lvl/text()", allocation_etree))
                dep_lowest_lvl = int(get_node(f"//cpufreq/CPU[@id={dep_cpu_id}]//limit_lowest_lvl/text()", allocation_etree))
                if highest_lvl > dep_highest_lvl:
                    highest_lvl = dep_highest_lvl
                if lowest_lvl < dep_lowest_lvl:
                    lowest_lvl = dep_lowest_lvl
                dep_highest_pstate = int(get_node(f"//cpufreq/CPU[@id={dep_cpu_id}]//limit_highest_pstate/text()", allocation_etree))
                dep_lowest_pstate = int(get_node(f"//cpufreq/CPU[@id={dep_cpu_id}]//limit_lowest_pstate/text()", allocation_etree))
                if highest_pstate < dep_highest_pstate:
                    highest_pstate = dep_highest_pstate
                if lowest_pstate > dep_lowest_pstate:
                    lowest_pstate = dep_lowest_pstate

            acrn_config_utilities.update_text("./limits/limit_highest_lvl", str(highest_lvl), alloc_cpu, True)
            acrn_config_utilities.update_text("./limits/limit_lowest_lvl", str(lowest_lvl), alloc_cpu, True)
            acrn_config_utilities.update_text("./limits/limit_highest_pstate", str(highest_pstate), alloc_cpu, True)
            acrn_config_utilities.update_text("./limits/limit_lowest_pstate", str(lowest_pstate), alloc_cpu, True)

def fn(board_etree, scenario_etree, allocation_etree):
    acrn_config_utilities.append_node("/acrn-config/hv/cpufreq", None, allocation_etree)
    alloc_limits(board_etree, scenario_etree, allocation_etree)
