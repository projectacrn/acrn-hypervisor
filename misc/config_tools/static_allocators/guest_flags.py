#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os, re
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import acrn_config_utilities, lib.error, lib.lib
from collections import namedtuple
from acrn_config_utilities import get_node

class GuestFlagPolicy(namedtuple("GuestFlagPolycy", ["condition", "guest_flag"])):
    pass

policies = [
    GuestFlagPolicy(".//lapic_passthrough = 'y'", "GUEST_FLAG_LAPIC_PASSTHROUGH"),
    GuestFlagPolicy(".//io_completion_polling = 'y'", "GUEST_FLAG_IO_COMPLETION_POLLING"),
    GuestFlagPolicy(".//virtual_cat_support = 'y'", "GUEST_FLAG_VCAT_ENABLED"),
    GuestFlagPolicy(".//secure_world_support = 'y'", "GUEST_FLAG_SECURE_WORLD_ENABLED"),
    GuestFlagPolicy(".//hide_mtrr_support = 'y'", "GUEST_FLAG_HIDE_MTRR"),
    GuestFlagPolicy(".//nested_virtualization_support = 'y'", "GUEST_FLAG_NVMX_ENABLED"),
    GuestFlagPolicy(".//security_vm = 'y'", "GUEST_FLAG_SECURITY_VM"),
    GuestFlagPolicy(".//vm_type = 'RTVM'", "GUEST_FLAG_RT"),
    GuestFlagPolicy(".//vm_type = 'RTVM' and .//load_order = 'PRE_LAUNCHED_VM' and //hv/BUILD_TYPE= 'debug'", "GUEST_FLAG_PMU_PASSTHROUGH"),
    GuestFlagPolicy(".//vm_type = 'TEE_VM'", "GUEST_FLAG_TEE"),
    GuestFlagPolicy(".//vm_type = 'REE_VM'", "GUEST_FLAG_REE"),
]

def fn(board_etree, scenario_etree, allocation_etree):
    for vm_node in scenario_etree.xpath("//vm"):
        vm_id = vm_node.get('id')
        allocation_vm_node = get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
        if allocation_vm_node is None:
            allocation_vm_node = acrn_config_utilities.append_node("/acrn-config/vm", None, allocation_etree, id = vm_id)
        for policy in policies:
            if vm_node.xpath(policy.condition):
                acrn_config_utilities.append_node("./guest_flags/guest_flag", str(policy.guest_flag), allocation_vm_node)
        acrn_config_utilities.append_node("./guest_flags/guest_flag",'GUEST_FLAG_STATIC_VM', allocation_vm_node)
        if board_etree.xpath("//capability[@id = 'hwp_supported']") and get_node(".//own_pcpu/text()", vm_node) == "y" and get_node(".//os_type/text()", vm_node) != "Windows OS":
            acrn_config_utilities.append_node("./guest_flags/guest_flag", 'GUEST_FLAG_VHWP', allocation_vm_node)
