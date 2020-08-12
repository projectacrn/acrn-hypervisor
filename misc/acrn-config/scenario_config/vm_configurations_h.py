# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common
import scenario_cfg_lib
import board_cfg_lib

VM_HEADER_DEFINE = scenario_cfg_lib.HEADER_LICENSE + r"""
#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H
"""
VM_END_DEFINE = r"""#endif /* VM_CONFIGURATIONS_H */"""


def gen_common_header(config):
    """
    This is common header for vm_configuration.h
    :param config: it is the pointer which file write to
    :return: None
    """
    print("{0}".format(VM_HEADER_DEFINE), file=config)


def cpu_affinity_output(vm_info, i, config):
    """
    Output the macro pcpu affinity bitmap
    :param vm_info: the data structure have all the xml items values
    :param i: the index of vm id
    :param config: file pointor to store the information
    """

    cpu_bits = vm_info.get_cpu_bitmap(i)
    if "SOS_VM" == common.VM_TYPES[i]:
        print("", file=config)
        print("#define SOS_VM_CONFIG_CPU_AFFINITY      {0}".format(
            cpu_bits['cpu_map']), file=config)
    else:
        print("#define VM{0}_CONFIG_CPU_AFFINITY         {1}".format(
            i, cpu_bits['cpu_map']), file=config)

def clos_config_output(scenario_items, i, config):
    """
    Output the macro vcpu affinity
    :param scenario_items: the data structure have all the xml items values
    :param i: the index of vm id
    :param config: file pointor to store the information
    """
    vm_info = scenario_items['vm']
    hv_info = scenario_items['hv']

    if board_cfg_lib.is_rdt_supported() and hv_info.features.rdt_enabled == 'y':
        clos_config = vm_info.get_clos_bitmap(i)
        print("#define VM{0}_VCPU_CLOS\t\t\t{1}".format(i, clos_config['clos_map']), file=config)

def scenario_vm_num(scenario_items, config):

    print("", file=config)
    print("/* SOS_VM_NUM can only be 0U or 1U;", file=config)
    print(" * When SOS_VM_NUM is 0U, MAX_POST_VM_NUM must be 0U too;", file=config)
    print(" * MAX_POST_VM_NUM must be bigger than CONFIG_MAX_KATA_VM_NUM;", file=config)
    print(" */", file=config)

    load_type_cnt = scenario_items['vm'].load_order_cnt
    print("#define PRE_VM_NUM\t\t\t{}U".format(load_type_cnt.pre_vm), file=config)
    print("#define SOS_VM_NUM\t\t\t{}U".format(load_type_cnt.sos_vm), file=config)
    print("#define MAX_POST_VM_NUM\t\t\t{}U".format(load_type_cnt.post_vm), file=config)
    print("#define CONFIG_MAX_KATA_VM_NUM\t\t{}U".format(scenario_cfg_lib.KATA_VM_COUNT), file=config)


def gen_pre_launch_vm(scenario_items, config):

    vm_info = scenario_items['vm']
    vm_i = 0
    for vm_type in common.VM_TYPES.values():
        if "PRE_LAUNCHED_VM" != scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            vm_i += 1
            continue

        cpu_bits = vm_info.get_cpu_bitmap(vm_i)
        cpu_affinity_output(vm_info, vm_i, config)
        clos_config_output(scenario_items, vm_i, config)
        print("#define VM{0}_CONFIG_MEM_START_HPA        {1}UL".format(
            vm_i, vm_info.mem_info.mem_start_hpa[vm_i]), file=config)
        print("#define VM{0}_CONFIG_MEM_SIZE             {1}UL".format(
            vm_i, vm_info.mem_info.mem_size[vm_i]), file=config)
        if vm_info.mem_info.mem_start_hpa2[vm_i] not in (None, ''):
            print("#define VM{0}_CONFIG_MEM_START_HPA2       {1}UL".format(
                vm_i, vm_info.mem_info.mem_start_hpa2[vm_i]), file=config)
            print("#define VM{0}_CONFIG_MEM_SIZE_HPA2        {1}UL".format(
                vm_i, vm_info.mem_info.mem_size_hpa2[vm_i]), file=config)

        print("#define VM{}_CONFIG_PCI_DEV_NUM          {}U".format(vm_i, vm_info.cfg_pci.pci_dev_num[vm_i]), file=config)
        print("", file=config)
        vm_i += 1


def gen_post_launch_header(scenario_items, config):
    vm_i = 0
    vm_info = scenario_items['vm']
    is_post_vm_available = False
    for vm_type in common.VM_TYPES.values():
        if "POST_LAUNCHED_VM" != scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            vm_i += 1
            continue

        is_post_vm_available = True
        cpu_affinity_output(vm_info, vm_i, config)
        clos_config_output(scenario_items, vm_i, config)
        vm_i += 1

    if is_post_vm_available:
        print("", file=config)


def gen_sos_header(scenario_items, config):

    if 'SOS_VM' not in common.VM_TYPES.values():
        return

    for vm_i,vm_type in common.VM_TYPES.items():
        if vm_type == 'SOS_VM':
            print("/* SOS_VM == VM{0} */".format(vm_i), file=config)

    print("#define SOS_VM_BOOTARGS\t\t\tSOS_ROOTFS\t\\", file=config)
    print("\t\t\t\t\tSOS_CONSOLE\t\\", file=config)
    print("\t\t\t\t\tSOS_IDLE\t\\", file=config)
    print("\t\t\t\t\tSOS_BOOTARGS_DIFF", file=config)

    vm_info = scenario_items['vm']
    for vm_i,vm_type in common.VM_TYPES.items():
        if vm_type == 'SOS_VM':
            cpu_affinity_output(vm_info, vm_i, config)
            clos_config_output(scenario_items, vm_i, config)
    print("", file=config)


def gen_header_file(scenario_items, config):

    gen_pre_launch_vm(scenario_items, config)
    gen_sos_header(scenario_items, config)
    gen_post_launch_header(scenario_items, config)


def get_dm_owned_guest_flag_mask(vm_info, config):

    print("", file=config)
    if "SOS_VM" not in common.VM_TYPES.values():
        print("#define DM_OWNED_GUEST_FLAG_MASK        0UL", file=config)
    else:
        print("/* Bits mask of guest flags that can be programmed by device model." +
              " Other bits are set by hypervisor only */", file=config)
        print("#define DM_OWNED_GUEST_FLAG_MASK        " +
              "(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH | \\\n" +
              "\t\t\t\t\t\tGUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)", file=config)
    print("", file=config)


def generate_file(scenario_items, config):
    """
    Start to generate vm_configurations.h
    :param scenario_items: it is the class which contain all user setting information
    :param config: it is a file pointer of board information for writing to
    """
    vm_info = scenario_items['vm']
    gen_common_header(config)

    print("#include <misc_cfg.h>", file=config)
    print("#include <pci_devices.h>", file=config)
    scenario_vm_num(scenario_items, config)
    get_dm_owned_guest_flag_mask(vm_info, config)

    gen_header_file(scenario_items, config)
    print("{0}".format(VM_END_DEFINE), file=config)
