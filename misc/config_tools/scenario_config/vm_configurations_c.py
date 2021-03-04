# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import common
import board_cfg_lib
import scenario_cfg_lib

C_HEADER = scenario_cfg_lib.HEADER_LICENSE + r"""
#include <x86/vm_config.h>
#include <vuart.h>
#include <x86/pci_dev.h>
"""

def get_pre_vm_type(vm_type, vm_i):

    if vm_type == "SAFETY_VM":
        return "CONFIG_SAFETY_VM(1)"

    if vm_type == "PRE_RT_VM":
        return "CONFIG_PRE_RT_VM(1)"

    i_cnt = 0
    for i,v_type in common.VM_TYPES.items():
        if v_type == "PRE_STD_VM" and i <= vm_i:
            i_cnt += 1
    return "CONFIG_PRE_STD_VM({})".format(i_cnt)


def get_post_vm_type(vm_type, vm_i):

    if vm_type == "KATA_VM":
        return "CONFIG_KATA_VM(1)"

    if vm_type == "POST_RT_VM":
        return "CONFIG_POST_RT_VM(1)"

    i_cnt = 0
    for i,v_type in common.VM_TYPES.items():
        if v_type == "POST_STD_VM" and i <= vm_i:
            i_cnt += 1
    return "CONFIG_POST_STD_VM({})".format(i_cnt)


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
    if vm_info.vuart.v0_vuart[i]['base'] == "INVALID_COM_BASE":
        print("\t\t\t.addr.port_base = INVALID_COM_BASE,", file=config)
        if "SOS_" in vm_type:
            print("\t\t\t.irq = SOS_COM1_IRQ,", file=config)
        elif "PRE_LAUNCHED_VM" == scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            print("\t\t\t.irq = COM1_IRQ,", file=config)
        elif "POST_LAUNCHED_VM" in scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            print("\t\t\t.irq = {0},".format(
                    vm_info.vuart.v0_vuart[i]['irq']), file=config)
    else:
        if "SOS_" in vm_type:
            print("\t\t\t.addr.port_base = SOS_COM1_BASE,", file=config)
            print("\t\t\t.irq = SOS_COM1_IRQ,", file=config)
        elif "PRE_LAUNCHED_VM" == scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            print("\t\t\t.addr.port_base = COM1_BASE,", file=config)
            print("\t\t\t.irq = COM1_IRQ,", file=config)
        elif "POST_LAUNCHED_VM" in scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            print("\t\t\t.addr.port_base = {0},".format(
                vm_info.vuart.v0_vuart[i]['base']), file=config)
            print("\t\t\t.irq = {0},".format(
                vm_info.vuart.v0_vuart[i]['irq']), file=config)
    print("\t\t},", file=config)


def vuart_map_enable(vm_info):

    map_dic = {}
    for i,vm_type in common.VM_TYPES.items():
        base_i = vm_info.vuart.v1_vuart[i]['base']
        src_t_vm_i = vm_info.vuart.v1_vuart[i]['target_vm_id']
        src_t_vuart_i = vm_info.vuart.v1_vuart[i]['target_uart_id']
        des_base = vm_info.vuart.v1_vuart[int(src_t_vm_i)]['base']
        des_t_vm_i = vm_info.vuart.v1_vuart[int(src_t_vm_i)]['target_vm_id']
        des_t_vuart_i = vm_info.vuart.v1_vuart[int(src_t_vm_i)]['target_uart_id']

        if int(des_t_vm_i) == i and int(des_t_vuart_i) == 1 and des_t_vuart_i == src_t_vuart_i:
            map_dic[i] = True
        else:
            map_dic[i] = False

    return map_dic

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
    vuart_enable = vuart_map_enable(vm_info)
    # vuart1:   {vmid:target_vmid}
    print("\t\t.vuart[1] = {", file=config)
    print("\t\t\t.type = {0},".format(vm_info.vuart.v1_vuart[i]['type']), file=config)
    if vm_info.vuart.v1_vuart[i]['base'] != "INVALID_COM_BASE" and vuart_enable[i]:
        print("\t\t\t.addr.port_base = {0},".format(
            vm_info.vuart.v1_vuart[i]['base']), file=config)
    else:
        print("\t\t\t.addr.port_base = INVALID_COM_BASE,", file=config)

    if vuart1_vmid_dic and i in vuart1_vmid_dic.keys():
        if "SOS_VM" == scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            if vm_info.vuart.v1_vuart[i]['base'] != "INVALID_COM_BASE" and vuart_enable[i]:
                print("\t\t\t.irq = SOS_COM2_IRQ,", file=config)
        else:
            if vm_info.vuart.v1_vuart[i]['base'] != "INVALID_COM_BASE" and vuart_enable[i]:
                print("\t\t\t.irq = COM2_IRQ,", file=config)

        if vm_info.vuart.v1_vuart[i]['base'] != "INVALID_COM_BASE" and vuart_enable[i]:
            print("\t\t\t.t_vuart.vm_id = {0}U,".format(
                vm_info.vuart.v1_vuart[i]['target_vm_id']), file=config)
            print("\t\t\t.t_vuart.vuart_id = {0}U,".format(
                vm_info.vuart.v1_vuart[i]['target_uart_id']), file=config)

def vuart_output(vm_type, i, vm_info, config):
    """
    This is generate vuart setting
    :param i: vm id number
    :param vm_info: it is the class which contain all user setting information
    :param config: it is the pointer which file write to
    :return: None
    """
    vuart1_vmid_dic = {}
    vuart1_vmid_dic = scenario_cfg_lib.get_vuart1_vmid(vm_info.vuart.v1_vuart)

    vuart0_output(i, vm_type, vm_info, config)
    vuart1_output(i, vm_type, vuart1_vmid_dic, vm_info, config)
    print("\t\t},", file=config)


def is_need_epc(epc_section, i, config):
    """
    Check if it is need epc section
    :param epc_section: struct epc_scectoin conatins base/size
    :param i: the index of vm id
    :param config: it is file pointer to store the information
    :return: None
    """
    # SOS_VM have not set epc section
    if i not in common.VM_TYPES.keys():
        return
    vm_type = list(common.VM_TYPES.values())[i]
    if "SOS_VM" == scenario_cfg_lib.VM_DB[vm_type]['load_type']:
        return

    if epc_section.base[i] == '0' and epc_section.size[i] == '0':
        return
    else:
        print("\t\t.epc= {", file=config)
        print('\t\t\t.base = {0},'.format(epc_section.base[i]), file=config)
        print('\t\t\t.size = {0},'.format(epc_section.size[i]), file=config)
        print("\t\t},", file=config)


def cpu_affinity_output(vm_info, i, config):
    """
    Output the pcpu affinity bitmap
    :param vminfo: the data structure have all the xml items values
    :param i: the index of vm id
    :param config: file pointor to store the information
    """

    if "SOS_VM" == common.VM_TYPES[i]:
        print("\t\t.cpu_affinity = SOS_VM_CONFIG_CPU_AFFINITY,", file=config)
    else:
        print("\t\t.cpu_affinity = VM{}_CONFIG_CPU_AFFINITY,".format(i), file=config)


def clos_output(scenario_items, i, config):
    """
    This is generate clos setting
    :param scenario_items: it is the class which contain all user setting information
    :param i: vm id number
    :param config: it is the pointer which file write to
    :return: None
    """
    hv_info = scenario_items['hv']

    print("#ifdef CONFIG_RDT_ENABLED", file=config)
    print("\t\t.clos = VM{}_VCPU_CLOS,".format(i), file=config)
    print("#endif", file=config)

def get_guest_flag(flags):
    """
    This is get flag index list
    :param flags:
    :return: flag index list in GUEST_FLAGS
    """
    err_dic = {}
    flag_str = ''
    for flag in flags:
        if flags.count(flag) >= 2:
            return (err_dic, flag)

    for i in range(len(flags)):
        if i == 0:
            if len(flags) == 1:
                # get the guest flag 0UL
                if flags[0] == '0UL':
                    return (err_dic, flags[0])
                flag_str = "{0}".format(flags[0])
            else:
                flag_str = "({0}".format(flags[0])

        else:
            # flags lenght already minus 1
            if i == len(flags) - 1:
                flag_str = flag_str + " | {0})".format(flags[i])
            else:
                flag_str = flag_str + " | {0}".format(flags[i])

    return (err_dic, flag_str)


def gen_source_header(config):
    """
    This is the common header for vm_configuration.c
    :param config: it is the pointer which file write to
    :return: None
    """
    print("{0}".format(C_HEADER), file=config)


def gen_sos_vm(vm_type, vm_i, scenario_items, config):

    vm_info = scenario_items['vm']
    (err_dic, sos_guest_flags) = get_guest_flag(vm_info.guest_flags[vm_i])
    if err_dic:
        return err_dic

    print("\t{{\t/* VM{} */".format(vm_i), file=config)
    print("\t\tCONFIG_SOS_VM,", file=config)
    print('\t\t.name = "{0}",'.format(vm_info.name[vm_i]), file=config)
    print("", file=config)
    print("\t\t/* Allow SOS to reboot the host since " +
          "there is supposed to be the highest severity guest */", file=config)
    if sos_guest_flags:
        print("\t\t.guest_flags = {0},".format(sos_guest_flags), file=config)
    clos_output(scenario_items, vm_i, config)
    cpu_affinity_output(vm_info, vm_i, config)
    print("\t\t.memory = {", file=config)
    print("\t\t\t.start_hpa = {}UL,".format(vm_info.mem_info.mem_start_hpa[vm_i]), file=config)
    print("\t\t\t.size = {0},".format("CONFIG_SOS_RAM_SIZE"), file=config)
    print("\t\t},", file=config)
    print("\t\t.os_config = {", file=config)
    print('\t\t\t.name = "{0}",'.format(vm_info.os_cfg.kern_name[vm_i]), file=config)
    print('\t\t\t.kernel_type = {0},'.format(vm_info.os_cfg.kern_type[vm_i]), file=config)
    print('\t\t\t.kernel_mod_tag = "{0}",'.format(
        vm_info.os_cfg.kern_mod[vm_i]), file=config)
    print('\t\t\t.bootargs = {0},'.format(vm_info.os_cfg.kern_args[vm_i]), file=config)
    if (vm_info.os_cfg.ramdisk_mod[vm_i].strip()):
        print('\t\t\t.ramdisk_mod_tag = "{0}",'.format(
            vm_info.os_cfg.ramdisk_mod[vm_i]), file=config)
    print("\t\t},", file=config)
    # VUART
    err_dic = vuart_output(vm_type, vm_i, vm_info, config)
    if err_dic:
        return err_dic
    sos_dev_num = scenario_cfg_lib.get_pci_dev_num_per_vm()[vm_i]
    print("\t\t.pci_dev_num = {}U,".format(sos_dev_num), file=config)
    print("\t\t.pci_devs = sos_pci_devs,", file=config)

    print("\t},", file=config)


def gen_pre_launch_vm(vm_type, vm_i, scenario_items, config):

    vm_info = scenario_items['vm']
    # guest flags
    (err_dic, guest_flags) = get_guest_flag(vm_info.guest_flags[vm_i])
    if err_dic:
        return err_dic

    pre_vm_type = get_pre_vm_type(vm_type, vm_i)
    print("\t{{\t/* VM{} */".format(vm_i), file=config)
    print("\t\t{},".format(pre_vm_type), file=config)
    print('\t\t.name = "{0}",'.format(vm_info.name[vm_i]), file=config)
    cpu_affinity_output(vm_info, vm_i, config)
    if guest_flags:
        print("\t\t.guest_flags = {0},".format(guest_flags), file=config)
    clos_output(scenario_items, vm_i, config)
    print("\t\t.memory = {", file=config)
    print("\t\t\t.start_hpa = VM{0}_CONFIG_MEM_START_HPA,".format(vm_i), file=config)
    print("\t\t\t.size = VM{0}_CONFIG_MEM_SIZE,".format(vm_i), file=config)
    print("\t\t\t.start_hpa2 = VM{0}_CONFIG_MEM_START_HPA2,".format(vm_i), file=config)
    print("\t\t\t.size_hpa2 = VM{0}_CONFIG_MEM_SIZE_HPA2,".format(vm_i), file=config)
    print("\t\t},", file=config)
    is_need_epc(vm_info.epc_section, vm_i, config)
    print("\t\t.os_config = {", file=config)
    print('\t\t\t.name = "{0}",'.format(vm_info.os_cfg.kern_name[vm_i]), file=config)
    print("\t\t\t.kernel_type = {0},".format(
        vm_info.os_cfg.kern_type[vm_i]), file=config)
    print('\t\t\t.kernel_mod_tag = "{0}",'.format(
        vm_info.os_cfg.kern_mod[vm_i]), file=config)
    if (vm_info.os_cfg.ramdisk_mod[vm_i].strip()):
        print('\t\t\t.ramdisk_mod_tag = "{0}",'.format(
            vm_info.os_cfg.ramdisk_mod[vm_i]), file=config)

    if vm_i in vm_info.os_cfg.kern_load_addr.keys() and vm_info.os_cfg.kern_entry_addr[vm_i]:
        print("\t\t\t.kernel_load_addr = {0},".format(vm_info.os_cfg.kern_load_addr[vm_i]), file=config)
    if vm_i in vm_info.os_cfg.kern_entry_addr.keys() and vm_info.os_cfg.kern_entry_addr[vm_i]:
        print("\t\t\t.kernel_entry_addr = {0},".format(vm_info.os_cfg.kern_entry_addr[vm_i]), file=config)

    if vm_i in vm_info.os_cfg.kern_args.keys() and vm_info.os_cfg.kern_args[vm_i]:
        print("\t\t\t.bootargs = VM{0}_BOOT_ARGS,".format(vm_i), file=config)

    print("\t\t},", file=config)
    print("\t\t.acpi_config = {", file=config)
    print('\t\t\t.acpi_mod_tag = "ACPI_VM{}",'.format(vm_i), file=config)
    print("\t\t},", file=config)
    # VUART
    err_dic = vuart_output(vm_type, vm_i, vm_info, config)
    if err_dic:
        return err_dic

    if scenario_cfg_lib.get_pci_dev_num_per_vm()[vm_i]:
        print("\t\t.pci_dev_num = VM{}_CONFIG_PCI_DEV_NUM,".format(vm_i), file=config)
        print("\t\t.pci_devs = vm{}_pci_devs,".format(vm_i), file=config)

    if vm_i == 0:
        print("#ifdef VM0_PASSTHROUGH_TPM", file=config)
        print("\t\t.pt_tpm2 = true,", file=config)
        print("\t\t.mmiodevs[0] = {", file=config)
        print("\t\t\t.base_gpa = VM0_TPM_BUFFER_BASE_ADDR_GPA,", file=config)
        print("\t\t\t.base_hpa = VM0_TPM_BUFFER_BASE_ADDR,", file=config)
        print("\t\t\t.size = VM0_TPM_BUFFER_SIZE,", file=config)
        print("\t\t},", file=config)
        print("#endif", file=config)

    if vm_i == 0:
        print("#ifdef P2SB_BAR_ADDR", file=config)
        print("\t\t.pt_p2sb_bar = true,", file=config)
        print("\t\t.mmiodevs[0] = {", file=config)
        print("\t\t\t.base_gpa = P2SB_BAR_ADDR_GPA,", file=config)
        print("\t\t\t.base_hpa = P2SB_BAR_ADDR,", file=config)
        print("\t\t\t.size = P2SB_BAR_SIZE,", file=config)
        print("\t\t},", file=config)
        print("#endif", file=config)

    if vm_i == 0:
        print("\t\t.pt_intx_num = VM0_PT_INTX_NUM,", file=config)
        print("\t\t.pt_intx = &vm0_pt_intx[0U],", file=config)

    print("\t},", file=config)


def gen_post_launch_vm(vm_type, vm_i, scenario_items, config):

    vm_info = scenario_items['vm']
    post_vm_type = get_post_vm_type(vm_type, vm_i)
    print("\t{{\t/* VM{} */".format(vm_i), file=config)
    print("\t\t{},".format(post_vm_type), file=config)
    clos_output(scenario_items, vm_i, config)
    if scenario_cfg_lib.get_pci_dev_num_per_vm()[vm_i]:
        print("\t\t/* The PCI device configuration is only for in-hypervisor vPCI devices. */", file=config)
        print("\t\t.pci_dev_num = VM{}_CONFIG_PCI_DEV_NUM,".format(vm_i), file=config)
        print("\t\t.pci_devs = vm{}_pci_devs,".format(vm_i), file=config)
    cpu_affinity_output(vm_info, vm_i, config)
    is_need_epc(vm_info.epc_section, vm_i, config)
    # VUART
    err_dic = vuart_output(vm_type, vm_i, vm_info, config)
    if err_dic:
        return err_dic

    print("\t},", file=config)

def declare_pci_devs(vm_info, config):

    for vm_i,vm_type in common.VM_TYPES.items():
        if vm_type == "SOS_VM":
            print("extern struct acrn_vm_pci_dev_config " +
                "sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM];", file=config)
            continue
        if scenario_cfg_lib.get_pci_dev_num_per_vm()[vm_i]:
            print("extern struct acrn_vm_pci_dev_config " +
                "vm{}_pci_devs[VM{}_CONFIG_PCI_DEV_NUM];".format(vm_i, vm_i), file=config)
    print("", file=config)

def generate_file(scenario_items, config):
    """
    Start to generate vm_configurations.c
    :param config: it is a file pointer of board information for writing to
    """
    err_dic = {}
    vm_info = scenario_items['vm']
    gen_source_header(config)

    declare_pci_devs(vm_info, config)

    if (board_cfg_lib.is_matched_board(("ehl-crb-b"))
        and vm_info.pt_intx_info.phys_gsi.get(0) is not None
        and len(vm_info.pt_intx_info.phys_gsi[0]) > 0):
        print("extern struct pt_intx_config vm0_pt_intx[{}U];".format(len(vm_info.pt_intx_info.phys_gsi[0])), file=config)
    else:
        print("extern struct pt_intx_config vm0_pt_intx[1U];", file=config)

    print("", file=config)

    print("struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {", file=config)
    for vm_i, vm_type in common.VM_TYPES.items():

        if "SOS_VM" == scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            gen_sos_vm(vm_type, vm_i, scenario_items, config)
        elif "PRE_LAUNCHED_VM" == scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            gen_pre_launch_vm(vm_type, vm_i, scenario_items, config)
        elif "POST_LAUNCHED_VM" == scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            gen_post_launch_vm(vm_type, vm_i, scenario_items, config)

    print("};", file=config)
    return err_dic
