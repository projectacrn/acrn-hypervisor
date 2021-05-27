# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""the tool to generate ASL code of ACPI tables for Pre-launched VMs.

"""

import os, re, argparse, shutil
import xml.etree.ElementTree as ElementTree
from acpi_const import *
import board_cfg_lib
import collections
import lxml.etree

def calculate_checksum8():
    '''
    this function is implemented in iasl.
    :return:
    '''
    pass


def gen_rsdp(dest_vm_acpi_path):
    '''
    generate rsdp.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :return:
    '''
    rsdp_asl = 'rsdp.asl'
    p_xsdt_addr = r'XSDT Address : ([0-9a-fA-F]{16})'

    with open(os.path.join(dest_vm_acpi_path, rsdp_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, rsdp_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_xsdt_addr, line):
                    lines.append(re.sub(p_xsdt_addr, 'XSDT Address : {0:016X}'.format(ACPI_XSDT_ADDR), line))
                else:
                    lines.append(line)
        dest.writelines(lines)


def gen_xsdt(dest_vm_acpi_path, passthru_devices):
    '''
    generate xsdt.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :param passthru_devices: dict to store passthru device list
    :return:
    '''
    xsdt_asl = 'xsdt.asl'
    p_fadt_addr = r'ACPI Table Address   0 : ([0-9a-fA-F]{16})'
    p_mcfg_addr = r'ACPI Table Address   1 : ([0-9a-fA-F]{16})'
    p_madt_addr = r'ACPI Table Address   2 : ([0-9a-fA-F]{16})'
    p_tpm2_addr = r'ACPI Table Address   3 : ([0-9a-fA-F]{16})'
    p_rtct_addr = r'ACPI Table Address   4 : ([0-9a-fA-F]{16})'

    with open(os.path.join(dest_vm_acpi_path, xsdt_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, xsdt_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_fadt_addr, line):
                    lines.append(re.sub(p_fadt_addr, 'ACPI Table Address   0 : {0:016X}'.format(ACPI_FADT_ADDR), line))
                elif re.search(p_mcfg_addr, line):
                    lines.append(re.sub(p_mcfg_addr, 'ACPI Table Address   1 : {0:016X}'.format(ACPI_MCFG_ADDR), line))
                elif re.search(p_madt_addr, line):
                    lines.append(re.sub(p_madt_addr, 'ACPI Table Address   2 : {0:016X}'.format(ACPI_MADT_ADDR), line))
                elif re.search(p_tpm2_addr, line):
                    if 'TPM2' in passthru_devices:
                        lines.append(re.sub(p_tpm2_addr, 'ACPI Table Address   3 : {0:016X}'.format(ACPI_TPM2_ADDR), line))
                elif re.search(p_rtct_addr, line):
                    if 'PTCT' in passthru_devices or 'RTCT' in passthru_devices:
                        lines.append(re.sub(p_rtct_addr, 'ACPI Table Address   4 : {0:016X}'.format(ACPI_RTCT_ADDR), line))
                else:
                    lines.append(line)

        dest.writelines(lines)


def gen_fadt(dest_vm_acpi_path, board_root):
    '''
    generate facp.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :param board_root: the root element of board xml
    :return:
    '''
    fadt_asl = 'facp.asl'
    p_facs_addr = r'FACS Address : ([0-9a-fA-F]{8})'
    p_dsdt_addr = r'DSDT Address : ([0-9a-fA-F]{8})$'

    with open(os.path.join(dest_vm_acpi_path, fadt_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, fadt_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_facs_addr, line):
                    lines.append(re.sub(p_facs_addr, 'FACS Address : {0:08X}'.format(ACPI_FACS_ADDR), line))
                elif re.search(p_dsdt_addr, line):
                    lines.append(re.sub(p_dsdt_addr, 'DSDT Address : {0:08X}'.format(ACPI_DSDT_ADDR), line))
                else:
                    lines.append(line)
        dest.writelines(lines)


def gen_mcfg(dest_vm_acpi_path):
    '''
    generate mcfg.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :return:
    '''
    mcfg_asl = 'mcfg.asl'
    p_base_addr = r'Base Address : ([0-9a-fA-F]{16})'
    p_segment_group_num = r'Segment Group Number : (\d+)'
    p_start_bus_num = r'Start Bus Number : (\d+)'
    p_end_bus_num = r'End Bus Number : ([0-9a-fA-F]{2})'

    with open(os.path.join(dest_vm_acpi_path, mcfg_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, mcfg_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_base_addr, line):
                    lines.append(re.sub(p_base_addr, 'Base Address : {0:016X}'.format(VIRT_PCI_MMCFG_BASE), line))
                elif re.search(p_segment_group_num, line):
                    lines.append(re.sub(p_segment_group_num, 'Segment Group Number : {0:04X}'.format(0), line))
                elif re.search(p_start_bus_num, line):
                    lines.append(re.sub(p_start_bus_num, 'Start Bus Number : {0:02X}'.format(0), line))
                elif re.search(p_end_bus_num, line):
                    lines.append(re.sub(p_end_bus_num, 'End Bus Number : {0:02X}'.format(0xff), line))
                else:
                    lines.append(line)
        dest.writelines(lines)

def gen_madt(dest_vm_acpi_path, max_cpu_num, apic_ids):
    '''
    generate apic.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :return:
    '''
    madt_asl = 'apic.asl'

    lapic_index = 0
    p_lapic_addr = r'Local Apic Address : ([0-9a-fA-F]{8})'
    p_flags = r'\[0004\]        Flags (decoded below) : (\d{8})'  # dup flags
    flags_index = 0

    p_lapic_index = 0
    p_lapic_type = r'Subtable Type : (\d+) \[Processor Local APIC\]'
    p_lapic_len = r'\[0001\]                       Length : ([0-9a-fA-F]{2})'  # dup len
    p_lapic_len_index = 0
    p_lapic_flags_index = 0
    p_lapic_process_id = r'\[0001\]                 Processor ID : (\d+)'   # dup processor
    p_lapic_process_id_index = 0
    p_lapic_id = r'Local Apic ID : ([0-9a-fA-F]{2})'
    p_lapic_line_index = 0
    lapic_lines = []

    ioapic_index = 0
    p_ioapic_type = r'Subtable Type : (\d+) \[I/O APIC\]'
    p_ioapic_len_index = 0
    p_ioapic_id = r'I/O Apic ID : (\d+)'
    p_ioapic_addr = r'\[0004\]                      Address : ([0-9a-fA-F]{8})'

    lapic_nmi_index = 0
    p_lapic_nmi_type = r'Subtable Type : (\d+) \[Local APIC NMI\]'
    p_lapic_nmi_len_index = 0
    p_lapic_nmi_processor_id_index = 0
    p_lapic_nmi_flags = r'\[0002\]        Flags (decoded below) : ([0-9a-fA-F]{4})'
    p_lapic_nmi_flags_index = 0
    p_lapic_nmi_lint = r'Interrupt Input LINT : (\d+)'

    with open(os.path.join(dest_vm_acpi_path, madt_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, madt_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_lapic_addr, line):
                    lapic_index += 1
                    lines.append(re.sub(p_lapic_addr, 'Local Apic Address : {0:08X}'.format(0xFEE00000), line))
                elif re.search(p_flags, line):
                    if lapic_index == 1 and flags_index == 0:
                        lines.append(
                            re.sub(p_flags, '[0004]        Flags (decoded below) : {0:08X}'.format(0x1), line))
                        flags_index += 1
                    elif p_lapic_index == 1 and p_lapic_flags_index == 0:
                        lines.append(
                            re.sub(p_flags, '[0004]        Flags (decoded below) : {0:08X}'.format(0x1),
                                   line))
                        p_lapic_flags_index += 1
                    else:
                        lines.append(line)

                elif re.search(p_lapic_type, line):
                    p_lapic_index += 1
                    if lapic_index == 1:
                        lines.append(re.sub(p_lapic_type, 'Subtable Type : {0:02X} [Processor Local APIC]'.format(
                            ACPI_MADT_TYPE_LOCAL_APIC), line))
                    else:
                        lines.append(line)
                elif re.search(p_lapic_len, line):
                    if p_lapic_index == 1 and p_lapic_len_index == 0:
                        lines.append(
                            re.sub(p_lapic_len, '[0001]                       Length : {0:02X}'.format(0x8),
                                   line))
                        p_lapic_len_index += 1
                    elif ioapic_index == 1 and p_ioapic_len_index == 0:
                        lines.append(
                            re.sub(p_lapic_len, '[0001]                       Length : {0:02X}'.format(0x0C),
                                   line))
                        p_ioapic_len_index += 1
                    elif lapic_nmi_index == 1 and p_lapic_nmi_len_index == 0:
                        lines.append(
                            re.sub(p_lapic_len, '[0001]                       Length : {0:02X}'.format(0x06),
                                   line))
                        p_lapic_nmi_len_index += 1
                    else:
                        lines.append(line)
                elif re.search(p_lapic_process_id, line):
                    if p_lapic_index == 1 and p_lapic_process_id_index == 0:
                        lines.append(re.sub(p_lapic_process_id,
                                            '[0001]                 Processor ID : {0:02X}'.format(0x0),
                                            line))
                        p_lapic_process_id_index += 1
                    elif lapic_nmi_index == 1 and p_lapic_nmi_processor_id_index == 0:
                        lines.append(
                            re.sub(p_lapic_process_id,
                                   '[0001]                 Processor ID : {0:02X}'.format(0xFF),
                                   line))
                        p_lapic_nmi_processor_id_index += 1
                    else:
                        lines.append(line)
                elif re.search(p_lapic_id, line):
                    lines.append(re.sub(p_lapic_id, 'Local Apic ID : {0:02X}'.format(apic_ids[0]), line))

                elif re.search(p_ioapic_type, line):
                    ioapic_index += 1
                    lines.append(
                        re.sub(p_ioapic_type, 'Subtable Type : {0:02X} [I/O APIC]'.format(ACPI_MADT_TYPE_IOAPIC), line))
                elif re.search(p_ioapic_id, line):
                    lines.append(re.sub(p_ioapic_id, 'I/O Apic ID : {0:02X}'.format(0x01), line))
                elif re.search(p_ioapic_addr, line):
                    lines.append(re.sub(p_ioapic_addr,
                                        '[0004]                      Address : {0:02X}'.format(VIOAPIC_BASE),
                                        line))

                elif re.search(p_lapic_nmi_type, line):
                    lapic_nmi_index += 1
                    if lapic_nmi_index == 1:
                        lines.append(re.sub(p_lapic_nmi_type, 'Subtable Type : {0:02X} [Local APIC NMI]'.format(
                            ACPI_MADT_TYPE_LOCAL_APIC_NMI), line))
                    else:
                        lines.append(line)
                elif re.search(p_lapic_nmi_flags, line):
                    if lapic_nmi_index == 1 and p_lapic_nmi_flags_index == 0:
                        lines.append(
                            re.sub(p_lapic_nmi_flags, '[0002]        Flags (decoded below) : {0:04X}'.format(0x5),
                                   line))
                        p_lapic_nmi_flags_index += 1
                    else:
                        lines.append(line)
                elif re.search(p_lapic_nmi_lint, line):
                    if lapic_nmi_index == 1:
                        lines.append(re.sub(p_lapic_nmi_lint, 'Interrupt Input LINT : {0:02X}'.format(0x1), line))
                    else:
                        lines.append(line)
                else:
                    lines.append(line)

                if p_lapic_index == 1 and p_lapic_line_index < 7:
                    lapic_lines.append(line)
                    p_lapic_line_index += 1
                if p_lapic_index == 1 and p_lapic_line_index == 7:
                    p_lapic_line_index = 0
                    for process_id in range(1, max_cpu_num):
                        p_lapic_index = process_id + 1
                        lines.append('\n')
                        for lapic_line in lapic_lines:
                            if re.search(p_lapic_type, lapic_line):
                                lines.append(re.sub(p_lapic_type,
                                                    'Subtable Type : {0:02X} [Processor Local APIC]'.format(
                                                        ACPI_MADT_TYPE_LOCAL_APIC), lapic_line))
                            elif re.search(p_lapic_len, lapic_line):
                                lines.append(
                                    re.sub(p_lapic_len,
                                           '[0001]                       Length : {0:02X}'.format(0x8),
                                           lapic_line))
                            elif re.search(p_flags, lapic_line):
                                lines.append(
                                    re.sub(p_flags,
                                           '[0004]              Flags (decoded below) : {0:08X}'.format(0x1),
                                           lapic_line))
                            elif re.search(p_lapic_process_id, lapic_line):
                                lines.append(re.sub(p_lapic_process_id,
                                                    '[0001]                 Processor ID : {0:02X}'.format(
                                                        process_id), lapic_line))
                            elif re.search(p_lapic_id, lapic_line):
                                lines.append(
                                    re.sub(p_lapic_id, 'Local Apic ID : {0:02X}'.format(apic_ids[process_id]), lapic_line))
                            else:
                                lines.append(lapic_line)

        dest.writelines(lines)


def gen_tpm2(dest_vm_acpi_path, passthru_devices):
    '''
    generate tpm2.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :param passthru_devices: dict to store passthru device list
    :return:
    '''
    tpm2_asl = 'tpm2.asl'
    p_control_addr = r'Control Address : ([0-9a-fA-F]{16})'
    p_start_method = r'Start Method : (.*)'

    if 'TPM2' not in passthru_devices:
        if os.path.isfile(os.path.join(dest_vm_acpi_path, tpm2_asl)):
            os.remove(os.path.join(dest_vm_acpi_path, tpm2_asl))
        return

    with open(os.path.join(dest_vm_acpi_path, tpm2_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, tpm2_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_control_addr, line):
                    lines.append(re.sub(p_control_addr, 'Control Address : {0:016X}'.format(0xFED40040), line))
                elif re.search(p_start_method, line):
                    lines.append(re.sub(p_start_method, 'Start Method : {0:02X}'.format(0x7), line))
                else:
                    lines.append(line)
        dest.writelines(lines)


def gen_dsdt(dest_vm_acpi_path, passthru_devices, board_info):
    '''
    generate dsdt.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :param passthru_devices:
    :return:
    '''
    dsdt_asl = 'dsdt.asl'
    p_dsdt_start = r'{'
    (bdf_desc_map, bdf_vpid_map) = board_cfg_lib.get_pci_info(board_info)
    with open(os.path.join(dest_vm_acpi_path, dsdt_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, dsdt_asl), 'r') as src:
            for line in src.readlines():
                lines.append(line)
                if line.startswith(p_dsdt_start):
                    for passthru_device in passthru_devices:
                        passthru_bdf = None
                        passthru_vpid = None
                        for bdf in bdf_desc_map.keys():
                            if bdf_desc_map[bdf].find(passthru_device) >= 0:
                                passthru_bdf = bdf
                                break
                        if passthru_bdf is not None and passthru_bdf in bdf_vpid_map.keys():
                            passthru_vpid = bdf_vpid_map[passthru_bdf]
                        if passthru_device in ['TPM2']:
                            tpm2_asl = os.path.join(TEMPLATE_ACPI_PATH, 'dsdt_tpm2.asl')
                            start = False
                            with open(tpm2_asl, 'r') as tpm2_src:
                                for tpm2_line in tpm2_src.readlines():
                                    if tpm2_line.startswith('{'):
                                        start = True
                                        continue
                                    if tpm2_line.startswith('}'):
                                        start = False
                                        continue
                                    if start:
                                        lines.append(tpm2_line)
                        elif passthru_vpid is not None and passthru_vpid == TSN_DEVICE_LIST[1]:
                            otn1_asl = os.path.join(TEMPLATE_ACPI_PATH, 'dsdt_tsn_otn1.asl')
                            start = False
                            with open(otn1_asl, 'r') as otn1_src:
                                for otn1_line in otn1_src.readlines():
                                    if otn1_line.startswith('{'):
                                        start = True
                                        continue
                                    if otn1_line.startswith('}'):
                                        start = False
                                        continue
                                    if start:
                                        lines.append(otn1_line)
                        else:
                            pass
        dest.writelines(lines)


def main(args):

    err_dic = {}

    (err_dic, params) = common.get_param(args)
    if err_dic:
        return err_dic

    board = params['--board']
    scenario= params['--scenario']
    out = params['--out']

    board_etree = lxml.etree.parse(board)
    board_root = ElementTree.parse(board).getroot()
    scenario_root = ElementTree.parse(scenario).getroot()
    board_type = board_root.attrib['board']
    scenario_name = scenario_root.attrib['scenario']
    pcpu_list = board_root.find('CPU_PROCESSOR_INFO').text.strip().split(',')
    if isinstance(pcpu_list, list):
        pcpu_list = [x.strip() for x in pcpu_list]
    if out is None or out == '':
        DEST_ACPI_PATH = os.path.join(VM_CONFIGS_PATH, 'scenarios', scenario_name)
    else:
        DEST_ACPI_PATH = os.path.join(common.SOURCE_ROOT_DIR, out, 'scenarios', scenario_name)

    if os.path.isdir(DEST_ACPI_PATH):
        for config in os.listdir(DEST_ACPI_PATH):
            if config.startswith('ACPI_VM') and os.path.isdir(os.path.join(DEST_ACPI_PATH, config)):
                shutil.rmtree(os.path.join(DEST_ACPI_PATH, config))

    dict_passthru_devices = collections.OrderedDict()
    dict_pcpu_list = collections.OrderedDict()
    for vm in scenario_root.findall('vm'):
        vm_id = vm.attrib['id']
        vm_type_node = vm.find('vm_type')
        if (vm_type_node is not None) and (vm_type_node.text in ['PRE_STD_VM', 'SAFETY_VM', 'PRE_RT_VM']):
            dict_passthru_devices[vm_id] = []
            for pci_dev_node in vm.findall('pci_devs/pci_dev'):
                if pci_dev_node is not None and pci_dev_node.text is not None and pci_dev_node.text.strip():
                    dict_passthru_devices[vm_id].append(pci_dev_node.text)
            mmio_dev_nodes = vm.find('mmio_resources')
            if mmio_dev_nodes is not None:
                for mmio_dev_node in list(mmio_dev_nodes):
                    if mmio_dev_node is not None and mmio_dev_node.text.strip() == 'y':
                        dict_passthru_devices[vm_id].append(mmio_dev_node.tag)
            dict_pcpu_list[vm_id] = []
            for pcpu_id in vm.findall('cpu_affinity/pcpu_id'):
                if pcpu_id is not None and pcpu_id.text.strip() in pcpu_list:
                    dict_pcpu_list[vm_id].append(int(pcpu_id.text))

    PASSTHROUGH_RTCT = False
    PRELAUNCHED_RTVM_ID = None
    try:
        if scenario_root.find('hv/FEATURES/SSRAM/SSRAM_ENABLED').text.strip() == 'y':
            PASSTHROUGH_RTCT = True
        for vm in scenario_root.findall('vm'):
            vm_id = vm.attrib['id']
            vm_type_node = vm.find('vm_type')
            if (vm_type_node is not None) and (vm_type_node.text in ['PRE_RT_VM']):
                PRELAUNCHED_RTVM_ID = vm_id
                break
    except:
        PASSTHROUGH_RTCT = False

    kern_args = common.get_leaf_tag_map(scenario, "os_config", "bootargs")
    kern_type = common.get_leaf_tag_map(scenario, "os_config", "kern_type")
    for vm_id, passthru_devices in dict_passthru_devices.items():
        if kern_args[int(vm_id)].find('reboot=acpi') == -1 and kern_type[int(vm_id)] in ['KERNEL_BZIMAGE']:
            emsg = "you need to specify 'reboot=acpi' in scenario file's bootargs for VM{}".format(vm_id)
            print(emsg)
            err_dic['vm,bootargs'] = emsg
            break

        print('start to generate ACPI ASL code for VM{}'.format(vm_id))
        dest_vm_acpi_path = os.path.join(DEST_ACPI_PATH, 'ACPI_VM'+vm_id)
        if not os.path.isdir(dest_vm_acpi_path):
            os.makedirs(dest_vm_acpi_path)
        if PASSTHROUGH_RTCT is True and vm_id == PRELAUNCHED_RTVM_ID:
                for f in RTCT:
                    if os.path.isfile(os.path.join(VM_CONFIGS_PATH, 'acpi_template', board_type, f)):
                        passthru_devices.append(f)
                        shutil.copy(os.path.join(VM_CONFIGS_PATH, 'acpi_template', board_type, f),
                                    dest_vm_acpi_path)
                        break
        gen_rsdp(dest_vm_acpi_path)
        gen_xsdt(dest_vm_acpi_path, passthru_devices)
        gen_fadt(dest_vm_acpi_path, board_root)
        gen_mcfg(dest_vm_acpi_path)
        if vm_id in dict_pcpu_list:
            dict_pcpu_list[vm_id].sort()

            apic_ids = []
            for id in dict_pcpu_list[vm_id]:
                apic_id = common.get_node(f"//processors/die/core/thread[cpu_id='{id}']/apic_id/text()", board_etree)
                if apic_id is None:
                    emsg = 'some or all of the processors/die/core/thread/cpu_id tags are missing in board xml file for cpu {}, please run board_inspector.py to regenerate the board xml file!'.format(id)
                    print(emsg)
                    err_dic['board config: processors'] = emsg
                    return err_dic
                else:
                    apic_ids.append(int(apic_id, 16))

            gen_madt(dest_vm_acpi_path, len(dict_pcpu_list[vm_id]), apic_ids)
            gen_tpm2(dest_vm_acpi_path, passthru_devices)
            gen_dsdt(dest_vm_acpi_path, passthru_devices, board)
            print('generate ASL code of ACPI tables for VM {} into {}'.format(vm_id, dest_vm_acpi_path))
        else:
            emsg = 'no cpu affinity config for VM {}'.format(vm_id)
            print(emsg)
            err_dic['vm,cpu_affinity,pcpu_id'] = emsg

    return err_dic


if __name__ == '__main__':

    main(sys.argv)

