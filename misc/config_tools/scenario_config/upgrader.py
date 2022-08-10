#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import argparse, logging
import re
from functools import lru_cache, partialmethod
from collections import defaultdict, namedtuple
import lxml.etree as etree

from scenario_transformer import ScenarioTransformer

from pipeline import PipelineObject, PipelineStage, PipelineEngine
from lxml_loader import LXMLLoadStage
from schema_slicer import SlicingSchemaByVMTypeStage

class VirtualUartConnections:
    class VirtualUartEndpoint:
        # The BDF of PCI virtual UARTs starts from 00:10.0
        next_dev = defaultdict(lambda: 16)

        @classmethod
        def from_endpoint_definition(cls, element):
            # For v2.x style scenario XML, the name of the VM is a child of a `vm` node.
            vm_name = element.xpath("ancestor::vm/name/text()").pop()
            if "legacy" in element.tag:
                base = element.find("base").text
                io_port = \
                    "0x3F8" if base.endswith("COM1_BASE") else \
                    "0x2F8" if base.endswith("COM2_BASE") else \
                    "0x3E8" if base.endswith("COM3_BASE") else \
                    "0x2E8" if base.endswith("COM4_BASE") else \
                    base
                return cls(vm_name, io_port = io_port)
            else:
                dev = cls.next_dev[vm_name]
                cls.next_dev[vm_name] += 1
                return cls(vm_name, pci_bdf = f"00:{dev:02x}.0")

        def __init__(self, vm_name, io_port = None, pci_bdf = None):
            self.vm_name = vm_name
            self.io_port = io_port
            self.pci_bdf = pci_bdf

    class VirtualUartConnection:
        next_id = 1

        @classmethod
        def from_connection_definition(cls, element):
            name = element.find("name").text
            ty = element.find("type").text
            conn = cls(name = name, ty = ty)
            for endpoint in element.findall("endpoint"):
                vm_name_node = endpoint.find("vm_name")
                vm_name = vm_name_node.text if vm_name_node is not None else ""
                io_port_node = endpoint.find("io_port")
                io_port = io_port_node.text if io_port_node is not None else None
                vbdf_node = endpoint.find("vbdf")
                vbdf = vbdf_node.text if vbdf_node is not None else None
                conn.add_endpoint(VirtualUartConnections.VirtualUartEndpoint(vm_name, io_port, vbdf))
            return conn

        def __init__(self, name = None, ty = "legacy"):
            if name:
                self.name = name
            else:
                self.name = f"vUART connection {self.next_id}"
                self.__class__.next_id += 1
            self.ty = ty
            self.endpoints = []

        def add_endpoint(self, endpoint):
            self.endpoints.append(endpoint)

    def __init__(self):
        self.conns = []            # List of connections
        self.dangling_conns = {}   # (vm_id, vuart_id) -> conn whose target is the key

    def add_endpoint(self, element):
        """Parse the vUART endpoint definition in ACRN v2.x. Returns True if and only if the element is parsed properly."""

        try:
            key = (element.xpath("ancestor::vm/@id").pop(), element.xpath("@id").pop())
            if key in self.dangling_conns.keys():
                conn = self.dangling_conns.pop(key)
                conn.add_endpoint(self.VirtualUartEndpoint.from_endpoint_definition(element))
                self.conns.append(conn)
            else:
                ty = "legacy" if "legacy" in element.tag else "pci"
                conn = self.VirtualUartConnection(ty = ty)
                conn.add_endpoint(self.VirtualUartEndpoint.from_endpoint_definition(element))
                self.dangling_conns[(element.xpath("target_vm_id/text()").pop(), element.xpath("target_uart_id/text()").pop())] = conn

            return True
        except Exception as e:
            # Skip vUART endpoint definition not satisfying the schema. The discarded-data warnings will report those
            # unmigrated data.
            logging.debug(e)
            return False

    def add_connection(self, element):
        """Parse the vUART connection definition in ACRN v3.x"""
        self.conns.append(self.VirtualUartConnection.from_connection_definition(element))
        return True

    def format_xml_elements(self, xsd_element_node):
        new_parent_node = etree.Element(xsd_element_node.get("name"))
        for conn in self.conns:
            new_node = etree.Element("vuart_connection")
            etree.SubElement(new_node, "name").text = conn.name
            etree.SubElement(new_node, "type").text = conn.ty
            for endpoint in conn.endpoints:
                new_endpoint_node = etree.SubElement(new_node, "endpoint")
                etree.SubElement(new_endpoint_node, "vm_name").text = endpoint.vm_name
                if endpoint.io_port:
                    etree.SubElement(new_endpoint_node, "io_port").text = endpoint.io_port
                if endpoint.pci_bdf:
                    etree.SubElement(new_endpoint_node, "vbdf").text = endpoint.pci_bdf
            new_parent_node.append(new_node)
        return [new_parent_node]

class SharedMemoryRegions:
    class SharedMemoryRegion(namedtuple("SharedMemoryRegion", ["provided_by", "name", "size", "shared_vms"])):
        # The BDF of IVSHMEM PCI functions starts from 00:08.0
        next_dev = defaultdict(lambda: 8)
        nr_regions = 0

        @classmethod
        def from_encoding(cls, text, old_xml_etree):
            provided_by = "Device Model" if text.startswith("dm:/") else "Hypervisor"
            parts = [p.strip() for p in text[text.find("/") + 1 :].split(",")]
            name = parts[0]
            size = parts[1]
            shared_vm_ids = parts[2].split(":")

            shared_vms = []
            for vm_id in shared_vm_ids:
                vm_name_node = old_xml_etree.xpath(f"//vm[@id='{vm_id}']/name")
                if not vm_name_node:
                    logging.warning(f"VM {vm_id}, which is referred by shared memory region {name}, is not defined in the scenario.")
                    continue

                vm_name = vm_name_node[0].text
                dev = cls.next_dev[vm_name]
                cls.next_dev[vm_name] += 1
                shared_vms.append((vm_name, f"00:{dev:02x}.0"))

            return cls(provided_by, name, size, shared_vms)

        @classmethod
        def from_launch_xml_node(cls, node):
            text = node.text
            provided_by = "Device Model"
            parts = [p.strip() for p in text[text.find("/") + 1 :].split(",")]
            name = parts[0]
            size = parts[1]

            shared_vms = []
            vm_name = node.xpath("ancestor::user_vm/vm_name/text()")
            if vm_name:
                vm_name = vm_name[0]
                dev = cls.next_dev[vm_name]
                cls.next_dev[vm_name] += 1
                shared_vms.append((vm_name, f"00:{dev:02x}.0"))

            return cls(provided_by, name, size, shared_vms)

        @classmethod
        def from_xml_node(cls, node):
            cls.nr_regions += 1
            name = node.get("name") if "name" in node.keys() else \
                node.find("NAME").text if node.find("NAME") is not None else \
                f"shared_memory_region_{nr_regions}"
            provided_by = node.find("PROVIDED_BY").text if node.find("PROVIDED_BY") is not None else "Hypervisor"
            size = node.find("IVSHMEM_SIZE").text
            shared_vms = []
            for shared_vm_node in node.find("IVSHMEM_VMS"):
                vm_name = shared_vm_node.find("VM_NAME").text
                vbdf = shared_vm_node.find("VBDF").text
                shared_vms.append((vm_name, vbdf))
            return cls(provided_by, name, size, shared_vms)

        def extend(self, region):
            self.shared_vms.extend(region.shared_vms)

        def format_xml_element(self):
            node = etree.Element("IVSHMEM_REGION")
            etree.SubElement(node, "NAME").text = self.name
            etree.SubElement(node, "PROVIDED_BY").text = self.provided_by
            etree.SubElement(node, "IVSHMEM_SIZE").text = self.size

            vms_node = etree.SubElement(node, "IVSHMEM_VMS")
            for vm_name, vbdf in self.shared_vms:
                vm_node = etree.SubElement(vms_node, "IVSHMEM_VM")
                etree.SubElement(vm_node, "VM_NAME").text = vm_name
                etree.SubElement(vm_node, "VBDF").text = vbdf

            return node

    def __init__(self, old_xml_etree):
        self.old_xml_etree = old_xml_etree
        self.regions = {}

    def add_ivshmem_region(self, ivshmem_region_node):
        """Parse IVSHMEM_REGION nodes in either v2.x and v3.x format."""

        if len(ivshmem_region_node) == 0:
            if ivshmem_region_node.tag == "IVSHMEM_REGION" and ivshmem_region_node.text is not None:
                # ACRN v2.x scenario XML format
                region = self.SharedMemoryRegion.from_encoding(ivshmem_region_node.text, self.old_xml_etree)
                self.regions[region.name] = region
            elif ivshmem_region_node.tag == "shm_region":
                # ACRN v2.x launch XML format
                if ivshmem_region_node.text:
                    region = self.SharedMemoryRegion.from_launch_xml_node(ivshmem_region_node)
                    if region.name in self.regions.keys():
                        self.regions[region.name].extend(region)
                    else:
                        self.regions[region.name] = region
        else:
            # ACRN v3.x format
            region = self.SharedMemoryRegion.from_xml_node(ivshmem_region_node)
            self.regions[region.name] = region

    def format_xml_element(self):
        node = etree.Element("IVSHMEM")
        for region in self.regions.values():
            node.append(region.format_xml_element())
        return node

class VirtioDevices(object):
    def __init__(self, old_xml_etree):
        self.gpu = []
        self.blocks = []
        self.inputs = []
        self.networks = []
        self.consoles = []

    def console_encoding(self, console):
        if console.text is not None:
            use_type = "Virtio console" if console.text.startswith("@") else "Virtio serial port"
            backend_type = console.text.split(":")[0].replace("@", "")
            file_path = console.text.split("=")[1].split(":")[0] if "=" in console.text else None
        else:
            use_type = console.xpath("./use_type")[0].text if console.xpath("./use_type") else None
            backend_type = console.xpath("./backend_type")[0].text if console.xpath("./backend_type") else None
            file_path = console.xpath("./file_path")[0].text if console.xpath("./file_path") else None
        self.consoles.append((use_type, backend_type, file_path))

    def gpu_encoding(self, gpu):
        if gpu.text is not None:
            window_regex = re.compile(f"geometry=([0-9]+x[0-9]+)\+([0-9]+)\+([0-9]+)")
            m = window_regex.match(gpu.text)
            if m is not None:
                self.gpu.append(("Window", m.group(1), m.group(2), m.group(3)))
            else:
                self.gpu.append(("Full screen", gpu.text.split(':')[1]))
        else:
            display_type = gpu.xpath("./display_type")[0].text
            for display in gpu.xpath("./displays/display"):
                if display_type == "Window":
                    window_resolutions = display.xpath("./window_resolutions")[0].text if display.xpath("./window_resolutions") else None
                    horizontal_offset = display.xpath("./horizontal_offset")[0].text if display.xpath("./horizontal_offset") else None
                    vertical_offset = display.xpath("./vertical_offset")[0].text if display.xpath("./vertical_offset") else None
                    self.gpu.append((display_type, window_resolutions, horizontal_offset, vertical_offset))
                elif display_type == "Full screen":
                    monitor_id = display.xpath("./monitor_id")[0].text if display.xpath("./monitor_id") else None
                    self.gpu.append((display_type, monitor_id))

    def format_console_element(self, console):
        node = etree.Element("console")
        if console[0] is not None:
            etree.SubElement(node, "use_type").text = console[0]
        if console[1] is not None:
            etree.SubElement(node, "backend_type").text = console[1]
        if console[1] == "socket":
            etree.SubElement(node, "sock_file_path").text = console[2]
        if console[1] == "tty":
            etree.SubElement(node, "tty_device_path").text = console[2]
        if console[1] == "file":
            etree.SubElement(node, "output_file_path").text = console[2]
        return node

    def format_network_element(self, network):
        node = etree.Element("network")
        if network[0] is not None:
            etree.SubElement(node, "virtio_framework").text = network[0]
        if network[1] is not None:
            etree.SubElement(node, "interface_name").text = network[1]
        return node

    def format_input_element(self, input):
        node = etree.Element("input")
        if input[0] is not None:
            etree.SubElement(node, "backend_device_file").text = input[0]
        if input[1] is not None:
            etree.SubElement(node, "id").text = input[1]
        return node

    def format_block_element(self, block):
        node = etree.Element("block")
        node.text = block
        return node

    def format_gpu_element(self, displays):
        node = etree.Element("gpu")
        if len(displays) > 0:
            etree.SubElement(node, "display_type").text = displays[0][0]
            displays_node = etree.SubElement(node, "displays")
            for display in displays:
                if display[0] == "Window":
                    display_node = etree.SubElement(displays_node, "display")
                    etree.SubElement(display_node, "window_resolutions").text = display[1]
                    etree.SubElement(display_node, "horizontal_offset").text = display[2]
                    etree.SubElement(display_node, "vertical_offset").text = display[3]
                elif display[0] == "Full screen":
                    display_node = etree.SubElement(displays_node, "display")
                    etree.SubElement(display_node, "monitor_id").text = display[1]
        return node

    def format_xml_element(self):
        node = etree.Element("virtio_devices")
        for console in self.consoles:
            node.append(self.format_console_element(console))
        for network in self.networks:
            node.append(self.format_network_element(network))
        for input in self.inputs:
            node.append(self.format_input_element(input))
        for block in self.blocks:
            node.append(self.format_block_element(block))

        node.append(self.format_gpu_element(self.gpu))
        return node

    def add_virtio_devices(self, virtio_device_node):
        if virtio_device_node.xpath("./network")[0].text is not None:
            for network in virtio_device_node.xpath("./network"):
                self.networks.append((None, network.text))
        else:
            for network in virtio_device_node.xpath("./network"):
                virtio_framework = network.xpath("./virtio_framework")[0].text if network.xpath("./virtio_framework") else None
                interface_name = network.xpath("./interface_name")[0].text if network.xpath("./interface_name") else None
                self.networks.append((virtio_framework, interface_name))

        if len(virtio_device_node.xpath("./input")) > 0:
            if virtio_device_node.xpath("./input")[0].text is not None:
                for input in virtio_device_node.xpath("./input"):
                    self.inputs.append((None, input.text))
            else:
                for input in virtio_device_node.xpath("./input"):
                    backend_device_file = input.xpath("./backend_device_file")[0].text if input.xpath("./backend_device_file") else None
                    id = input.xpath("./id")[0].text if input.xpath("./id") else None
                    self.inputs.append((backend_device_file, id))

        for console in virtio_device_node.xpath("./console"):
            self.console_encoding(console)
        for block in virtio_device_node.xpath("./block"):
            self.blocks.append(block.text)
        for gpu in virtio_device_node.xpath("./gpu"):
            self.gpu_encoding(gpu)

class ScenarioUpgrader(ScenarioTransformer):
    @classmethod
    def get_node(cls, element, xpath):
        return next(iter(element.xpath(xpath, namespaces=cls.xpath_ns)), None)

    def __init__(self, xsd_etree, old_xml_etree, old_launch_etree = None):
        super().__init__(xsd_etree, visit_optional_node=True)
        self.old_xml_etree = old_xml_etree
        self.old_launch_etree = old_launch_etree
        if old_launch_etree is not None:
            service_vm_id = old_xml_etree.xpath("//vm[.//load_order = 'SERVICE_VM' or .//vm_type = 'SERVICE_VM']/@id")
            if not service_vm_id:
                self.old_launch_etree = None
            else:
                self.service_vm_id = int(service_vm_id[0])

        # Collect all nodes in old_xml_etree which will be used to track data not moved
        self.old_data_nodes = set()
        for node in old_xml_etree.iter():
            if node.text:
                self.old_data_nodes.add(node)
        if self.old_launch_etree is not None:
            for node in self.old_launch_etree.iter():
                if node.text:
                    self.old_data_nodes.add(node)

        self.hv_vm_node_map = {}

    def get_from_old_data(self, new_parent_node, xpath):
        hv_vm_node = new_parent_node
        if hv_vm_node.tag not in ["vm", "hv"]:
            hv_vm_node = next(new_parent_node.iterancestors(["vm", "hv"]), None)
        old_hv_vm_node = self.hv_vm_node_map[hv_vm_node]
        old_data_node = old_hv_vm_node.xpath(xpath)
        return old_data_node

    def get_from_old_launch_data(self, new_parent_node, xpath):
        if self.old_launch_etree is None:
            return []

        vm_node = new_parent_node
        if vm_node.tag != "vm":
            vm_node = next(new_parent_node.iterancestors("vm"), None)
        if vm_node is None:
            return []

        old_vm_node = self.hv_vm_node_map[vm_node]
        user_vm_id = int(old_vm_node.get("id")) - self.service_vm_id
        user_vm_node = self.old_launch_etree.xpath(f"//user_vm[@id = '{user_vm_id}']")
        old_data_node = user_vm_node[0].xpath(xpath) if user_vm_node else []
        return old_data_node

    def move_build_type(self, xsd_element_node, xml_parent_node, new_nodes):
        old_data_node = self.get_node(self.old_xml_etree, f"//hv//RELEASE")
        if old_data_node is not None:
            new_node = etree.Element(xsd_element_node.get("name"))
            new_node.text = "release" if old_data_node.text == "y" else "debug"
            new_nodes.append(new_node)
            self.old_data_nodes.discard(old_data_node)
        else:
            self.move_data_by_xpath(".//BUILD_TYPE", xsd_element_node, xml_parent_node, new_nodes)
        return False

    def move_virtio_devices(self, xsd_element_node, xml_parent_node, new_nodes):
        virtio = VirtioDevices(self.old_xml_etree)
        try:
            old_data_virtio = self.get_from_old_data(xml_parent_node, ".//virtio_devices").pop()
        except IndexError as e:
            logging.debug(e)
            return

        virtio.add_virtio_devices(old_data_virtio)
        for child in old_data_virtio.iter():
            self.old_data_nodes.discard(child)
        new_nodes.append(virtio.format_xml_element())
        return False

    def move_memory(self, xsd_element_node, xml_parent_node, new_nodes):
        new_node = etree.Element(xsd_element_node.get("name"))
        memory_node = self.hv_vm_node_map[xml_parent_node].xpath("./memory")
        old_data_start_hpa = []
        old_data_size_hpa = []
        old_data_whole = []
        if len(memory_node) != 0:
            for element in memory_node[0]:
                if "start_hpa" in element.tag:
                    old_data_start_hpa.append(element)
                elif "size" in element.tag:
                    if "0x" in element.text:
                        element.text = str(int(element.text, 16) // 1024 // 1024)
                    old_data_size_hpa.append(element)
                elif "whole" in element.tag:
                    old_data_whole.append(element)
                elif "hpa_region" in element.tag:
                    for subelement in element:
                        if "start_hpa" in subelement.tag:
                            old_data_start_hpa.append(subelement)
                        elif "size" in subelement.tag:
                            old_data_size_hpa.append(subelement)
                        elif "whole" in subelement.tag:
                            old_data_whole.append(subelement)

        if len(old_data_start_hpa) != 0 and len(old_data_size_hpa) != 0:
            for i in range(len(old_data_start_hpa)):
                if int(old_data_start_hpa[i].text, 16) != 0 and int(old_data_size_hpa[i].text, 16) != 0:
                    hpa_region_node = etree.SubElement(new_node, 'hpa_region')
                    old_data_size_hpa[i].tag = "size_hpa"
                    hpa_region_node.append(old_data_start_hpa[i])
                    hpa_region_node.append(old_data_size_hpa[i])
        elif len(old_data_whole) != 0 or (len(old_data_start_hpa) == 0 and len(old_data_size_hpa) != 0):
            if len(old_data_whole) != 0:
                for i in range(len(old_data_whole)):
                    old_data_whole[i].tag = "size"
                    new_node.append(old_data_whole[i])
            else:
                for i in range(len(old_data_size_hpa)):
                    old_data_size_hpa[i].tag = "size"
                    new_node.append(old_data_size_hpa[i])

        new_nodes.append(new_node)

        for n in old_data_start_hpa:
            self.old_data_nodes.discard(n)
        for n in old_data_size_hpa:
            self.old_data_nodes.discard(n)
        for n in old_data_whole:
            self.old_data_nodes.discard(n)

        return False

    def move_console_vuart(self, xsd_element_node, xml_parent_node, new_nodes):
        new_node = etree.Element(xsd_element_node.get("name"))
        new_node.text = "None"
        new_nodes.append(new_node)

        vm_load_order = next(iter(self.get_from_old_data(xml_parent_node, ".//load_order/text()")), None)
        legacy_vuart = self.get_from_old_data(xml_parent_node, ".//legacy_vuart[@id = '0']")
        legacy_vuart = legacy_vuart[0] if legacy_vuart else None
        console_vuart = self.get_from_old_data(xml_parent_node, ".//console_vuart")
        console_vuart = console_vuart[0] if console_vuart else None
        launch_console_vuart = self.get_from_old_launch_data(xml_parent_node, ".//console_vuart")
        launch_console_vuart = launch_console_vuart[0] if launch_console_vuart else None

        if legacy_vuart is None and console_vuart is None and launch_console_vuart is None:
            return False

        if console_vuart is not None and console_vuart.text:
            new_node.text = console_vuart.text
        elif legacy_vuart is not None and legacy_vuart.find("type").text == "VUART_LEGACY_PIO":
            vuart_base = legacy_vuart.find("base").text
            if vuart_base == "CONFIG_COM_BASE":
                # The new schema does not support arbitrary configuration of console vUART bases. Report the data as lost.
                return False
            elif vuart_base.endswith("COM1_BASE"):
                new_node.text = "COM Port 1"
            elif vuart_base.endswith("COM2_BASE"):
                new_node.text = "COM Port 2"
            elif vuart_base.endswith("COM3_BASE"):
                new_node.text = "COM Port 3"
            elif vuart_base.endswith("COM4_BASE"):
                new_node.text = "COM Port 4"

            if vm_load_order == "SERVICE_VM":
                logging.info(f"The console virtual UART of the service VM is moved to {new_node.text}. Please double check the console= command line option in the OS bootargs of the service VM.")
        elif console_vuart is not None:
            if console_vuart.find("base") == "PCI_VUART":
                new_node.text = "PCI"
            else:
                new_node.text = console_vuart.text
        elif launch_console_vuart and launch_console_vuart.text != "Disable":
            new_node.text = "PCI"

        if legacy_vuart is not None:
            for n in legacy_vuart.iter():
                self.old_data_nodes.discard(n)
        if console_vuart is not None:
            for n in console_vuart.iter():
                self.old_data_nodes.discard(n)
        if launch_console_vuart is not None:
            for n in launch_console_vuart.iter():
                self.old_data_nodes.discard(n)

        return False

    def move_vuart_connections(self, xsd_element_node, xml_parent_node, new_nodes):
        conns = VirtualUartConnections()

        # Fetch vUART endpoints in the old data
        vuart_endpoints = self.old_xml_etree.xpath("//legacy_vuart[@id != '0' and base != 'INVALID_COM_BASE'] | //communication_vuart[base != 'INVALID_PCI_BASE']")
        vuart_connections = self.old_xml_etree.xpath("//vuart_connection")

        for endpoint in vuart_endpoints:
            if conns.add_endpoint(endpoint):
                for child in endpoint.iter():
                    self.old_data_nodes.discard(child)

        for connection in vuart_connections:
            if conns.add_connection(connection):
                for child in connection.iter():
                    self.old_data_nodes.discard(child)

        new_nodes.extend(conns.format_xml_elements(xsd_element_node))

        # Disconnected endpoints do not migrate, but remove such nodes from old_data_nodes to avoid raising
        # data-is-discarded warnings.
        for n in self.old_xml_etree.xpath("//legacy_vuart[@id != '0' and base = 'INVALID_COM_BASE'] | //communication_vuart[base = 'INVALID_PCI_BASE']"):
            for child in n.iter():
                self.old_data_nodes.discard(child)

        return False

    def move_ivshmem(self, xsd_element_node, xml_parent_node, new_nodes):
        regions = SharedMemoryRegions(self.old_xml_etree)
        for old_region in self.old_xml_etree.xpath("//IVSHMEM_REGION"):
            regions.add_ivshmem_region(old_region)
            for child in old_region.iter():
                self.old_data_nodes.discard(child)
        if self.old_launch_etree:
            for old_region in self.old_launch_etree.xpath("//shm_region"):
                regions.add_ivshmem_region(old_region)
                for child in old_region.iter():
                    self.old_data_nodes.discard(child)

        new_nodes.append(regions.format_xml_element())

        return False

    def move_vm_type(self, xsd_element_node, xml_parent_node, new_nodes):
        try:
            old_vm_type_node = self.get_from_old_data(xml_parent_node, ".//vm_type").pop()
        except IndexError as e:
            logging.debug(e)
            return
        old_guest_flag_nodes = self.get_from_old_data(xml_parent_node, ".//guest_flag[text() = 'GUEST_FLAG_RT']")
        old_rtos_type_nodes = self.get_from_old_launch_data(xml_parent_node, ".//rtos_type")

        new_node = etree.Element(xsd_element_node.get("name"))
        if old_vm_type_node.text in ["PRE_RT_VM", "POST_RT_VM"] or \
           old_guest_flag_nodes or \
           (old_rtos_type_nodes and old_rtos_type_nodes[0].text in ["Soft RT", "Hard RT"]):
            new_node.text = "RTVM"
        elif old_vm_type_node.text in ["SAFETY_VM", "PRE_STD_VM", "POST_STD_VM", "SERVICE_VM", "SOS_VM"]:
            new_node.text = "STANDARD_VM"
        else:
            new_node.text = old_vm_type_node.text
        new_nodes.append(new_node)
        self.old_data_nodes.discard(old_vm_type_node)
        for n in old_guest_flag_nodes:
            self.old_data_nodes.discard(n)
        for n in old_rtos_type_nodes:
            self.old_data_nodes.discard(n)

        return False

    def move_pcpu(self, xsd_element_node, xml_parent_node, new_nodes):
        vm_type = self.get_node(xml_parent_node, "parent::vm/vm_type/text()")

        pcpus = self.get_from_old_launch_data(xml_parent_node, "cpu_affinity/pcpu_id[text() != '']")
        if not pcpus:
            pcpus = self.get_from_old_data(xml_parent_node, "cpu_affinity/pcpu_id[text() != '']")

        if pcpus:
            for n in pcpus:
                new_node = etree.Element(xsd_element_node.get("name"))
                etree.SubElement(new_node, "pcpu_id").text = n.text
                if vm_type == "RTVM":
                    etree.SubElement(new_node, "real_time_vcpu").text = "y"
                new_nodes.append(new_node)
                self.old_data_nodes.discard(n)
        else:
            for n in self.get_from_old_data(xml_parent_node, "cpu_affinity/pcpu"):
                new_nodes.append(n)
                for child in n.iter():
                    self.old_data_nodes.discard(child)

        return False

    def move_os_type(self, xsd_element_node, xml_parent_node, new_nodes):
        old_os_type_nodes = self.get_from_old_launch_data(xml_parent_node, ".//user_vm_type")

        if old_os_type_nodes:
            new_node = etree.Element(xsd_element_node.get("name"))
            if old_os_type_nodes[0].text == "WINDOWS":
                new_node.text = "Windows OS"
            else:
                new_node.text = "Non-Windows OS"
            new_nodes.append(new_node)
            for n in old_os_type_nodes:
                self.old_data_nodes.discard(n)
        else:
            self.move_data_by_same_tag(xsd_element_node, xml_parent_node, new_nodes)

        return False

    def move_guest_flag(self, guest_flag, xsd_element_node, xml_parent_node, new_nodes):
        old_data_nodes = self.get_from_old_data(xml_parent_node, f".//guest_flag[text() = '{guest_flag}']")
        if old_data_nodes:
            new_node = etree.Element(xsd_element_node.get("name"))
            new_node.text = "y"
            new_nodes.append(new_node)
            for n in old_data_nodes:
                self.old_data_nodes.discard(n)
        else:
            self.move_data_by_same_tag(xsd_element_node, xml_parent_node, new_nodes)

        return False

    def move_lapic_passthrough(self, xsd_element_node, xml_parent_node, new_nodes):
        old_rtos_type_nodes = self.get_from_old_launch_data(xml_parent_node, ".//rtos_type")
        if old_rtos_type_nodes and old_rtos_type_nodes[0].text == "Hard RT":
            new_node = etree.Element(xsd_element_node.get("name"))
            new_node.text = "y"
            new_nodes.append(new_node)
            # The rtos_type node will be consumed by the vm_type mover
        else:
            self.move_guest_flag("GUEST_FLAG_LAPIC_PASSTHROUGH", xsd_element_node, xml_parent_node, new_nodes)

        return False

    def move_enablement(self, xpath, xsd_element_node, xml_parent_node, new_nodes, values_as_enabled = ["Enable"], values_as_disabled = ["Disable"]):
        ret = self.move_data_by_xpath(xpath, xsd_element_node, xml_parent_node, new_nodes)
        for n in new_nodes:
            if n.text in values_as_enabled:
                n.text = "y"
            elif n.text in values_as_disabled:
                n.text = "n"
        return ret

    def move_hierarchy(self, xsd_element_node, xml_parent_node, new_nodes):
        element_tag = xsd_element_node.get("name")
        for n in self.get_from_old_data(xml_parent_node, f"//{element_tag}"):
            new_nodes.append(n)
            for child in n.iter():
                self.old_data_nodes.discard(child)

    def move_data_by_xpath(self, xpath, xsd_element_node, xml_parent_node, new_nodes, scenario_xml_only = False, launch_xml_only = False):
        element_tag = xsd_element_node.get("name")

        old_data_nodes = []
        if not launch_xml_only:
            old_data_nodes = self.get_from_old_data(xml_parent_node, xpath)
        if not scenario_xml_only and not old_data_nodes and self.old_launch_etree is not None:
            old_data_nodes = self.get_from_old_launch_data(xml_parent_node, xpath)

        if self.complex_type_of_element(xsd_element_node) is None:
            max_occurs_raw = xsd_element_node.get("maxOccurs")

            # Use `len(old_data_nodes)` to ensure that all old data nodes are moved if an unbound number of
            # occurrences is allowed.
            max_occurs = \
                len(old_data_nodes)  if max_occurs_raw == "unbounded" else \
                1                    if max_occurs_raw is None        else \
                int(max_occurs_raw)

            if len(old_data_nodes) <= max_occurs:
                for n in old_data_nodes:
                    new_node = etree.Element(element_tag)
                    new_node.text = n.text
                    for k, v in n.items():
                        if k in ["id", "name"]:
                            new_node.set(k, v)
                    new_nodes.append(new_node)
                    self.old_data_nodes.discard(n)

            return False
        else:
            # For each complex type containing multiple configuration items, this method can only create at most one
            # single node, as there is no way for the default data movers to migrate multiple pieces of data of the same
            # type to the new XML.
            if old_data_nodes:
                if old_data_nodes[0].tag == "usb_xhci":
                    old_data_nodes[0].attrib.clear()
                new_node = etree.Element(element_tag)
                for k, v in old_data_nodes[0].items():
                    new_node.set(k, v)
                new_nodes.append(new_node)
            return True

    def move_data_by_same_tag(self, xsd_element_node, xml_parent_node, new_nodes):
        element_tag = xsd_element_node.get("name")
        return self.move_data_by_xpath(f".//{element_tag}", xsd_element_node, xml_parent_node, new_nodes)

    def rename_data(self, old_xpath, new_xpath, xsd_element_node, xml_parent_node, new_nodes):
        ret = self.move_data_by_xpath(old_xpath, xsd_element_node, xml_parent_node, new_nodes)
        if not new_nodes:
            ret = self.move_data_by_xpath(new_xpath, xsd_element_node, xml_parent_node, new_nodes)
        return ret

    def move_data_from_either_xml(self, scenario_xpath, launch_xpath, xsd_element_node, xml_parent_node, new_nodes):
        # When moving data from either XML files, data in the launch XML take precedence.
        ret = self.move_data_by_xpath(launch_xpath, xsd_element_node, xml_parent_node, new_nodes, launch_xml_only = True)
        if not new_nodes:
            ret = self.move_data_by_xpath(scenario_xpath, xsd_element_node, xml_parent_node, new_nodes, scenario_xml_only = True)
        else:
            self.move_data_by_xpath(scenario_xpath, xsd_element_node, xml_parent_node, list(), scenario_xml_only = True)
        return ret

    def move_data_from_both_xmls(self, scenario_xpath, launch_xpath, xsd_element_node, xml_parent_node, new_nodes):
        ret_scenario = self.move_data_by_xpath(scenario_xpath, xsd_element_node, xml_parent_node, new_nodes, scenario_xml_only = True)
        ret_launch = self.move_data_by_xpath(launch_xpath, xsd_element_node, xml_parent_node, new_nodes, launch_xml_only = True)
        return ret_scenario or ret_launch

    def create_node_if(self, scenario_xpath, launch_xpath, xsd_element_node, xml_parent_node, new_nodes):
        if self.get_from_old_data(xml_parent_node, scenario_xpath) or \
           self.get_from_old_launch_data(xml_parent_node, launch_xpath):
            new_node = etree.Element(xsd_element_node.get("name"))
            new_nodes.append(new_node)
            return True
        return False

    def move_data_null(self, xsd_element_node, xml_parent_node, new_nodes):
        return False

    data_movers = {
        "vm/name": partialmethod(move_data_from_either_xml, "name", "vm_name"),
        "pcpu": move_pcpu,
        "pcpu_id": partialmethod(move_data_from_either_xml, "cpu_affinity/pcpu_id[text() != '']", "cpu_affinity/pcpu_id[text() != '']"),
        "pci_dev": partialmethod(move_data_from_both_xmls, ".//pci_devs/pci_dev[text()]", "passthrough_devices/*[text()] | sriov/*[text()]"),
        "PTM": partialmethod(move_data_from_either_xml, ".//PTM", "enable_ptm"),

        # Configuration items with the same name but under different parents
        "os_config/name": partialmethod(move_data_by_xpath, ".//os_config/name"),
        "epc_section/base": partialmethod(move_data_by_xpath, ".//epc_section/base"),
        "console_vuart/base": partialmethod(move_data_by_xpath, ".//console_vuart/base"),
        "epc_section/size": partialmethod(move_data_by_xpath, ".//epc_section/size"),
        "memory/size": partialmethod(move_data_by_xpath, ".//memory/size"),

        # Guest flags
        "lapic_passthrough": move_lapic_passthrough,
        "io_completion_polling": partialmethod(move_guest_flag, "GUEST_FLAG_IO_COMPLETION_POLLING"),
        "nested_virtualization_support": partialmethod(move_guest_flag, "GUEST_FLAG_NVMX_ENABLED"),
        "virtual_cat_support": partialmethod(move_guest_flag, "GUEST_FLAG_VCAT_ENABLED"),
        "secure_world_support": partialmethod(move_guest_flag, "GUEST_FLAG_SECURITY_VM"),
        "hide_mtrr_support": partialmethod(move_guest_flag, "GUEST_FLAG_HIDE_MTRR"),
        "security_vm": partialmethod(move_guest_flag, "GUEST_FLAG_SECURITY_VM"),

        # Feature enabling or disabling
        "vuart0": partialmethod(move_enablement, ".//vuart0"),
        "vbootloader": partialmethod(move_enablement, ".//vbootloader", values_as_enabled = ["ovmf", "Enable"], values_as_disabled = ["no", "Disable"]),
        "MCE_ON_PSC_ENABLED": partialmethod(move_enablement, ".//MCE_ON_PSC_DISABLED", values_as_enabled = ["n"], values_as_disabled = ["y"]),
        "SPLIT_LOCK_DETECTION_ENABLED": partialmethod(move_enablement, ".//ENFORCE_TURNOFF_AC", values_as_enabled = ["n"], values_as_disabled = ["y"]),
        "UC_LOCK_DETECTION_ENABLED": partialmethod(move_enablement, ".//ENFORCE_TURNOFF_GP", values_as_enabled = ["n"], values_as_disabled = ["y"]),

        # Intermediate nodes
        "pci_devs": partialmethod(create_node_if, ".//pci_devs", ".//passthrough_devices/*[text() != ''] | .//sriov/*[text() != '']"),

        "BUILD_TYPE": move_build_type,
        "RELOC_ENABLED": partialmethod(rename_data, "FEATURES/RELOC", "FEATURES/RELOC_ENABLED"),
        "MULTIBOOT2_ENABLED": partialmethod(rename_data, "FEATURES/MULTIBOOT2", "FEATURES/MULTIBOOT2_ENABLED"),
        "console_vuart": move_console_vuart,
        "vuart_connections": move_vuart_connections,
        "IVSHMEM": move_ivshmem,
        "vm_type": move_vm_type,
        "os_type": move_os_type,
        "virtio_devices": move_virtio_devices,
        "memory": move_memory,

        "CACHE_REGION": move_hierarchy,

        "default": move_data_by_same_tag,
    }

    def add_missing_nodes(self, xsd_element_node, xml_parent_node, xml_anchor_node):
        new_nodes = []
        def call_mover(mover):
            if isinstance(mover, list):
                ret = False
                for fn in mover:
                    ret = call_mover(fn)
                return ret
            elif isinstance(mover, partialmethod):
                return mover.__get__(self, type(self))(xsd_element_node, xml_parent_node, new_nodes)
            else:
                return mover(self, xsd_element_node, xml_parent_node, new_nodes)

        # Common names (such as 'name' or 'base') may be used as tags in multiple places each of which has different
        # meanings. In such cases it is ambiguious to query old data by that common tag alone.
        element_tag = xsd_element_node.get("name")
        element_tag_with_parent = f"{xml_parent_node.tag}/{element_tag}"

        mover_key = \
            element_tag_with_parent if element_tag_with_parent in self.data_movers.keys() else \
            element_tag if element_tag in self.data_movers.keys() else \
            "default"
        visit_children = call_mover(self.data_movers[mover_key])

        if xml_anchor_node is not None:
            for n in new_nodes:
                xml_anchor_node.addprevious(n)
        else:
            xml_parent_node.extend(new_nodes)

        if visit_children:
            return new_nodes
        else:
            return []

    @property
    @lru_cache
    def upgraded_etree(self):
        new_xml_etree = etree.ElementTree(etree.Element(self.old_xml_etree.getroot().tag))
        root_node = new_xml_etree.getroot()

        # Migrate the HV and VM nodes, which are needed to kick off a thorough traversal of the existing scenario.
        for old_node in self.old_xml_etree.getroot():
            new_node = etree.Element(old_node.tag)

            if old_node.tag == "vm":
                # FIXME: Here we still hard code how the load order of a VM is specified in different versions of
                # schemas. While it is not subject to frequent changes, it would be better if we use a more generic
                # approach instead.
                load_order_node = etree.SubElement(new_node, "load_order")

                # In the history we have two ways of specifying the load order of a VM: either by vm_type or by
                # loader_order.
                vm_type = old_node.xpath(".//vm_type/text()")
                old_load_order_node = old_node.xpath(".//load_order")
                if old_load_order_node:
                    load_order_node.text = old_load_order_node[0].text
                    self.old_data_nodes.discard(old_load_order_node[0])
                elif vm_type:
                    if vm_type[0].startswith("PRE_") or vm_type[0] in ["SAFETY_VM"]:
                        load_order_node.text = "PRE_LAUNCHED_VM"
                    elif vm_type[0].startswith("POST_"):
                        load_order_node.text = "POST_LAUNCHED_VM"
                    else:
                        load_order_node.text = "SERVICE_VM"
                else:
                    logging.error(f"Cannot infer the loader order of VM {self.old_xml_etree.getelementpath(old_node)}")
                    continue

            root_node.append(new_node)
            for k, v in old_node.items():
                new_node.set(k, v)
            self.hv_vm_node_map[new_node] = old_node

        # Now fill the rest of configuration items using the old data
        self.transform(new_xml_etree)

        return new_xml_etree

class UpgradingScenarioStage(PipelineStage):
    uses = {"schema_etree", "scenario_etree"}
    provides = {"scenario_etree"}

    def __init__(self, has_launch_xml = False):
        self.has_launch_xml = has_launch_xml
        if has_launch_xml:
            self.uses.add("launch_etree")

    class DiscardedDataFilter(namedtuple("DiscardedDataFilter", ["path", "data", "info"])):
        def filter(self, path, data):
            simp_path = re.sub(r"\[[^\]]*\]", "", path)
            if not simp_path.endswith(self.path):
                return False
            if self.data and data != self.data:
                return False

            if self.info:
                logging.info(f"{path} = '{data}': {self.info}")
            return True

    filters = [
        DiscardedDataFilter("hv/FEATURES/IVSHMEM", None, "IVSHMEM is now automatically enabled if any IVSHMEM region is specified."),
        DiscardedDataFilter("hv/FEATURES/NVMX_ENABLED", None, "Nest virtualization support is now automatically included if enabled for any VM."),
        DiscardedDataFilter("hv/CAPACITIES/IOMMU_BUS_NUM", None, "The maximum bus number to be supported by ACRN IOMMU configuration is now inferred from board data."),
        DiscardedDataFilter("hv/MISC_CFG/UEFI_OS_LOADER_NAME", None, None),
        DiscardedDataFilter("vm/guest_flags/guest_flag", "0", None),
        DiscardedDataFilter("vm/clos/vcpu_clos", None, "clos nodes are no longer needed in scenario definitions."),
        DiscardedDataFilter("vm/epc_section/base", "0", "Post-launched VMs cannot have EPC sections."),
        DiscardedDataFilter("vm/epc_section/size", "0", "Post-launched VMs cannot have EPC sections."),
        DiscardedDataFilter("vm/os_config/name", None, "Guest OS names are no longer needed in scenario definitions."),
    ]

    def run(self, obj):
        if self.has_launch_xml:
            upgrader = ScenarioUpgrader(obj.get("schema_etree"), obj.get("scenario_etree"), obj.get("launch_etree"))
        else:
            upgrader = ScenarioUpgrader(obj.get("schema_etree"), obj.get("scenario_etree"))
        new_scenario_etree = upgrader.upgraded_etree

        discarded_data = [(n.getroottree().getelementpath(n), n.text) for n in upgrader.old_data_nodes]
        for path, data in sorted(discarded_data):
            if not any(map(lambda x: x.filter(path, data), self.filters)):
                escaped_data = data.replace("\n", "\\n")
                logging.warning(f"{path} = '{escaped_data}' is discarded")

        obj.set("scenario_etree", new_scenario_etree)

def main(args):
    if args.launch:
        pipeline = PipelineEngine(["schema_path", "scenario_path", "launch_path"])
        pipeline.add_stages([
            LXMLLoadStage("schema"),
            LXMLLoadStage("scenario"),
            LXMLLoadStage("launch"),
            SlicingSchemaByVMTypeStage(),
            UpgradingScenarioStage(has_launch_xml=True),
        ])
    else:
        pipeline = PipelineEngine(["schema_path", "scenario_path"])
        pipeline.add_stages([
            LXMLLoadStage("schema"),
            LXMLLoadStage("scenario"),
            SlicingSchemaByVMTypeStage(),
            UpgradingScenarioStage(),
        ])

    obj = PipelineObject(schema_path = args.schema, scenario_path = args.scenario, launch_path=args.launch)
    pipeline.run(obj)
    # We know we are using lxml to parse the scenario XML, so it is ok to use lxml specific write options here.
    obj.get("scenario_etree").write(args.out, pretty_print=True)

if __name__ == "__main__":
    config_tools_dir = os.path.join(os.path.dirname(__file__), "..")
    schema_dir = os.path.join(config_tools_dir, "schema")

    parser = argparse.ArgumentParser(description="Try adapting data in a scenario XML to the latest schema.")
    parser.add_argument("scenario", help="Path to the scenario XML file from users")
    parser.add_argument("out", nargs="?", default="out.xml", help="Path where the output is placed")
    parser.add_argument("--schema", default=os.path.join(schema_dir, "config.xsd"), help="the XML schema that defines the syntax of scenario XMLs")
    parser.add_argument("--launch", default=None, help="Path to the launch XML file")
    args = parser.parse_args()

    logging.basicConfig(level="INFO")
    main(args)
