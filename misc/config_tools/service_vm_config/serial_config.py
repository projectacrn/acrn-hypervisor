#!/usr/bin/env python3
#
# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#
import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))

import argparse
import lxml.etree
import acrn_config_utilities
from acrn_config_utilities import get_node

#vuart devices name is configured to start from /dev/ttyS8
START_VUART_DEV_NAME_NO = 8
VUART_DEV_NAME_NUM = 8
stadard_uart_port = {'0x3F8', '0x2F8', '0x3E8', '0x2E8'}
UART_IRQ_BAUD = " irq 0 uart 16550A baud_base 115200"

def find_non_standard_uart(vm, scenario_etree, allocation_etree):
    uart_list = []
    vmname = get_node("./name/text()", vm)

    connection_list0 = scenario_etree.xpath(f"//vuart_connection[endpoint/vm_name = '{vmname}']")
    connection_list1 = allocation_etree.xpath(f"//vuart_connection[endpoint/vm_name = '{vmname}']")
    for connection in (connection_list0 + connection_list1):
        type = get_node(f"./type/text()", connection)

        if (type != "legacy") :
            continue
        port = get_node(f".//endpoint[vm_name = '{vmname}']/io_port/text()", connection)
        if port not in stadard_uart_port:
            target_vm_name = get_node(f".//endpoint[vm_name != '{vmname}']/vm_name/text()", connection)
            target_vm_id = get_node(f"//vm[name = '{target_vm_name}']/@id", scenario_etree)
            uart_list.append({"io_port" : port, "target_vm_id" : target_vm_id})
    return uart_list

def main(args):
    """
    Generate serial configuration file for service VM
    :param args: command line args
    """
    scenario_etree = lxml.etree.parse(args.scenario)
    allocation_etree = lxml.etree.parse(args.allocation)
    vuart_target_vmid = {}

    vm_list = scenario_etree.xpath("//vm[load_order = 'SERVICE_VM']")
    for vm in vm_list:
        vuart_list = find_non_standard_uart(vm, scenario_etree, allocation_etree)
        vmname = get_node("./name/text()", vm)
        if len(vuart_list) != 0:
            with open(args.out, "w+") as config_f:
                for uart_start_num, vuart in enumerate(vuart_list, start=START_VUART_DEV_NAME_NO):
                    base = " port " + vuart["io_port"]
                    vm_id_note = "# User_VM_id: " + str(vuart["target_vm_id"])+ '\n'
                    config_f.write(vm_id_note)
                    conf = "/dev/ttyS" + str(uart_start_num) + base + UART_IRQ_BAUD + '\n'
                    config_f.write(conf)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--allocation", help="the XML file summarizing resource allocated by config tool")
    parser.add_argument("--scenario", help="the XML file specifying the scenario to be set up")
    parser.add_argument("--out", help="location of the output serial configuration file")
    args = parser.parse_args()
    main(args)
