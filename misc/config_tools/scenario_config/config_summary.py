#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#
import sys
import argparse
import logging
import typing
import functools
import textwrap
from lxml import etree

t_content = typing.Union[str, typing.List[str]]


class Doc:
    def __init__(self, stream: typing.TextIO = sys.stdout, line_width: int = 72) -> None:
        self._stream = stream
        self._line_width = line_width

    def fill(self, text: str, initial_indent: int = 0, subsequent_indent: int = 0) -> str:
        return textwrap.fill(
            text=text,
            width=self._line_width,
            initial_indent=" " * initial_indent,
            subsequent_indent=" " * subsequent_indent,
            expand_tabs=False,
            break_long_words=False,
            break_on_hyphens=False,
        )

    def _add(self, content: t_content) -> None:
        if isinstance(content, list):
            self._stream.write("\n".join(content) + "\n")
        else:
            self._stream.write(content + "\n")

    def content(self, content: t_content, indent: int = 0) -> None:
        if isinstance(content, list):
            content = " ".join(content)
        self._add(self.fill(content, indent, indent))

    def note(self, content: t_content, indent: int = 0):
        marker = ".. {type}::".format(type='note')
        self._add(marker)
        self.content(content, indent=indent + 3)

    def newline(self, count: int = 1) -> None:
        if count == 1:
            self._add("")
        else:
            self._add("\n" * (count - 1))

    def table(self, header: typing.List, data) -> None:
        column_widths = list()
        content = list()
        data = [header] + data

        for i in range(len(header)):
            column_widths.append(max(list(map(lambda x: len(str(x[i])), data))))

        for j in range(len(data)):
            overline = "+" + "+".join(["-" * column_widths[i] for i in range(len(header))]) + "+"
            underline = "+" + "+".join(["=" * column_widths[i] for i in range(len(header))]) + "+"
            format_raw = "|" + "|".join([str(data[j][i]).ljust(column_widths[i]) for i in range(len(header))]) + "|"
            if j == 0:
                content.extend([overline, format_raw, underline])
            else:
                content.extend([format_raw, overline])

        if len(data) == 1:
            content.append(overline)

        self.newline()
        self._add(content)
        self.newline()

    def heading(self, text: str, char: str, overline: bool = False) -> None:
        underline = char * len(text)
        content = [text, underline]
        if overline:
            content.insert(0, underline)
        self._add(content)

    h1 = functools.partialmethod(heading, char="#")
    h2 = functools.partialmethod(heading, char="*")
    h3 = functools.partialmethod(heading, char="=")
    title = functools.partialmethod(heading, char="=", overline=True)


class GenerateRst:
    io_port = {}
    io_description = []
    pci_vuart = {}
    pci_ivshmem = {}
    amount_l3_cache = {}
    service_vm_used_pcpu_list = []

    # Class initialization
    def __init__(self, board_file_name, scenario_file_name, rst_file_name) -> None:
        self.board_etree = etree.parse(board_file_name)
        self.scenario_etree = etree.parse(scenario_file_name)
        self.file = open(rst_file_name, 'w')
        self.doc = Doc(self.file)

    # The rst content is written in three parts according to the first level title
    # 1. Hardware Resource Allocation 2. Inter-VM Connections 3. VM info
    def write_configuration_rst(self):
        self.doc.title(f"ACRN Scenario Datasheet")
        self.doc.newline()
        self.write_hardware_resource_allocation()
        self.write_inter_vm_connections()
        self.write_vms()
        self.close_file()

    # Write all info under the first level heading "Hardware Resource Allocation"
    def write_hardware_resource_allocation(self):
        self.doc.h1("Hardware Resource Allocation")
        self.doc.newline()
        self.write_pcpu()
        self.write_shared_cache()

    # Write physical CPUs info table
    def write_pcpu(self):
        self.doc.h2("Physical CPUs")
        column_title, data_table = self.get_pcpu_table()
        self.doc.table(column_title, data_table)

    # Get VM node from scenario xml
    def get_vm_node_list(self):
        return self.scenario_etree.xpath("/acrn-config/vm")

    # Get a specified VM name from an etree_node
    @classmethod
    def get_vm_name(cls, vm_node):
        return f"<VM {vm_node.attrib['id']} {vm_node.find('name').text}>"

    # Get all VM names info from the scenario.xml
    def get_all_vm_name(self):
        return list(map(lambda x: GenerateRst.get_vm_name(x), self.scenario_etree.xpath("/acrn-config/vm")))

    # get the column_title and datatable required by the physical CPUs table
    def get_pcpu_table(self):
        data_table = []
        column_title = [" "]
        total_pcpu_list = self.get_pcpu()
        vm_node_list = self.get_vm_node_list()
        pre_launch_vm_node_list = self.scenario_etree.xpath("vm[load_order = 'PRE_LAUNCHED_VM']")
        column_title.extend(map(str, total_pcpu_list))

        if pre_launch_vm_node_list is not None and len(pre_launch_vm_node_list) != 0:
            for pre_launch_vm_node in pre_launch_vm_node_list:
                pre_launch_vm_pcpu_list = list(map(int, (pre_launch_vm_node.xpath("cpu_affinity/pcpu/pcpu_id/text()"))))
                self.service_vm_used_pcpu_list.extend(set(total_pcpu_list).difference(set(pre_launch_vm_pcpu_list)))
        else:
            self.service_vm_used_pcpu_list.extend(total_pcpu_list)

        for vm_node in vm_node_list:
            vm_pcpu_id_list = []
            data_row = [" "] * (len(total_pcpu_list) + 1)
            data_row[0] = GenerateRst.get_vm_name(vm_node)
            vm_load_order = vm_node.find("load_order").text
            vm_pcpu_id_node = vm_node.xpath("cpu_affinity/pcpu/pcpu_id")
            vm_pcpu_id_list.extend(map(lambda x: int(x.text), vm_pcpu_id_node))

            if len(vm_pcpu_id_node) == 0 and vm_load_order == "SERVICE_VM":
                for pcpu in self.service_vm_used_pcpu_list:
                    data_row[pcpu + 1] = "*"
            else:
                for pcpu in vm_pcpu_id_list:
                    data_row[pcpu + 1] = "*"
            data_table.append(data_row)

        return column_title, data_table

    # Get all physical CPU information from board.xml
    def get_pcpu(self):
        pcpu_list = list(map(int, self.board_etree.xpath("processors//cpu_id/text()")))
        return pcpu_list

    def write_shared_cache(self):
        self.doc.h2("Shared Cache")
        self.doc.newline()
        self.write_each_shared_cache(cache_level='2')
        self.write_each_shared_cache(cache_level='3')

    # For L2 cache and L3 cache, fill in different cache info according to different situations
    def write_each_shared_cache(self, cache_level):
        vm_vcpu_info = self.get_vm_used_vcpu(cache_level)
        if not bool(vm_vcpu_info) and cache_level == '2':
            self.doc.h3(f"Level-{cache_level} Cache")
            self.doc.newline()
            self.doc.content("Level-2 caches are local on all physical CPUs.")
        elif not bool(vm_vcpu_info) and cache_level == '3':
            return
        else:
            self.doc.h3(f"Level-{cache_level} Cache")
            for cache_info, vm_info in vm_vcpu_info.items():
                each_cache_way_size = self.get_each_cache_way_info(cache_level, cache_info[1])[0]
                column_title, data_table = self.get_vcpu_table({cache_info: vm_info}, cache_level)
                self.doc.table(column_title, data_table)
                self.doc.note(content=f"Each cache chunk is {each_cache_way_size}KB.")
        self.doc.newline()

    # Get used vcpu table
    def get_vcpu_table(self, vm_vcpu_info, cache_level):
        data_table = []
        column_title = [" "]
        chunk_list = self.get_each_cache_way_info(cache_level=cache_level)[1]
        column_title.extend(chunk_list)
        for vm_name, vcpu_list in vm_vcpu_info.items():
            for vcpu_tuple in vcpu_list:
                data_row = [f"{vcpu_tuple[0]} vCPU{vcpu_tuple[1]}"] + ([" "] * len(chunk_list))
                clos_mask_list = list(vcpu_tuple[2])
                for index in range(len(clos_mask_list)):
                    if clos_mask_list[index] == '1':
                        data_row[index + 1] = "*"
                data_table.append(data_row)
        return column_title, sorted(data_table, key=lambda x: x[-1])

    # Get the vcpu info used by each VM from scenario.xml
    def get_vm_used_vcpu(self, cache_level):
        vm_used_vcpu_info = {}
        cache_allocation_list = self.scenario_etree.xpath("/acrn-config/hv/CACHE_REGION/CACHE_ALLOCATION")
        for cache_allocation_node in cache_allocation_list:
            vm_name_list = []
            vcpu_num_list = []
            clos_mask_list = []
            cache_id = cache_allocation_node.find("CACHE_ID").text
            if cache_allocation_node.find("CACHE_LEVEL").text == cache_level:
                policy_node_list = cache_allocation_node.xpath("POLICY")
                vm_name_list.extend(map(lambda x: x.find("VM").text, policy_node_list))
                vcpu_num_list.extend(map(lambda x: int(x.find("VCPU").text), policy_node_list))
                clos_mask_list.extend(map(lambda x: bin(int(x.find("CLOS_MASK").text, base=16))[2:], policy_node_list))
                for index in range(len(vm_name_list)):
                    if (cache_level, cache_id) not in vm_used_vcpu_info.keys():
                        vm_used_vcpu_info[(cache_level, cache_id)] = [(vm_name_list[index], vcpu_num_list[index], clos_mask_list[index])]
                    else:
                        vm_used_vcpu_info[(cache_level, cache_id)].append((vm_name_list[index], vcpu_num_list[index], clos_mask_list[index]))
        return vm_used_vcpu_info

    # Write all info under the first level heading "Inter-VM Connections"
    def write_inter_vm_connections(self):
        self.doc.h1("Inter-VM Connections")
        self.doc.newline()
        self.write_virtual_uarts()
        self.write_shared_memory()

    # Write the virtual uarts info according to the virtual uarts table
    def write_virtual_uarts(self):
        self.doc.h2("Virtual UARTs")
        self.doc.newline()
        self.doc.content("The table below summarizes the virtual UART connections in the system.")
        column_title, data_table = self.get_virtual_uarts_table()
        self.doc.table(column_title, data_table)

    # Get virtual uarts table
    def get_virtual_uarts_table(self):
        data_table = []
        vm_name_list = self.get_all_vm_name()
        column_title = ["vUART Connection"]
        column_title.extend(vm_name_list)
        vuart_connections_list = self.scenario_etree.xpath("/acrn-config/hv/vuart_connections/vuart_connection")
        for vuart_connection in vuart_connections_list:
            data_row = [""] * (len(vm_name_list) + 1)
            pci_row = [""] * 3
            vc_name = vuart_connection.find("name").text
            vc_type = vuart_connection.find("type").text
            vc_endpoint_list = vuart_connection.xpath("endpoint")
            data_row[0] = vc_name
            for vc_endpoint_node in vc_endpoint_list:
                vc_vm_name = vc_endpoint_node.find("vm_name").text
                vc_io_port = vc_endpoint_node.find("io_port").text
                if vc_endpoint_node.find("vbdf") is not None:
                    vc_vbdf = vc_endpoint_node.find("vbdf").text
                    pci_row[1] = vc_vbdf
                else:
                    vc_vbdf = ""
                if vc_vm_name not in self.io_port.keys():
                    self.io_port[vc_vm_name] = [vc_io_port]
                else:
                    self.io_port[vc_vm_name].append(vc_io_port)
                self.io_description.append(vc_name)
                for vm_name in vm_name_list:
                    if vm_name.find(vc_vm_name) != -1:
                        data_row[vm_name_list.index(vm_name) + 1] = f"{vc_type} {vc_io_port} {vc_vbdf}"
                        pci_row[0] = vc_vm_name
                        pci_row[2] = f"{vc_name} {vc_type} {vc_io_port}"
                if pci_row[0] not in self.pci_vuart:
                    self.pci_vuart[pci_row[0]] = [pci_row[-2:]]
                else:
                    self.pci_vuart[pci_row[0]].append(pci_row[-2:])
            data_table.append(data_row)
        return column_title, data_table

    # Write the shared memory information according to the shared memory table
    def write_shared_memory(self):
        self.doc.h2("Shared Memory")
        self.doc.newline()
        self.doc.content("The table below summarizes topology of shared memory in the system.")
        column_title, data_table = self.get_shared_memory_table()
        self.doc.table(column_title, data_table)

    # Get shared memory table
    def get_shared_memory_table(self):
        data_table = []
        vm_name_list = self.get_all_vm_name()
        column_title = ["Shared Memory Block"]
        column_title.extend(vm_name_list)
        ivshmem_region_list = self.scenario_etree.xpath("/acrn-config/hv/FEATURES/IVSHMEM/IVSHMEM_REGION")
        for ivshmem_region_node in ivshmem_region_list:
            data_row = [""] * (len(vm_name_list) + 1)
            ivshmem_row = [""] * 3
            ir_name = ivshmem_region_node.find("NAME").text
            powered_by = ivshmem_region_node.find("PROVIDED_BY").text
            data_row[0] = ir_name
            for ivshmem_vm in ivshmem_region_node.xpath("IVSHMEM_VMS/IVSHMEM_VM"):
                ivshmem_vm_name = ivshmem_vm.find("VM_NAME").text
                ivshmem_vm_vbdf = ivshmem_vm.find("VBDF").text
                for vm_name in vm_name_list:
                    if vm_name.find(ivshmem_vm_name) != -1:
                        ivshmem_row[0] = ivshmem_vm_name
                        ivshmem_row[1] = ivshmem_vm_vbdf
                        ivshmem_row[2] = f"pci, {ir_name}, powered by {powered_by}"
                        data_row[vm_name_list.index(vm_name) + 1] = f"pci {ivshmem_vm_vbdf}"

                if ivshmem_row[0] not in self.pci_ivshmem:
                    self.pci_ivshmem[ivshmem_row[0]] = [ivshmem_row[-2:]]
                else:
                    self.pci_ivshmem[ivshmem_row[0]].append(ivshmem_row[-2:])
            data_table.append(data_row)
        return column_title, data_table

    # Write all info under the first level heading "VM X - <VM Name>"
    def write_vms(self):
        vm_node_list = self.get_vm_node_list()
        for vm_node in vm_node_list:
            self.doc.h1(GenerateRst.get_vm_name(vm_node))
            self.doc.newline()
            self.write_each_vm(vm_node)

    # Write the information under all secondary headings for each VM in order
    def write_each_vm(self, vm_node):
        self.doc.h2("Basic Information")
        column_title1, data_table1 = self.get_basic_information_table(vm_node)
        self.doc.table(column_title1, data_table1)
        self.doc.h2("PCI Devices")
        column_title2, data_table2 = self.get_pci_devices_table(vm_node)
        self.doc.table(column_title2, data_table2)
        self.doc.h2("Fixed I/O Addresses")
        column_title3, data_table3 = self.get_fixed_io_address_table(vm_node)
        self.doc.table(column_title3, data_table3)

    # Get basic information table for VM info
    def get_basic_information_table(self, vm_node):
        parameter_dict = {}
        memory_size = 0
        data_table = []
        column_title = ["Parameter", "Configuration"]
        load_order = vm_node.find("load_order").text
        vm_name = vm_node.find("name").text
        if len(vm_node.xpath("memory/hpa_region")) == 0 and len(vm_node.xpath("memory/size")) == 0:
            memory_size = " "
        else:
            if len(vm_node.xpath("memory/hpa_region")) == 0:
                memory_size = int(vm_node.find("memory/size").text)
            else:
                hpa_region_list = vm_node.xpath("memory/hpa_region")
                for hpa_region in hpa_region_list:
                    memory_size = memory_size + int(hpa_region.find("size_hpa").text)
        vm_vcpu_info_l2 = self.get_vm_used_vcpu("2")
        vm_vcpu_info_l3 = self.get_vm_used_vcpu("3")
        l3_cache = self.board_etree.xpath(f"caches/cache [@level = '3']")
        if len(l3_cache) > 0:
            amount_vm_l3_cache = self.get_amount_l3_cache(vm_node)
            parameter_dict["Amount of L3 Cache"] = amount_vm_l3_cache
        parameter_dict["Load Order"] = load_order
        if load_order == "SERVICE_VM":
            parameter_dict["Number of vCPUs"] = len(self.service_vm_used_pcpu_list)
        else:
            parameter_dict["Number of vCPUs"] = len(vm_node.xpath(f"cpu_affinity/pcpu/pcpu_id"))
        parameter_dict["Ammount of RAM"] = str(memory_size) + "MB"
        data_table.extend(map(list, parameter_dict.items()))
        return column_title, data_table

    # Get cache way size for each shared cache in board.xml
    def get_each_cache_way_info(self, cache_level, cache_id=None):
        chunk_list = []
        total_cache_size = 0
        total_cache_ways = 0
        if cache_id is not None:
            cache_node_list = self.board_etree.xpath(f"caches/cache[@level = '{cache_level}' and @id = '{cache_id}']")
        else:
            cache_node_list = self.board_etree.xpath(f"caches/cache [@level = '{cache_level}']")
        for cache_node in cache_node_list:
            total_cache_size = max(int(cache_node.find("cache_size").text), total_cache_size)
            total_cache_ways = max(int(cache_node.find("ways").text), total_cache_ways)
        for i in range(total_cache_ways - 1, -1, -1):
            chunk_list.append(f"chunk{i}")

        return int(total_cache_size / 1024 / total_cache_ways), chunk_list

    # get Amount of L3 Cache info
    def get_amount_l3_cache(self, vm_node):
        vm_bit_map = 0
        vm_name = vm_node.find("name").text
        each_cache_way_size = self.get_each_cache_way_info(cache_level='3')[0]
        clos_mask_list = self.scenario_etree.xpath(f"/acrn-config/hv/CACHE_REGION/CACHE_ALLOCATION[CACHE_LEVEL='3']/POLICY[VM='{vm_name}']/CLOS_MASK/text()")
        for item in clos_mask_list:
            vm_bit_map |= int(item, base=16)

        vm_cache_way = bin(vm_bit_map).count('1')
        amount_vm_l3_cache = f"{vm_cache_way * each_cache_way_size} MB"
        return amount_vm_l3_cache

    # Get pci device table for VM info
    def get_pci_devices_table(self, vm_node):
        data_table = []
        vm_name = vm_node.find("name").text
        column_title = ["Parameter", "Configuration"]
        pci_devices_list = vm_node.xpath("pci_devs/pci_dev")
        if len(pci_devices_list) != 0:
            for pci_device in pci_devices_list:
                data_row = pci_device.text.split(" ", 1)
                data_table.append(data_row)
        if vm_name in self.pci_vuart.keys():
            for item in self.pci_vuart[vm_name]:
                data_row = [item[0], item[1]]
                data_table.append(data_row)
        if vm_name in self.pci_ivshmem.keys():
            for item in self.pci_ivshmem[vm_name]:
                data_row = [item[0], item[1]]
                data_table.append(data_row)
        return column_title, data_table

    # Get fixed io address table for VM info
    def get_fixed_io_address_table(self, vm_node):
        data_table = []
        column_title = ["I/O Address", "Function Description"]
        vm_name = vm_node.find("name").text
        for k, v in self.io_port.items():
            if k in vm_name:
                for item in v:
                    data_table.append([item, "Virtual UART"])
        return column_title, data_table

    # Close the Rst file after all information is written.
    def close_file(self):
        self.file.close()


def main(board_xml, scenario_xml, config_summary):
    GenerateRst(board_file_name=board_xml, scenario_file_name=scenario_xml,
                rst_file_name=config_summary).write_configuration_rst()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("board_file_name",
                        help="Specifies the board config XML path and name used to generate the rst file")
    parser.add_argument("scenario_file_name",
                        help="Specifies the scenario XML path and name used to generate the rst file")
    parser.add_argument("rst_file_name", default="config_summary.rst",
                        help="the path and name of the output rst file that "
                             "summaries the config from scenario.xml and board.xml")
    args = parser.parse_args()

    logging.basicConfig(level="INFO")

    sys.exit(main(args.board_file_name, args.scenario_file_name, args.rst_file_name))
