#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, board_cfg_lib, lib.error, lib.lib

LEGACY_IRQ_MAX = 16

def get_native_valid_irq():
    """
    This is get available irq from board info file
    :return: native available irq list
    """
    val_irq = []
    irq_info_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, "<AVAILABLE_IRQ_INFO>", "</AVAILABLE_IRQ_INFO>")
    for irq_string in irq_info_lines:
        val_irq = [int(x.strip()) for x in irq_string.split(',')]
    return val_irq

def alloc_irq(irq_list):
    try:
        irq = irq_list[0]
        remove_irq(irq_list, irq)
        return irq
    except IndexError as e:
        raise lib.error.ResourceError("Cannot allocate legacy irq, the available legacy irq list: {}, {}".format(e, irq_list)) from e

def remove_irq(irq_list, irq):
    try:
        irq_list.remove(irq)
    except ValueError as e:
        raise ValueError("Cannot remove irq:{} from available legacy irq list:{}, {}". format(irq, e, irq_list)) from e

def create_vuart_irq_node(etree, vm_id, vuart_id, irq):
    allocation_sos_vm_node = common.get_node(f"/acrn-config/vm[@id = '{vm_id}']", etree)
    if allocation_sos_vm_node is None:
        allocation_sos_vm_node = common.append_node("/acrn-config/vm", None, etree, id = vm_id)
    if common.get_node("./vm_type", allocation_sos_vm_node) is None:
        common.append_node("./vm_type", "SOS_VM", allocation_sos_vm_node)
    if common.get_node(f"./legacy_vuart[@id = '{vuart_id}']", allocation_sos_vm_node) is None:
        common.append_node("./legacy_vuart", None, allocation_sos_vm_node, id = vuart_id)

    common.append_node(f"./legacy_vuart[@id = '{vuart_id}']/irq", irq, allocation_sos_vm_node)

def fn(board_etree, scenario_etree, allocation_etree):
    irq_list = get_native_valid_irq()
    hv_debug_console = lib.lib.parse_hv_console(scenario_etree)
    native_ttys = lib.lib.get_native_ttys()
    vuart_valid = ['ttyS0', 'ttyS1', 'ttyS2', 'ttyS3']

    scenario_sos_vm_node = common.get_node("//vm[vm_type = 'SOS_VM']", scenario_etree)
    if scenario_sos_vm_node is not None:
        vm_id = common.get_node("./@id", scenario_sos_vm_node)
        if common.get_node("./legacy_vuart[@id = '0']/base/text()", scenario_sos_vm_node) != "INVALID_COM_BASE":
            vuart0_irq = -1
            if hv_debug_console in vuart_valid and hv_debug_console in native_ttys.keys() and native_ttys[hv_debug_console]['irq'] < LEGACY_IRQ_MAX:
                vuart0_irq = native_ttys[hv_debug_console]['irq']
            else:
                vuart0_irq = alloc_irq(irq_list)

            create_vuart_irq_node(allocation_etree, vm_id, "0", vuart0_irq)

        if common.get_node("./legacy_vuart[@id = '1']/base/text()", scenario_sos_vm_node) != "INVALID_COM_BASE":
            vuart1_irq = alloc_irq(irq_list)

            create_vuart_irq_node(allocation_etree, vm_id, "1", vuart1_irq)
