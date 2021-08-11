#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, board_cfg_lib, lib.error, lib.lib
import re
from collections import defaultdict
from itertools import combinations

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

def alloc_sos_vuart_irqs(board_etree, scenario_etree, allocation_etree):
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

def get_irqs_of_device(device_node):
    irqs = set()

    # IRQs in ACPI
    for res in device_node.xpath("resource[@type='irq']"):
        irqs.update(set(map(int, res.get("int").split(", "))))

    # PCI interrupt pin
    for res in device_node.xpath("resource[@type='interrupt_pin']"):
        irq = res.get("source", None)
        if irq is not None:
            irqs.add(int(irq))

    return irqs

def alloc_device_irqs(board_etree, scenario_etree, allocation_etree):
    service_vm_id = -1
    irq_allocation = defaultdict(lambda: defaultdict(lambda: []))  # vm_id -> irq -> [device]

    # Collect the list of devices that have to use INTx, excluding legacy UART which is to be emulated.
    device_nodes = set(board_etree.xpath("//device[count(resource[@type='irq' or @type='interrupt_pin']) > 0 and count(capability[@id='MSI' or @id='MSI-X']) = 0]"))
    uart_nodes = set(board_etree.xpath("//device[@id='PNP0501']"))
    device_nodes -= uart_nodes

    #
    # Identify the interrupt lines each pre-launched VM uses
    #
    for vm in scenario_etree.xpath("//vm"):
        vm_type = vm.find("vm_type").text
        vm_id = int(vm.get("id"))
        if lib.lib.is_pre_launched_vm(vm_type):
            pt_intx_text = common.get_node("pt_intx/text()", vm)
            if pt_intx_text is not None:
                pt_intx_mapping = dict(eval(f"[{pt_intx_text.replace(')(', '), (')}]"))
                for irq in pt_intx_mapping.keys():
                    irq_allocation[vm_id][irq].append("(Explicitly assigned in scenario configuration)")
            for pci_dev in vm.xpath("pci_devs/pci_dev/text()"):
                bdf = lib.lib.BusDevFunc.from_str(pci_dev.split(" ")[0])
                address = hex((bdf.dev << 16) | (bdf.func))
                device_node = common.get_node(f"//bus[@address='{hex(bdf.bus)}']/device[@address='{address}']", board_etree)
                if device_node in device_nodes:
                    irqs = get_irqs_of_device(device_node)
                    for irq in irqs:
                        irq_allocation[vm_id][irq].append(pci_dev)
                    device_nodes.discard(device_node)

            # Raise error when any pre-launched VM with LAPIC passthrough requires any interrupt line.
            lapic_passthru_flag = common.get_node("guest_flags[guest_flag='GUEST_FLAG_LAPIC_PASSTHROUGH']", vm)
            if lapic_passthru_flag is not None and irq_allocation[vm_id]:
                for irq, devices in irq_allocation[vm_id].items():
                    print(f"Interrupt line {irq} is used by the following device(s).")
                    for device in devices:
                        print(f"\t{device}")
                raise lib.error.ResourceError(f"Pre-launched VM {vm_id} with LAPIC_PASSTHROUGH flag cannot use interrupt lines.")
        elif lib.lib.is_sos_vm(vm_type):
            service_vm_id = vm_id

    #
    # Detect interrupt line conflicts
    #
    conflicts = defaultdict(lambda: defaultdict(lambda: set())) # irq -> vm_id -> devices

    # If a service VM exists, collect its interrupt lines as well
    if service_vm_id >= 0:
        # Collect the interrupt lines that may be used by the service VM
        for device_node in device_nodes:
            acpi_object = device_node.find("acpi_object")
            description = ""
            if acpi_object is not None:
                description = acpi_object.text
            description = device_node.get("description", description)

            # Guess BDF of the device
            bus = device_node.getparent()
            if bus.tag == "bus" and bus.get("type") == "pci" and device_node.get("address") is not None:
                bus_number = int(bus.get("address"), 16)
                address = int(device_node.get("address"), 16)
                device_number = address >> 16
                function_number = address & 0xffff
                description = f"{bus_number:02x}:{device_number:02x}.{function_number} {description}"

                for irq in get_irqs_of_device(device_node):
                    irq_allocation[service_vm_id][irq].append(description)

    # Identify and report conflicts among interrupt lines of the VMs
    for vm1, vm2 in combinations(irq_allocation.keys(), 2):
        common_irqs = set(irq_allocation[vm1].keys()) & set(irq_allocation[vm2].keys())
        for irq in common_irqs:
            conflicts[irq][vm1].update(set(irq_allocation[vm1][irq]))
            conflicts[irq][vm2].update(set(irq_allocation[vm2][irq]))

    if conflicts:
        print("Interrupt line conflicts detected!")
        for irq, vm_devices in sorted(conflicts.items()):
            print(f"Interrupt line {irq} is shared by the following devices.")
            for vm_id, devices in vm_devices.items():
                for device in sorted(devices):
                    print(f"\tVM {vm_id}: {device}")
        raise lib.error.ResourceError(f"VMs have conflicting interrupt lines.")

    #
    # Dump allocations to allocation_etree. The virtual interrupt line is the same as the physical one unless otherwise
    # stated in the scenario configuration.
    #
    for vm_id, alloc in irq_allocation.items():
        vm_node = common.get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
        if vm_node is None:
            vm_node = common.append_node("/acrn-config/vm", None, allocation_etree, id = str(vm_id))
        pt_intx_text = common.get_node(f"//vm[@id='{vm_id}']/pt_intx/text()", scenario_etree)
        pt_intx_mapping = dict(eval(f"[{pt_intx_text.replace(')(', '), (')}]")) if pt_intx_text is not None else {}
        for irq, devs in alloc.items():
            for dev in devs:
                if dev.startswith("("):  # Allocation in the scenario configuration need not go to allocation.xml
                    continue
                bdf = dev.split(" ")[0]
                dev_name = f"PTDEV_{bdf}"
                dev_node = common.get_node(f"device[@name = '{dev_name}']", vm_node)
                if dev_node is None:
                    dev_node = common.append_node("./device", None, vm_node, name = dev_name)
                pt_intx_node = common.get_node(f"pt_intx", dev_node)
                virq = pt_intx_mapping.get(irq, irq)
                if pt_intx_node is None:
                    common.append_node(f"./pt_intx", f"({irq}, {virq})", dev_node)
                else:
                    pt_intx_node.text += f" ({irq}, {virq})"

def fn(board_etree, scenario_etree, allocation_etree):
    alloc_sos_vuart_irqs(board_etree, scenario_etree, allocation_etree)
    alloc_device_irqs(board_etree, scenario_etree, allocation_etree)
