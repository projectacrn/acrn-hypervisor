# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import scenario_cfg_lib
import common

C_HEADER = scenario_cfg_lib.HEADER_LICENSE + r"""
#include <vm_config.h>
#include <vuart.h>
#include <pci_dev.h>
"""


def uuid2str(uuid):
    """
    This is convert uuid format string to string format
    :param uuid: uuid number generate by uuid
    :return: string uuid
    """
    str1 = ""
    tmp_uuid = uuid.split('-')
    tmp_uuid = str1.join(tmp_uuid)
    return tmp_uuid


def uuid_output(uuid, uuid_string, config):
    """
    This is generate uuid information
    :param uuid:
    :param uuid_string:
    :param config:
    :return:
    """
    # UUID
    print("\t\t.uuid = {{0x{0}U, 0x{1}U, 0x{2}U, 0x{3}U, 0x{4}U, 0x{5}U, 0x{6}U, 0x{7}U,\t\\".
          format(uuid[0:2], uuid[2:4], uuid[4:6], uuid[6:8],
                 uuid[8:10], uuid[10:12], uuid[12:14], uuid[14:16]), file=config)
    print("\t\t\t 0x{0}U, 0x{1}U, 0x{2}U, 0x{3}U, 0x{4}U, 0x{5}U, 0x{6}U, 0x{7}U}},".
          format(uuid[16:18], uuid[18:20], uuid[20:22], uuid[22:24],
                 uuid[24:26], uuid[26:28], uuid[28:30], uuid[30:32]), file=config)
    print("\t\t\t/* {0} */".format(uuid_string), file=config)


def vuart0_output(i, vm_type, vm_info, config):
    """
    This is generate vuart 0 setting
    :param i: vm id number
    :param vm_type: vm load order type
    :param vm_info: it is the class which contain all user setting information
    :param config: it is the pointer which file write to
    :return: None
    """
    # SOS_VM vuart[0]
    print("\t\t.vuart[0] = {", file=config)
    print("\t\t\t.type = {0},".format(vm_info.vuart.v0_vuart[i]['type']), file=config)
    if vm_type == "SOS_VM":
        print("\t\t\t.addr.port_base = SOS_COM1_BASE,", file=config)
        print("\t\t\t.irq = SOS_COM1_IRQ,", file=config)
    elif vm_type == "PRE_LAUNCHED_VM":
        print("\t\t\t.addr.port_base = COM1_BASE,", file=config)
        print("\t\t\t.irq = COM1_IRQ,", file=config)
    elif vm_type == "POST_LAUNCHED_VM":
        print("\t\t\t.addr.port_base = {0},".format(
            vm_info.vuart.v0_vuart[i]['base']), file=config)
        if vm_info.vuart.v0_vuart[i]['base'] != "INVALID_COM_BASE":
            print("\t\t\t.irq = {0},".format(
                vm_info.vuart.v0_vuart[i]['irq']), file=config)
    print("\t\t},", file=config)


def vuart1_output(i, vm_type, vuart1_vmid_dic, vm_info, config):
    """
    This is generate vuart 1 setting
    :param i: vm id number
    :param vm_type: vm load order type
    :param vuart1_vmid_dic: vuart1 and vm id mapping
    :param vm_info: it is the class which contain all user setting information
    :param config: it is the pointer which file write to
    :return: None
    """
    # vuart1:   {vmid:target_vmid}
    print("\t\t.vuart[1] = {", file=config)
    print("\t\t\t.type = {0},".format(vm_info.vuart.v1_vuart[i]['type']), file=config)
    if vm_type == "SOS_VM":
        if vm_info.vuart.v1_vuart[i]['base'] != "INVALID_COM_BASE":
            print("\t\t\t.addr.port_base = SOS_COM2_BASE,", file=config)
        else:
            print("\t\t\t.addr.port_base = INVALID_COM_BASE,", file=config)
    else:
        print("\t\t\t.addr.port_base = {0},".format(
            vm_info.vuart.v1_vuart[i]['base']), file=config)
    if vuart1_vmid_dic and i in vuart1_vmid_dic.keys():
        if vm_type == "SOS_VM":
            if vm_info.vuart.v1_vuart[i]['base'] != "INVALID_COM_BASE":
                print("\t\t\t.irq = SOS_COM2_IRQ,", file=config)
        else:
            if vm_info.vuart.v1_vuart[i]['base'] != "INVALID_COM_BASE":
                print("\t\t\t.irq = COM2_IRQ,", file=config)

        if vm_info.vuart.v1_vuart[i]['base'] != "INVALID_COM_BASE":
            print("\t\t\t.t_vuart.vm_id = {0}U,".format(
                vm_info.vuart.v1_vuart[i]['target_vm_id']), file=config)
            print("\t\t\t.t_vuart.vuart_id = {0}U,".format(
                vm_info.vuart.v1_vuart[i]['target_uart_id']), file=config)


def vuart_output(i, vm_info, config):
    """
    This is generate vuart setting
    :param i: vm id number
    :param vm_info: it is the class which contain all user setting information
    :param config: it is the pointer which file write to
    :return: None
    """
    vuart1_vmid_dic = {}
    vuart1_vmid_dic = scenario_cfg_lib.get_vuart1_vmid(vm_info.vuart.v1_vuart)
    vm_type = scenario_cfg_lib.get_order_type_by_vmid(i)

    vuart0_output(i, vm_type, vm_info, config)
    vuart1_output(i, vm_type, vuart1_vmid_dic, vm_info, config)
    print("\t\t},", file=config)

    # pci_dev_num/pci_devs only for SOS_VM or logical_partition pre_launched_vm
    if vm_type == "SOS_VM":
        print("\t\t.pci_dev_num = {},".format(vm_info.cfg_pci.pci_dev_num[0]), file=config)
        print("\t\t.pci_devs = {},".format(vm_info.cfg_pci.pci_devs[0]), file=config)

    (err_dic, scenario_name) = scenario_cfg_lib.get_scenario_name()
    if err_dic:
        return err_dic

    if scenario_name == "logical_partition":
        print("\t\t.pci_dev_num = VM{}_CONFIG_PCI_DEV_NUM,".format(i), file=config)
        print("\t\t.pci_devs = vm{}_pci_devs,".format(i), file=config)
    print("\t},", file=config)

    return err_dic


def is_need_epc(epc_section, i, config):
    if epc_section.base[i] == '0' and epc_section.size[i] == '0':
        return
    else:
        print("\t\t.epc= {", file=config)
        print('\t\t\t.base = "{0}",'.format(epc_section.base), file=config)
        print('\t\t\t.size = {0},'.format(epc_section.size), file=config)
        print("\t\t},", file=config)

def get_guest_flag(flag_index):
    """
    This is get flag index list
    :param flag_index:
    :return: flag index list in GUEST_FLAGS
    """
    err_dic = {}
    flag_str = ''
    if not flag_index:
        err_dic['guest flags'] = "No assign flag to the guest"
        return (err_dic, flag_str)

    for i in range(len(flag_index)):
        if i == 0:
            if len(flag_index) == 1:
                # get the guest flag 0UL
                if flag_index[0] == 0:
                    return (err_dic, common.GUEST_FLAG[flag_index[0]])
                flag_str = "{0}".format(common.GUEST_FLAG[int(flag_index[0])])
            else:
                flag_str = "({0}".format(common.GUEST_FLAG[int(flag_index[0])])

        else:
            # flag_index lenght already minus 1
            if i == len(flag_index) - 1:
                flag_str = flag_str + " | {0})".format(common.GUEST_FLAG[int(flag_index[i])])
            else:
                flag_str = flag_str + " | {0}".format(common.GUEST_FLAG[int(flag_index[i])])

    return (err_dic, flag_str)


def gen_source_header(config):
    """
    This is the common header for vm_configuration.c
    :param config: it is the pointer which file write to
    :return: None
    """
    print("{0}".format(C_HEADER), file=config)


def gen_sdc_source(vm_info, config):
    """
    Generate vm_configuration.c of sdc scenario
    :param vm_info: it is the class which contain all user setting information
    :param config: it is the pointer which file write to
    :return: None
    """
    uuid_0 = uuid2str(vm_info.uuid[0])
    uuid_1 = uuid2str(vm_info.uuid[1])
    uuid_2 = uuid2str(vm_info.uuid[2])

    (err_dic, sos_guest_flags) = get_guest_flag(vm_info.guest_flag_idx[0])
    if err_dic:
        return err_dic

    gen_source_header(config)
    # VM0
    print("struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {", file=config)
    print("\t{", file=config)
    print("\t\t.load_order = {0},".format(
        vm_info.load_order[0]), file=config)
    print('\t\t.name = "{0}",'.format(vm_info.name[0]), file=config)
    # UUID
    uuid_output(uuid_0, vm_info.uuid[0], config)
    print("", file=config)
    print("\t\t/* Allow SOS to reboot the host since " +
          "there is supposed to be the highest severity guest */", file=config)
    print("\t\t.guest_flags = {0},".format(sos_guest_flags), file=config)
    if vm_info.clos_set[0] == None or not vm_info.clos_set[0].strip():
        print("\t\t.clos = {0}U,".format(0), file=config)
    else:
        print("\t\t.clos = {0}U,".format(vm_info.clos_set[0]), file=config)
    print("\t\t.memory = {", file=config)
    print("\t\t\t.start_hpa = {}UL,".format(vm_info.mem_info.mem_start_hpa[0]), file=config)
    print("\t\t\t.size = {0},".format("CONFIG_SOS_RAM_SIZE"), file=config)
    print("\t\t},", file=config)
    print("\t\t.os_config = {", file=config)
    print('\t\t\t.name = "{0}",'.format(vm_info.os_cfg.kern_name[0]), file=config)
    print('\t\t\t.kernel_type = {0},'.format(vm_info.os_cfg.kern_type[0]), file=config)
    print('\t\t\t.kernel_mod_tag = "{0}",'.format(
        vm_info.os_cfg.kern_mod[0]), file=config)
    print('\t\t\t.bootargs = {0},'.format(vm_info.os_cfg.kern_args[0]), file=config)
    print("\t\t},", file=config)
    # VUART
    err_dic = vuart_output(0, vm_info, config)
    if err_dic:
        return err_dic
    # VM1
    print("\t{", file=config)
    print("\t\t.load_order = {0},".format(vm_info.load_order[1]), file=config)
    # UUID
    uuid_output(uuid_1, vm_info.uuid[1], config)
    is_need_epc(vm_info.epc_section, 0, config)
    # VUART
    err_dic = vuart_output(1, vm_info, config)
    if err_dic:
        return err_dic
    # VM2
    print("#if CONFIG_MAX_KATA_VM_NUM > 0", file=config)
    print("\t{", file=config)
    print("\t\t.load_order = POST_LAUNCHED_VM,", file=config)
    uuid_output(uuid_2, vm_info.uuid[2], config)
    is_need_epc(vm_info.epc_section, 1, config)
    print("\t\t.vuart[0] = {", file=config)
    print("\t\t\t.type = VUART_LEGACY_PIO,", file=config)
    print("\t\t\t.addr.port_base = INVALID_COM_BASE,", file=config)
    print("\t\t},", file=config)
    print("\t\t.vuart[1] = {", file=config)
    print("\t\t\t.type = VUART_LEGACY_PIO,", file=config)
    print("\t\t\t.addr.port_base = INVALID_COM_BASE,", file=config)
    print("\t\t}", file=config)
    print("\t},", file=config)
    print("#endif", file=config)
    print("};", file=config)

    return err_dic


def gen_sdc2_source(vm_info, config):
    """
    Generate vm_configuration.c of sdc2 scenario
    :param vm_info: it is the class which contain all user setting information
    :param config: it is the pointer which file write to
    :return: None
    """
    uuid_0 = uuid2str(vm_info.uuid[0])
    uuid_1 = uuid2str(vm_info.uuid[1])
    uuid_2 = uuid2str(vm_info.uuid[2])
    uuid_3 = uuid2str(vm_info.uuid[3])

    (err_dic, sos_guest_flags) = get_guest_flag(vm_info.guest_flag_idx[0])
    if err_dic:
        return err_dic

    gen_source_header(config)

    # VM0
    print("struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {", file=config)
    print("\t{", file=config)
    print("\t\t.load_order = {0},".format(
        vm_info.load_order[0]), file=config)
    print('\t\t.name = "{0}",'.format(vm_info.name[0]), file=config)
    # UUID
    uuid_output(uuid_0, vm_info.uuid[0], config)
    print("", file=config)
    print("\t\t/* Allow SOS to reboot the host since " +
          "there is supposed to be the highest severity guest */", file=config)
    print("\t\t.guest_flags = {0},".format(sos_guest_flags), file=config)
    if vm_info.clos_set[0] == None or not vm_info.clos_set[0].strip():
        print("\t\t.clos = {0}U,".format(0), file=config)
    else:
        print("\t\t.clos = {0}U,".format(vm_info.clos_set[0]), file=config)
    print("\t\t.memory = {", file=config)
    print("\t\t\t.start_hpa = {}UL,".format(vm_info.mem_info.mem_start_hpa[0]), file=config)
    print("\t\t\t.size = {0},".format("CONFIG_SOS_RAM_SIZE"), file=config)
    print("\t\t},", file=config)
    print("\t\t.os_config = {", file=config)
    print('\t\t\t.name = "{0}",'.format(vm_info.os_cfg.kern_name[0]), file=config)
    print('\t\t\t.kernel_type = {0},'.format(vm_info.os_cfg.kern_type[0]), file=config)
    print('\t\t\t.kernel_mod_tag = "{0}",'.format(
        vm_info.os_cfg.kern_mod[0]), file=config)
    print('\t\t\t.bootargs = {0},'.format(vm_info.os_cfg.kern_args[0]), file=config)
    print("\t\t},", file=config)
    # VUART
    err_dic = vuart_output(0, vm_info, config)
    if err_dic:
        return err_dic

    # VM1
    print("\t{", file=config)
    print("\t\t.load_order = {0},".format(vm_info.load_order[1]), file=config)
    # UUID
    uuid_output(uuid_1, vm_info.uuid[1], config)
    is_need_epc(vm_info.epc_section, 0, config)
    # VUART
    err_dic = vuart_output(1, vm_info, config)
    if err_dic:
        return err_dic

    # VM2
    print("\t{", file=config)
    print("\t\t.load_order = {0},".format(vm_info.load_order[1]), file=config)
    # UUID
    uuid_output(uuid_2, vm_info.uuid[2], config)
    is_need_epc(vm_info.epc_section, 1, config)
    # VUART
    err_dic = vuart_output(1, vm_info, config)
    if err_dic:
        return err_dic
    print("", file=config)

    # VM3
    print("\t{", file=config)
    print("\t\t.load_order = POST_LAUNCHED_VM,", file=config)
    uuid_output(uuid_3, vm_info.uuid[3], config)
    is_need_epc(vm_info.epc_section, 2, config)
    print("\t\t.vuart[0] = {", file=config)
    print("\t\t\t.type = VUART_LEGACY_PIO,", file=config)
    print("\t\t\t.addr.port_base = INVALID_COM_BASE,", file=config)
    print("\t\t},", file=config)
    print("\t\t.vuart[1] = {", file=config)
    print("\t\t\t.type = VUART_LEGACY_PIO,", file=config)
    print("\t\t\t.addr.port_base = INVALID_COM_BASE,", file=config)
    print("\t\t}", file=config)
    print("\t},", file=config)
    print("};", file=config)

    return err_dic


def split_cmdline(cmd_str, config):

    cmd_list = [i for i in cmd_str.split() if i != '']

    if cmd_list:
        cmd_len = len(cmd_list)
        i = 0
        for cmd_arg in cmd_list:
            if not cmd_arg.strip():
                continue

            if i == 0:
                print('\\\n\t\t\t\t"', end="", file=config)

            if i % 4 == 0 and i != 0:
                print("\\\n\t\t\t\t", end="", file=config)

            print('{} '.format(cmd_arg), end="", file=config)
            i += 1
            if i == cmd_len:
                print('"', file=config)



def gen_logical_partition_source(vm_info, config):
    """
    Generate vm_configuration.c of logical_partition scenario
    :param vm_info: it is the class which contain all user setting information
    :param config: it is the pointer which file write to
    :return: None
    """
    err_dic = {}
    gen_source_header(config)
    for i in range(scenario_cfg_lib.VM_COUNT):
        print("extern struct acrn_vm_pci_dev_config " +
              "vm{0}_pci_devs[VM{1}_CONFIG_PCI_DEV_NUM];".format(i, i), file=config)
    print("", file=config)
    print("struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {", file=config)

    for i in range(scenario_cfg_lib.VM_COUNT):
        uuid = uuid2str(vm_info.uuid[i])
        print("\t{{\t/* VM{0} */".format(i), file=config)
        print("\t\t.load_order = {0},".format(vm_info.load_order[i]), file=config)
        print('\t\t.name = "{0}",'.format(vm_info.name[i]), file=config)
        # UUID
        uuid_output(uuid, vm_info.uuid[i], config)
        print("\t\t.pcpu_bitmap = VM{0}_CONFIG_PCPU_BITMAP,".format(i), file=config)
        # skip the vm0 for guest flag

        # guest flags
        (err_dic, guest_flags) = get_guest_flag(vm_info.guest_flag_idx[i])
        if err_dic:
            return err_dic

        print("\t\t.guest_flags = {0},".format(guest_flags), file=config)

        if vm_info.clos_set[i] == None or not vm_info.clos_set[i].strip():
            print("\t\t.clos = {0}U,".format(0), file=config)
        else:
            print("\t\t.clos = {0}U,".format(vm_info.clos_set[i]), file=config)
        print("\t\t.memory = {", file=config)
        print("\t\t\t.start_hpa = VM{0}_CONFIG_MEM_START_HPA,".format(i), file=config)
        print("\t\t\t.size = VM{0}_CONFIG_MEM_SIZE,".format(i), file=config)
        print("\t\t},", file=config)
        is_need_epc(vm_info.epc_section, i, config)
        print("\t\t.os_config = {", file=config)
        print('\t\t\t.name = "{0}",'.format(vm_info.os_cfg.kern_name[i]), file=config)
        print("\t\t\t.kernel_type = {0},".format(
            vm_info.os_cfg.kern_type[i]), file=config)
        print('\t\t\t.kernel_mod_tag = "{0}",'.format(
            vm_info.os_cfg.kern_mod[i]), file=config)
        print("\t\t\t.bootargs = VM{0}_CONFIG_OS_BOOTARG_CONSOLE\t\\".format(i), file=config)
        print("\t\t\t\tVM{0}_CONFIG_OS_BOOTARG_MAXCPUS\t\t\\".format(i), file=config)
        print("\t\t\t\tVM{0}_CONFIG_OS_BOOTARG_ROOT\t\t\\".format(i), file=config)
        #print("\t\t\t\t{0}".format(vm_info.os_cfg.kern_args_append[i].strip()), file=config)
        split_cmdline(vm_info.os_cfg.kern_args[i].strip(), config)
        print("\t\t},", file=config)
        # VUART
        err_dic = vuart_output(i, vm_info, config)
        if err_dic:
            return err_dic

    print("};", file=config)

    return err_dic


def gen_industry_source(vm_info, config):
    """
    Generate vm_configuration.c of industry scenario
    :param vm_info: it is the class which contain all user setting information
    :param config: it is the pointer which file write to
    :return: None
    """
    err_dic = {}
    gen_source_header(config)
    print("struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {", file=config)
    for i in range(scenario_cfg_lib.VM_COUNT):
        uuid = uuid2str(vm_info.uuid[i])
        print("\t{", file=config)
        print("\t\t.load_order = {0},".format(vm_info.load_order[i]), file=config)
        if i == 0:
            print('\t\t.name = "{0}",'.format(vm_info.name[i]), file=config)

        # UUID
        uuid_output(uuid, vm_info.uuid[i], config)
        if i != 0:
            is_need_epc(vm_info.epc_section, i - 1, config)

        if i == 0:
            (err_dic, sos_guest_flags) = get_guest_flag(vm_info.guest_flag_idx[i])
            if err_dic:
                return err_dic
            print("\t\t")
            print("\t\t.guest_flags = {0},".format(sos_guest_flags), file=config)
            if vm_info.clos_set[i] == None or not vm_info.clos_set[i].strip():
                print("\t\t.clos = {0}U,".format(0), file=config)
            else:
                print("\t\t.clos = {0}U,".format(vm_info.clos_set[i]), file=config)
            print("\t\t.memory = {", file=config)
            print("\t\t\t.start_hpa = 0UL,", file=config)
            print("\t\t\t.size = CONFIG_SOS_RAM_SIZE,", file=config)
            print("\t\t},", file=config)
            print("\t\t.os_config = {", file=config)
            print('\t\t\t.name = "{0}",'.format(vm_info.os_cfg.kern_name[i]), file=config)
            print('\t\t\t.kernel_type = {0},'.format(
                vm_info.os_cfg.kern_type[i]), file=config)
            print('\t\t\t.kernel_mod_tag = "{0}",'.format(
                vm_info.os_cfg.kern_mod[i]), file=config)
            print("\t\t\t.bootargs = {0}".format(
                vm_info.os_cfg.kern_args[i]), file=config)
            print("\t\t},", file=config)

        if i == 2:
            print("\t\t/* The hard RTVM must be launched as VM2 */", file=config)
            (err_dic, vm_guest_flags) = get_guest_flag(vm_info.guest_flag_idx[i])
            if err_dic:
                return err_dic
            print("\t\t.guest_flags = {0},".format(vm_guest_flags), file=config)
        # VUART
        err_dic = vuart_output(i, vm_info, config)
        if err_dic:
            return err_dic

    print("};", file=config)

    return err_dic


def gen_hybrid_source(vm_info, config):
    """
    Generate vm_configuration.c of hybrid scenario
    :param vm_info: it is the class which contain all user setting information
    :param config: it is the pointer which file write to
    :return: None
    """
    err_dic = {}
    (err_dic, post_vm_i) = scenario_cfg_lib.get_first_post_vm()
    if err_dic:
        return err_dic

    gen_source_header(config)
    print("struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {", file=config)

    for i in range(scenario_cfg_lib.VM_COUNT):
        uuid = uuid2str(vm_info.uuid[i])
        print("\t{{\t/* VM{0} */".format(i), file=config)
        print("\t\t.load_order = {0},".format(vm_info.load_order[i]), file=config)
        if i != 2:
            print('\t\t.name = "{0}",'.format(vm_info.name[i]), file=config)

        # UUID
        uuid_output(uuid, vm_info.uuid[i], config)

        if i != 2:
            (err_dic, sos_guest_flags) = get_guest_flag(vm_info.guest_flag_idx[i])
            if err_dic:
                return err_dic
            print("\t\t.guest_flags = {0},".format(sos_guest_flags), file=config)
            if i == 0:
                print("\t\t.pcpu_bitmap = VM0_CONFIG_PCPU_BITMAP,", file=config)
            if vm_info.clos_set[i] == None or not vm_info.clos_set[i].strip():
                print("\t\t.clos = {0}U,".format(0), file=config)
            else:
                print("\t\t.clos = {0}U,".format(vm_info.clos_set[i]), file=config)
            print("\t\t.memory = {", file=config)
            if i == 0:
                print("\t\t\t.start_hpa = VM0_CONFIG_MEM_START_HPA,", file=config)
                print("\t\t\t.size = VM0_CONFIG_MEM_SIZE,", file=config)
            elif i == 1:
                print("\t\t\t.start_hpa = 0UL,", file=config)
                print("\t\t\t.size = CONFIG_SOS_RAM_SIZE,", file=config)
            print("\t\t},", file=config)
            if i == 0:
              is_need_epc(vm_info.epc_section, i, config)
            elif i == scenario_cfg_lib.VM_COUNT:
              is_need_epc(vm_info.epc_section, i - 1, config)
            print("\t\t.os_config = {", file=config)
            print('\t\t\t.name = "{0}",'.format(vm_info.os_cfg.kern_name[i]), file=config)
            print('\t\t\t.kernel_type = {0},'.format(
                vm_info.os_cfg.kern_type[i]), file=config)
            print('\t\t\t.kernel_mod_tag = "{0}",'.format(
                vm_info.os_cfg.kern_mod[i]), file=config)

            if i < post_vm_i:
                if not vm_info.os_cfg.kern_args[i] or not vm_info.os_cfg.kern_args[i].strip():
                    print('\t\t\t.bootargs = "",', file=config)
                else:
                    print("\t\t\t.bootargs = {0},".format(
                        vm_info.os_cfg.kern_args[i]), file=config)
            if i == 0:
                print("\t\t\t.kernel_load_addr = {0},".format(
                    vm_info.os_cfg.kern_load_addr[i]), file=config)
                print("\t\t\t.kernel_entry_addr = {0},".format(
                    vm_info.os_cfg.kern_entry_addr[i]), file=config)

            print("\t\t},", file=config)

        # VUART
        err_dic = vuart_output(i, vm_info, config)
        if err_dic:
            return err_dic
    print("};", file=config)

    return err_dic


def generate_file(scenario, vm_info, config):
    """
    Start to generate vm_configurations.c
    :param config: it is a file pointer of board information for writing to
    """
    err_dic = {}
    if scenario == 'sdc':
        err_dic = gen_sdc_source(vm_info, config)
    elif scenario == 'sdc2':
        err_dic = gen_sdc2_source(vm_info, config)
    elif scenario == 'logical_partition':
        err_dic = gen_logical_partition_source(vm_info, config)
    elif scenario == 'industry':
        err_dic = gen_industry_source(vm_info, config)
    else:
        # scenario is 'hybrid'
        err_dic = gen_hybrid_source(vm_info, config)

    return err_dic
