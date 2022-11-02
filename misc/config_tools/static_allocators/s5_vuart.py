#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import acrn_config_utilities, lib.error
from acrn_config_utilities import get_node

# The COM1 was used for console vUART, so we alloc io_port frome COM2~COM4
service_port_list = list(range(0x9000, 0x9100, 8))

def create_s5_vuart_connection(allocation_etree, service_vm_name, service_vm_port, user_vm_name, user_vm_port):
    vuart_connections_node = get_node(f"/acrn-config/hv/vuart_connections", allocation_etree)
    if vuart_connections_node is None:
        vuart_connections_node = acrn_config_utilities.append_node("/acrn-config/hv/vuart_connections", None, allocation_etree)

    connection_name = service_vm_name + "_"  + user_vm_name

    vuart_connection_node = acrn_config_utilities.append_node(f"./vuart_connection", None, vuart_connections_node)
    acrn_config_utilities.append_node(f"./name", connection_name, vuart_connection_node)
    acrn_config_utilities.append_node(f"./type", "legacy", vuart_connection_node)

    service_vm_endpoint = acrn_config_utilities.append_node(f"./endpoint", None, vuart_connection_node)
    acrn_config_utilities.append_node(f"./vm_name", service_vm_name, service_vm_endpoint)
    acrn_config_utilities.append_node(f"./io_port", service_vm_port, service_vm_endpoint)

    user_vm_endpoint = acrn_config_utilities.append_node(f"./endpoint", None, vuart_connection_node)
    acrn_config_utilities.append_node(f"./vm_name", user_vm_name, user_vm_endpoint)
    acrn_config_utilities.append_node(f"./io_port", user_vm_port, user_vm_endpoint)

def get_console_vuart_port(scenario_etree, vm_name):
    port = get_node(f"//vm[name = '{vm_name}']/console_vuart/text()", scenario_etree)

    if port == "COM Port 1":
        port = "0x3F8U"
    elif port == "COM Port 2":
        port = "0x2F8U"
    elif port == "COM Port 3":
        port = "0x3E8U"
    elif port == "COM Port 4":
        port = "0x2E8U"
    else:
        port = "0x0U"

    return port

def alloc_free_port(scenario_etree, load_order, vm_name):
    port_list = scenario_etree.xpath(f"//endpoint[vm_name = '{vm_name}']/io_port/text()")
    console_port = get_console_vuart_port(scenario_etree, vm_name)
    if console_port is not None:
        port_list.append(console_port.replace("U", ""))

    if load_order == "SERVICE_VM":
        tmp_list = []
        for port in port_list:
            tmp_list.append(int(port, 16))

        global service_port_list
        service_port_list = list(set(service_port_list) - set(tmp_list))
        service_port_list.sort()
        port = hex(service_port_list[0])
        service_port_list.remove(service_port_list[0])
        return str(port).upper()
    else:
        return "0x2F8"

def alloc_vuart_connection_info(board_etree, scenario_etree, allocation_etree):
    user_vm_list = scenario_etree.xpath(f"//vm[load_order != 'SERVICE_VM']")
    service_vm_id = get_node(f"//vm[load_order = 'SERVICE_VM']/@id", scenario_etree)
    service_vm_name = get_node(f"//vm[load_order = 'SERVICE_VM']/name/text()", scenario_etree)

    if (service_vm_id is None) or (service_vm_name is None):
        return

    for index,vm_node in enumerate(user_vm_list):
        vm_id = get_node("./@id", vm_node)
        load_order = get_node("./load_order/text()", vm_node)
        user_vm_name = get_node(f"./name/text()", vm_node)
        service_vm_port = alloc_free_port(scenario_etree, "SERVICE_VM", user_vm_name)
        user_vm_port = alloc_free_port(scenario_etree, load_order, user_vm_name)

        create_s5_vuart_connection(allocation_etree, service_vm_name, service_vm_port, user_vm_name, user_vm_port)

def fn(board_etree, scenario_etree, allocation_etree):
    alloc_vuart_connection_info(board_etree, scenario_etree, allocation_etree)
