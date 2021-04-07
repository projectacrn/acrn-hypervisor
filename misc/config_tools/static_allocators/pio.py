#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, lib.lib

VALID_PIO = ['0x3F8', '0x2F8', '0x3E8', '0x2E8']

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

def create_vuart_base_node(etree, vm_id, vuart_id, vuart_base):
    allocation_sos_vm_node = common.get_node(f"/acrn-config/vm[@id = '{vm_id}']", etree)
    if allocation_sos_vm_node is None:
        allocation_sos_vm_node = common.append_node("/acrn-config/vm", None, etree, id = vm_id)
    if common.get_node("./vm_type", allocation_sos_vm_node) is None:
        common.append_node("./vm_type", "SOS_VM", allocation_sos_vm_node)
    if common.get_node(f"./legacy_vuart[@id = '{vuart_id}']", allocation_sos_vm_node) is None:
        common.append_node("./legacy_vuart", None, allocation_sos_vm_node, id = vuart_id)

    common.append_node(f"./legacy_vuart[@id = '{vuart_id}']/base", vuart_base, allocation_sos_vm_node)

def fn(board_etree, scenario_etree, allocation_etree):
    native_ttys = lib.lib.get_native_ttys()
    pio_list = [base for base in VALID_PIO if all(native_ttys[tty]['base'] != base for tty in native_ttys.keys())]
    # This pio_list is workaround. Since there are whl-ipc-i7 and whl-ipc-i5 which occupy all valid pio ports.
    # It would fail to allocate pio base for enabled sos legacy vuart1. In that case, we allow vuart1 take one pio
    # base which is used in native.
    full = False
    if len(pio_list) == 0:
        full = True
        pio_list = VALID_PIO

    vuart_valid = ['ttyS0', 'ttyS1', 'ttyS2', 'ttyS3']
    hv_debug_console = lib.lib.parse_hv_console(scenario_etree)

    scenario_sos_vm_node = common.get_node("//vm[vm_type = 'SOS_VM']", scenario_etree)
    if scenario_sos_vm_node is not None:
        vm_id = common.get_node("./@id", scenario_sos_vm_node)
        if common.get_node("./legacy_vuart[@id = '0']/base/text()", scenario_sos_vm_node) != "INVALID_COM_BASE":
            vuart0_base = ""
            if hv_debug_console in vuart_valid and hv_debug_console in native_ttys.keys() and native_ttys[hv_debug_console]['type'] == "portio":
                vuart0_base = native_ttys[hv_debug_console]['base']
                if vuart0_base in pio_list:
                    remove_pio(pio_list, vuart0_base)
            else:
                vuart0_base = alloc_pio(pio_list)
                if full:
                    common.print_yel("All available pio bases are used by native fully. '{}' is taken by sos legacy vuart 0.".format(vuart0_base), warn=True)

            create_vuart_base_node(allocation_etree, str(vm_id), "0", vuart0_base)

        if common.get_node("./legacy_vuart[@id = '1']/base/text()", scenario_sos_vm_node) != "INVALID_COM_BASE":
            vuart1_base = alloc_pio(pio_list)
            if full:
                common.print_yel("All available pio bases are used by native fully. '{}' is taken by sos legacy vuart 1.".format(vuart1_base), warn=True)

            create_vuart_base_node(allocation_etree, str(vm_id), "1", vuart1_base)
