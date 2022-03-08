#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#
import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))

import argparse
import lxml.etree
import common

#vuart devices name is configured to start from /dev/ttyS8
START_VUART_DEV_NAME_NO = 8
VUART_DEV_NAME_NUM = 8
stadard_uart_port = {'0x3F8', '0x2F8', '0x3E8', '0x2E8'}
UART_IRQ_BAUD = " irq 0 uart 16550A baud_base 115200"

def find_non_standard_uart(vm, scenario_etree):
    uart_list = []
    vmname = common.get_node("./name/text()", vm)

    connection_list = scenario_etree.xpath(f"//vuart_connection[endpoint/vm_name = '{vmname}']")
    for connection in connection_list:
        type = common.get_node(f"./type/text()", connection)

        if (type != "legacy") :
            continue

        port = common.get_node(f".//endpoint[vm_name = '{vmname}']/io_port/text()", connection)
        if port not in stadard_uart_port:
           uart_list.append(connection)

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
        vmname = common.get_node("./name/text()", vm)
        for connection in scenario_etree.xpath(f"//vuart_connection[endpoint/vm_name = '{vmname}']"):
            vm_name = common.get_node(f".//endpoint[vm_name != '{vmname}']/vm_name/text()", connection)
            for target_vm in scenario_etree.xpath(f"//vm[name = '{vm_name}']"):
                vuart_target_vmid[connection.find('name').text] = target_vm.attrib["id"]

    vm_list = scenario_etree.xpath("//vm[load_order = 'SERVICE_VM']")
    for vm in vm_list:
        vuart_list = find_non_standard_uart(vm, scenario_etree)
        vmname = common.get_node("./name/text()", vm)
        if len(vuart_list) != 0:
            with open(args.out, "w+") as config_f:
                for uart_start_num, vuart in enumerate(vuart_list, start=START_VUART_DEV_NAME_NO):
                    port = common.get_node(f".//endpoint[vm_name = '{vmname}']/io_port/text()", vuart)
                    base = " port " + str(port)
                    connection_name = vuart.find('name').text
                    vm_id_note = "# User_VM_id: " + str(vuart_target_vmid[connection_name]) + '\n'
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
