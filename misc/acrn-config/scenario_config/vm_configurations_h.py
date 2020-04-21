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
    Output the macro vcpu affinity
    :param vm_info: the data structure have all the xml items values
    :param i: the index of vm id
    :param config: file pointor to store the information
    """
    if "SOS_VM" == scenario_cfg_lib.VM_DB[vm_info.load_vm[i]]['load_type']:
        return

    cpu_bits = vm_info.get_cpu_bitmap(i)
    print("#define VM{0}_CONFIG_VCPU_AFFINITY\t\t{1}".format(
        i, cpu_bits['cpu_map']), file=config)

def clos_config_output(vm_info, i, config):
    """
    Output the macro vcpu affinity
    :param vm_info: the data structure have all the xml items values
    :param i: the index of vm id
    :param config: file pointor to store the information
    """
    (rdt_res, rdt_res_clos_max, _) = board_cfg_lib.clos_info_parser(common.BOARD_INFO_FILE)
    if len(rdt_res_clos_max) != 0:
        common_clos_max = min(rdt_res_clos_max)
    else:
        common_clos_max = 0

    if common_clos_max == 0:
        return

    clos_config = vm_info.get_clos_bitmap(i)
    print("#define VM{0}_VCPU_CLOS\t\t{1}".format(i, clos_config['clos_map']), file=config)

def scenario_vm_num(scenario_items, config):

    load_type_cnt = scenario_items['vm'].load_order_cnt
    kata_vm_num = scenario_items['hv'].cap.max_kata_vm_num
    print("#define PRE_VM_NUM\t\t{}U".format(load_type_cnt.pre_vm), file=config)
    print("#define SOS_VM_NUM\t\t{}U".format(load_type_cnt.sos_vm), file=config)
    print("#define MAX_POST_VM_NUM\t\t{}U".format(load_type_cnt.post_vm), file=config)
    print("#define CONFIG_MAX_KATA_VM_NUM\t\t{}U".format(kata_vm_num), file=config)


def gen_pre_launch_vm(vm_info, config):

    vm_i = 0
    for vm_type in common.VM_TYPES.values():
        if "PRE_LAUNCHED_VM" != scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            vm_i += 1
            continue

        cpu_bits = vm_info.get_cpu_bitmap(vm_i)
        cpu_affinity_output(vm_info, vm_i, config)
        clos_config_output(vm_info, vm_i, config)
        print("#define VM{0}_CONFIG_MEM_START_HPA\t\t{1}UL".format(
            vm_i, vm_info.mem_info.mem_start_hpa[vm_i]), file=config)
        print("#define VM{0}_CONFIG_MEM_SIZE\t\t\t{1}UL".format(
            vm_i, vm_info.mem_info.mem_size[vm_i]), file=config)
        if vm_info.mem_info.mem_start_hpa2[vm_i] not in (None, ''):
            print("#define VM{0}_CONFIG_MEM_START_HPA2\t\t{1}UL".format(
                vm_i, vm_info.mem_info.mem_start_hpa2[vm_i]), file=config)
            print("#define VM{0}_CONFIG_MEM_SIZE_HPA2\t\t{1}UL".format(
                vm_i, vm_info.mem_info.mem_size_hpa2[vm_i]), file=config)
        if vm_info.os_cfg.kern_root_dev:
            print('#define VM{0}_CONFIG_OS_BOOTARG_ROOT\t\t"root={1} "'.format(
                vm_i, vm_info.os_cfg.kern_root_dev[vm_i]), file=config)

        print('#define VM{0}_CONFIG_OS_BOOTARG_MAXCPUS\t\t"maxcpus={1} "'.format(vm_i, cpu_bits['cpu_num']), file=config)
        if vm_info.os_cfg.kern_console:
            print('#define VM{0}_CONFIG_OS_BOOTARG_CONSOLE\t\t"console={1} "'.format(vm_i, vm_info.os_cfg.kern_console.strip('/dev/')), file=config)
        print("", file=config)
        vm_i += 1


def gen_post_launch_header(vm_info, config):
    vm_i = 0
    for vm_type in common.VM_TYPES.values():
        if "POST_LAUNCHED_VM" != scenario_cfg_lib.VM_DB[vm_type]['load_type']:
            vm_i += 1
            continue
        cpu_affinity_output(vm_info, vm_i, config)
        clos_config_output(vm_info, vm_i, config)
        vm_i += 1

def gen_sos_header(vm_info, config):

    if 'SOS_VM' not in common.VM_TYPES.values():
        return
    print("", file=config)
    print("#define SOS_VM_BOOTARGS\t\t\tSOS_ROOTFS\t\\", file=config)
    print("\t\t\t\t\tSOS_CONSOLE\t\\", file=config)
    print("\t\t\t\t\tSOS_IDLE\t\\", file=config)
    print("\t\t\t\t\tSOS_BOOTARGS_DIFF", file=config)


def gen_header_file(vm_info, config):

    gen_post_launch_header(vm_info, config)
    gen_pre_launch_vm(vm_info, config)
    gen_sos_header(vm_info, config)


def get_dm_owned_guest_flag_mask(vm_info, config):

    if "SOS_VM" not in common.VM_TYPES.values():
        print("#define DM_OWNED_GUEST_FLAG_MASK\t0UL", file=config)
    else:
        print("/* Bits mask of guest flags that can be programmed by device model." +
              " Other bits are set by hypervisor only */", file=config)
        print("#define DM_OWNED_GUEST_FLAG_MASK\t" +
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

    print("#include <misc_cfg.h>\n", file=config)
    for vm_i,pci_dev_num in vm_info.cfg_pci.pci_dev_num.items():
        if pci_dev_num >= 2:
            print("#include <pci_devices.h>", file=config)
            break
    get_dm_owned_guest_flag_mask(vm_info, config)
    scenario_vm_num(scenario_items, config)
    print("", file=config)

    gen_header_file(vm_info, config)
    print("", file=config)
    print("{0}".format(VM_END_DEFINE), file=config)
