#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#
import argparse
import lxml.etree

#vuart devices name is configured to start from /dev/ttyS8
START_VUART_DEV_NAME_NO = 8
VUART_DEV_NAME_NUM = 8
stadard_uart_port = {'0x3F8', '0x2F8', '0x3E8', '0x2E8'}
UART_IRQ_BAUD = " irq 0 uart 16550A baud_base 115200"

def find_non_standard_uart(vm):
    uart_list = []
    for vuart in vm.iter(tag = 'legacy_vuart'):
        base = vuart.find('base').text
        if base not in stadard_uart_port:
            uart_list.append(vuart)
    return uart_list

def main(args):
    """
    Generate serial configuration file for service VM
    :param args: command line args
    """
    scenario_etree = lxml.etree.parse(args.scenario)
    allocation_etree = lxml.etree.parse(args.allocation)
    vuart_target_vmid = [0] * VUART_DEV_NAME_NUM

    vm_list = scenario_etree.xpath("//vm[load_order = 'SERVICE_VM']")
    for vm in vm_list:
        for legacy_vuart in vm.iter(tag = 'legacy_vuart'):
            if legacy_vuart.find('target_vm_id') != None:
                user_vm_id = legacy_vuart.find('target_vm_id').text
                legacy_vuartid = int(legacy_vuart.attrib["id"])
                vuart_target_vmid[legacy_vuartid] = user_vm_id

    vm_list = allocation_etree.xpath("//vm[load_order = 'SERVICE_VM']")
    for vm in vm_list:
        vuart_list = find_non_standard_uart(vm)
        if len(vuart_list) != 0:
            with open(args.out, "w+") as config_f:
                for uart_start_num, vuart in enumerate(vuart_list, start=START_VUART_DEV_NAME_NO):
                    base = " port " + vuart.find('base').text
                    vuart_id = int(vuart.attrib["id"])
                    vm_id_note = "# User_VM_id: " + str(vuart_target_vmid[vuart_id]) + '\n'
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
