#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os, re
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, lib.error, lib.lib

# Constants for device name prefix
IVSHMEM = "IVSHMEM"
VUART = "VUART"

# Exception bdf list
# Some hardware drivers' bdf is hardcoded, the bdf cannot be changed even it is passtrhough devices.
HARDCODED_BDF_LIST = ["00:0e.0"]

def find_unused_bdf(used_bdf):
    # never assign 0:00.0 to any emulated devices, it's reserved for pci hostbridge
    for dev in range(0x1, 0x20):
        bdf = lib.lib.BusDevFunc(bus=0x00, dev=dev, func=0x0)
        if all((bdf.dev != in_use_bdf.dev for in_use_bdf in used_bdf)):
            return bdf
    raise lib.error.ResourceError(f"Cannot find free bdf, used bdf: {sorted(used_bdf)}")

def insert_vuart_to_dev_dict(scenario_etree, devdict, used):
    console_vuart =  scenario_etree.xpath(f"./console_vuart[base != 'INVALID_PCI_BASE']/@id")
    communication_vuarts = scenario_etree.xpath(f".//communication_vuart[base != 'INVALID_PCI_BASE']/@id")
    for vuart_id in console_vuart:
        free_bdf = find_unused_bdf(used)
        devdict[f"{VUART}_{vuart_id}"] = free_bdf
        used.append(free_bdf)
    for vuart_id in communication_vuarts:
        free_bdf = find_unused_bdf(used)
        devdict[f"{VUART}_{vuart_id}"] = free_bdf
        used.append(free_bdf)

def insert_ivsheme_to_dev_dict(scenario_etree, devdict, vm_id, used):
    shmem_regions = lib.lib.get_shmem_regions(scenario_etree)
    if vm_id not in shmem_regions:
        return
    shmems = shmem_regions.get(vm_id)
    for shm in shmems.values():
        free_bdf = find_unused_bdf(used)
        devdict[f"{IVSHMEM}_{shm.get('id')}"] = free_bdf
        used.append(free_bdf)

def insert_pt_devs_to_dev_dict(vm_node_etree, devdict, used):
    """
    Assign an unused bdf to each of passtrhough devices.
    If a passtrhough device's bdf is in the list of HARDCODED_BDF_LIST, this device should apply the same bdf as native one.
    Calls find_unused_bdf to assign an unused bdf for the rest of passtrhough devices except the ones in HARDCODED_BDF_LIST.
    """
    pt_devs = vm_node_etree.xpath(f".//pci_dev/text()")
    # assign the bdf of the devices in HARDCODED_BDF_LIST
    for pt_dev in pt_devs:
        bdf_string = pt_dev.split()[0]
        if bdf_string in HARDCODED_BDF_LIST:
            bdf = lib.lib.BusDevFunc.from_str(bdf_string)
            dev_name = str(bdf)
            devdict[dev_name] = bdf
            used.append(bdf)

    # remove the pt_dev nodes which are in HARDCODED_BDF_LIST
    pt_devs = [pt_dev for pt_dev in pt_devs if lib.lib.BusDevFunc.from_str(bdf_string) not in used]

    # call find_unused_bdf to assign an unused bdf for other passthrough devices except the ones in HARDCODED_BDF_LIST
    for pt_dev in pt_devs:
        bdf = lib.lib.BusDevFunc.from_str(pt_dev.split()[0])
        free_bdf = find_unused_bdf(used)
        dev_name = str(bdf)
        devdict[dev_name] = free_bdf
        used.append(free_bdf)

def get_devs_bdf_native(board_etree):
    """
    Get all pci devices' bdf in native environment.
    return: list of pci devices' bdf
    """
    nodes = board_etree.xpath(f"//bus[@type = 'pci' and @address = '0x0']/device[@address]")
    dev_list = []
    for node in nodes:
        address = node.get('address')
        bus = int(common.get_node("../@address", node), 16)
        dev = int(address, 16) >> 16
        func = int(address, 16) & 0xffff
        dev_list.append(lib.lib.BusDevFunc(bus = bus, dev = dev, func = func))
    return dev_list

def get_devs_bdf_passthrough(scenario_etree):
    """
    Get all pre-launched vms' passthrough devices' bdf in native environment.
    return: list of passtrhough devices' bdf.
    """
    dev_list = []
    for vm_type in lib.lib.PRE_LAUNCHED_VMS_TYPE:
        pt_devs = scenario_etree.xpath(f"//vm[vm_type = '{vm_type}']/pci_devs/pci_dev/text()")
        for pt_dev in pt_devs:
            bdf = lib.lib.BusDevFunc.from_str(pt_dev.split()[0])
            dev_list.append(bdf)
    return dev_list

def create_device_node(allocation_etree, vm_id, devdict):
    for dev in devdict:
        dev_name = dev
        bdf = devdict.get(dev)
        vm_node = common.get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
        if vm_node is None:
            vm_node = common.append_node("/acrn-config/vm", None, allocation_etree, id = vm_id)
        dev_node = common.get_node(f"./device[@name = '{dev_name}']", vm_node)
        if dev_node is None:
            dev_node = common.append_node("./device", None, vm_node, name = dev_name)
        if common.get_node(f"./bus", dev_node) is None:
            common.append_node(f"./bus",  f"{bdf.bus:#04x}", dev_node)
        if common.get_node(f"./dev", dev_node) is None:
            common.append_node(f"./dev", f"{bdf.dev:#04x}", dev_node)
        if common.get_node(f"./func", dev_node) is None:
            common.append_node(f"./func", f"{bdf.func:#04x}", dev_node)

def fn(board_etree, scenario_etree, allocation_etree):
    vm_nodes = scenario_etree.xpath("//vm")
    for vm_node in vm_nodes:
        vm_id = vm_node.get('id')
        devdict = {}
        used = []
        vm_type = common.get_node("./vm_type/text()", vm_node)
        if vm_type is not None and lib.lib.is_post_launched_vm(vm_type):
            continue

        if vm_type is not None and lib.lib.is_sos_vm(vm_type):
            native_used = get_devs_bdf_native(board_etree)
            passthrough_used = get_devs_bdf_passthrough(scenario_etree)
            used = [bdf for bdf in native_used if bdf not in passthrough_used]
            if common.get_node("//@board", scenario_etree) == "tgl-rvp":
                used.append(lib.lib.BusDevFunc(bus = 0, dev = 1, func = 0))

        insert_vuart_to_dev_dict(vm_node, devdict, used)
        insert_ivsheme_to_dev_dict(scenario_etree, devdict, vm_id, used)
        insert_pt_devs_to_dev_dict(vm_node, devdict, used)
        create_device_node(allocation_etree, vm_id, devdict)
