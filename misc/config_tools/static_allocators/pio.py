#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os, logging
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, lib.lib

def alloc_pio(pio_list):
    try:
        base = pio_list[0]
        remove_pio(pio_list, base)
        return base
    except IndexError as e:
        raise lib.error.ResourceError("Cannot allocate a pio base, the available pio base list:{}, {}".format(e, pio_list))

def remove_pio(pio_list, base):
    try:
        pio_list.remove(base)
    except ValueError as e:
        raise ValueError("Cannot remove a pio base:{} from the available pio base list:{}, {}". format(base, e, pio_list)) from e

def assign_legacy_vuart_io_port(vm_node, legacy_vuart_id):
    legacy_vuart_base = ""
    legacy_vuart_node_base_text = common.get_node(f"./legacy_vuart[@id = '{legacy_vuart_id}']/base/text()", vm_node)
    if legacy_vuart_node_base_text == 'COM1_BASE' or legacy_vuart_node_base_text == 'SERVICE_VM_COM1_BASE':
        legacy_vuart_base = '0x3F8'
    elif legacy_vuart_node_base_text == 'COM2_BASE' or legacy_vuart_node_base_text == 'SERVICE_VM_COM2_BASE':
        legacy_vuart_base = '0x2F8'
    elif legacy_vuart_node_base_text == 'COM3_BASE' or legacy_vuart_node_base_text == 'SERVICE_VM_COM3_BASE':
        legacy_vuart_base = '0x3E8'
    elif legacy_vuart_node_base_text == 'COM4_BASE' or legacy_vuart_node_base_text == 'SERVICE_VM_COM4_BASE':
        legacy_vuart_base = '0x2E8'
    return legacy_vuart_base

def create_vuart_base_node(etree, vm_id, vuart_id, vuart_base):
    vm_node = common.get_node(f"/acrn-config/vm[@id = '{vm_id}']", etree)
    if vm_node is None:
        vm_node = common.append_node("/acrn-config/vm", None, etree, id = vm_id)
    vuart_node = common.get_node(f"./legacy_vuart[@id = '{vuart_id}']", vm_node)
    if vuart_node is None:
        vuart_node = common.append_node("./legacy_vuart", None, vm_node, id = vuart_id)
    if common.get_node(f"./base", vuart_node) is None:
        common.append_node(f"./base", vuart_base, vuart_node)

def fn(board_etree, scenario_etree, allocation_etree):
    native_ttys = lib.lib.get_native_ttys()
    hv_debug_console = lib.lib.parse_hv_console(scenario_etree)

    vm_node_list = scenario_etree.xpath("//vm")
    for vm_node in vm_node_list:
        vm_type = common.get_node("./vm_type/text()", vm_node)
        legacy_vuart_base = ""
        legacy_vuart_id_list = vm_node.xpath("legacy_vuart[base != 'INVALID_COM_BASE']/@id")
        for legacy_vuart_id in legacy_vuart_id_list:
            if legacy_vuart_id == '0' and  vm_type == "SERVICE_VM":
                if hv_debug_console in native_ttys.keys():
                    if native_ttys[hv_debug_console]['type'] == "portio":
                        legacy_vuart_base = native_ttys[hv_debug_console]['base']
                    else:
                        legacy_vuart_base = assign_legacy_vuart_io_port(vm_node, legacy_vuart_id)
                else:
                    raise lib.error.ResourceError(f"{hv_debug_console} is not in the native environment! The ttyS available are: {native_ttys.keys()}")
            else:
                legacy_vuart_base = assign_legacy_vuart_io_port(vm_node, legacy_vuart_id)

            if legacy_vuart_base != "":
                create_vuart_base_node(allocation_etree, common.get_node("./@id", vm_node), legacy_vuart_id, legacy_vuart_base)
