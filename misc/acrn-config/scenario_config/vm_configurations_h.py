# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

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
    if vm_info.load_order[i] == "SOS_VM":
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
    (rdt_res, rdt_res_clos_max, _) = board_cfg_lib.clos_info_parser(scenario_cfg_lib.BOARD_INFO_FILE)
    if len(rdt_res_clos_max) != 0:
        common_clos_max = min(rdt_res_clos_max)
    else:
        common_clos_max = 0

    if common_clos_max == 0:
        return

    clos_config = vm_info.get_clos_bitmap(i)
    print("#define VM{0}_VCPU_CLOS\t\t{1}".format(i, clos_config['clos_map']), file=config)

def scenario_vm_num(load_type_cnt, config):

    print("#define PRE_VM_NUM\t\t{}U".format(load_type_cnt.pre_vm), file=config)
    print("#define SOS_VM_NUM\t\t{}U".format(load_type_cnt.sos_vm), file=config)
    print("#define MAX_POST_VM_NUM\t\t{}U".format(load_type_cnt.post_vm), file=config)

def gen_sdc_header(vm_info, config):
    """
    Generate vm_configuration.h of sdc scenario
    :param config: it is the pointer which file write to
    :return: None
    """
    gen_common_header(config)
    print("#include <misc_cfg.h>\n", file=config)

    scenario_vm_num(vm_info.load_order_cnt, config)
    print("", file=config)
    print("/* Bits mask of guest flags that can be programmed by device model." +
          " Other bits are set by hypervisor only */", file=config)
    print("#define DM_OWNED_GUEST_FLAG_MASK\t" +
          "(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH | \\\n" +
          "\t\t\t\t\t\tGUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)", file=config)

    print("", file=config)
    print("#define SOS_VM_BOOTARGS\t\t\tSOS_ROOTFS\t\\", file=config)
    print("\t\t\t\t\tSOS_CONSOLE\t\\", file=config)
    print("\t\t\t\t\tSOS_BOOTARGS_DIFF", file=config)
    print("", file=config)

    # POST LAUNCHED VM
    if scenario_cfg_lib.KATA_VM_COUNT == 1:
        print("#if CONFIG_MAX_KATA_VM_NUM > 0", file=config)
        # Set VM1 vcpu
        cpu_affinity_output(vm_info, 1, config)
        clos_config_output(vm_info, 1, config)
        # KATA VM
        cpu_affinity_output(vm_info, 2, config)
        clos_config_output(vm_info, 2, config)
        #else:
        print("#else", file=config)
        # Only two VMs in SDC config, setup vcpu affinity for VM1
        cpu_affinity_output(vm_info, 1, config)
        clos_config_output(vm_info, 1, config)
        print("#endif", file=config)
    else:
        cpu_affinity_output(vm_info, 1, config)
        clos_config_output(vm_info, 1, config)
    print("", file=config)
    print("{0}".format(VM_END_DEFINE), file=config)


def gen_sdc2_header(vm_info, config):
    """
    Generate vm_configuration.h of sdc2 scenario
    :param config: it is the pointer which file write to
    :return: None
    """
    gen_common_header(config)
    print("#include <misc_cfg.h>\n", file=config)
    scenario_vm_num(vm_info.load_order_cnt, config)
    print("", file=config)
    print("/* Bits mask of guest flags that can be programmed by device model." +
          " Other bits are set by hypervisor only */", file=config)
    print("#define DM_OWNED_GUEST_FLAG_MASK\t" +
          "(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH | \\\n" +
          "\t\t\t\t\t\tGUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)", file=config)

    print("", file=config)
    print("#define SOS_VM_BOOTARGS\t\t\tSOS_ROOTFS\t\\", file=config)
    print("\t\t\t\t\tSOS_CONSOLE\t\\", file=config)
    print("\t\t\t\t\tSOS_BOOTARGS_DIFF", file=config)

    print("", file=config)
    for i in range(scenario_cfg_lib.VM_COUNT):
        cpu_affinity_output(vm_info, i, config)
        clos_config_output(vm_info, i, config)
    print("", file=config)
    print("{0}".format(VM_END_DEFINE), file=config)


def logic_max_vm_num(vm_info, config):
    """
    This is logical max vm number comment
    :param config: it is the pointer which file write to
    :return: None
    """
    print("", file=config)
    scenario_vm_num(vm_info.load_order_cnt, config)
    print("", file=config)
    print("/* The VM CONFIGs like:", file=config)
    print(" *\tVMX_CONFIG_VCPU_AFFINITY", file=config)
    print(" *\tVMX_CONFIG_MEM_START_HPA", file=config)
    print(" *\tVMX_CONFIG_MEM_SIZE", file=config)
    print(" *\tVMX_CONFIG_MEM_START_HPA2", file=config)
    print(" *\tVMX_CONFIG_MEM_SIZE_HPA2", file=config)
    print(" *\tVMX_CONFIG_OS_BOOTARG_ROOT", file=config)
    print(" *\tVMX_CONFIG_OS_BOOTARG_MAX_CPUS", file=config)
    print(" *\tVMX_CONFIG_OS_BOOTARG_CONSOLE", file=config)
    print(" * might be different on your board, please modify them per your needs.", file=config)
    print(" */", file=config)
    print("", file=config)


def gen_logical_partition_header(vm_info, config):
    """
    Generate vm_configuration.h of logical_partition scenario
    :param config: it is the pointer which file write to
    :return: None
    """
    scenario_cfg_lib.vms_count = scenario_cfg_lib.VM_COUNT
    gen_common_header(config)
    # map all the needed pci sub class
    print("#include <pci_devices.h>", file=config)
    print("#include <misc_cfg.h>", file=config)
    print("", file=config)
    print("/* Bits mask of guest flags that can be programmed by device model." +
          " Other bits are set by hypervisor only */", file=config)
    print("#define DM_OWNED_GUEST_FLAG_MASK\t0UL", file=config)

    logic_max_vm_num(vm_info, config)

    for i in range(scenario_cfg_lib.VM_COUNT):

        cpu_bits = vm_info.get_cpu_bitmap(i)
        cpu_affinity_output(vm_info, i, config)
        clos_config_output(vm_info, i, config)
        print("#define VM{0}_CONFIG_MEM_START_HPA\t\t{1}UL".format(
            i, vm_info.mem_info.mem_start_hpa[i]), file=config)
        print("#define VM{0}_CONFIG_MEM_SIZE\t\t\t{1}UL".format(
            i, vm_info.mem_info.mem_size[i]), file=config)
        print("#define VM{0}_CONFIG_MEM_START_HPA2\t\t{1}UL".format(
            i, vm_info.mem_info.mem_start_hpa2[i]), file=config)
        print("#define VM{0}_CONFIG_MEM_SIZE_HPA2\t\t{1}UL".format(
            i, vm_info.mem_info.mem_size_hpa2[i]), file=config)
        print('#define VM{0}_CONFIG_OS_BOOTARG_ROOT\t\t"root={1} "'.format(
            i, vm_info.os_cfg.kern_root_dev[i]), file=config)
        print('#define VM{0}_CONFIG_OS_BOOTARG_MAXCPUS\t\t"maxcpus={1} "'.format(
            i, cpu_bits['cpu_num']), file=config)
        print('#define VM{0}_CONFIG_OS_BOOTARG_CONSOLE\t\t"console={1} "'.format(
            i, vm_info.os_cfg.kern_console[i].strip('/dev/')), file=config)
        print("", file=config)

    print('/* VM pass-through devices assign policy:', file=config)
    print(' * VM0: one Mass Storage controller, one Network controller;', file=config)
    print(' * VM1: one Mass Storage controller, one Network controller' +
          '(if a secondary Network controller class device exist);', file=config)
    print(' */', file=config)
    print('#define VM0_STORAGE_CONTROLLER\t\t\tSATA_CONTROLLER_0', file=config)
    print('#define VM0_NETWORK_CONTROLLER\t\t\tETHERNET_CONTROLLER_0', file=config)
    print('#define VM0_CONFIG_PCI_DEV_NUM\t\t\t3U', file=config)
    print('', file=config)
    print('#define VM1_STORAGE_CONTROLLER\t\t\tUSB_CONTROLLER_0', file=config)
    print('#if defined(ETHERNET_CONTROLLER_1)', file=config)
    print('/* if a secondary Ethernet controller subclass exist, assign to VM1 */', file=config)
    print('#define VM1_NETWORK_CONTROLLER\t\t\tETHERNET_CONTROLLER_1', file=config)
    print('#elif defined(NETWORK_CONTROLLER_0)', file=config)
    print('/* if a Network controller subclass exist' +
          '(usually it is a wireless network card), assign to VM1 */', file=config)
    print('#define VM1_NETWORK_CONTROLLER\t\t\tNETWORK_CONTROLLER_0', file=config)
    print('#endif', file=config)
    print('', file=config)
    print('#if defined(VM1_NETWORK_CONTROLLER)', file=config)
    print('#define VM1_CONFIG_PCI_DEV_NUM\t\t\t3U', file=config)
    print('#else', file=config)
    print('/* no network controller could be assigned to VM1 */', file=config)
    print('#define VM1_CONFIG_PCI_DEV_NUM\t\t\t2U', file=config)
    print('#endif', file=config)
    print("", file=config)
    print("{0}".format(VM_END_DEFINE), file=config)


def gen_industry_header(vm_info, config):
    """
    Generate vm_configuration.h of industry scenario
    :param config: it is the pointer which file write to
    :return: None
    """
    gen_common_header(config)
    print("#include <misc_cfg.h>", file=config)
    print("", file=config)
    scenario_vm_num(vm_info.load_order_cnt, config)
    print("", file=config)
    print("/* Bits mask of guest flags that can be programmed by device model." +
          " Other bits are set by hypervisor only */", file=config)
    print("#define DM_OWNED_GUEST_FLAG_MASK\t(GUEST_FLAG_SECURE_WORLD_ENABLED | " +
          "GUEST_FLAG_LAPIC_PASSTHROUGH | \\", file=config)
    print("\t\t\t\t\t\tGUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)", file=config)
    print("", file=config)
    print("#define SOS_VM_BOOTARGS\t\t\tSOS_ROOTFS\t\\", file=config)
    print("\t\t\t\t\tSOS_CONSOLE\t\\", file=config)
    print("\t\t\t\t\tSOS_BOOTARGS_DIFF", file=config)
    print("", file=config)
    for i in range(scenario_cfg_lib.VM_COUNT):
        cpu_affinity_output(vm_info, i, config)
        clos_config_output(vm_info, i, config)
    print("", file=config)
    print("{0}".format(VM_END_DEFINE), file=config)


def gen_hybrid_header(vm_info, config):
    """
    Generate vm_configuration.h of hybrid scenario
    :param vm_info: it is the class which contain all user setting information
    :param config: it is the pointer which file write to
    :return: None
    """
    gen_common_header(config)
    print("#include <misc_cfg.h>\n", file=config)
    print("/* Bits mask of guest flags that can be programmed by device model." +
          " Other bits are set by hypervisor only */", file=config)
    print("#define DM_OWNED_GUEST_FLAG_MASK\t" +
          "(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH | \\\n" +
          "\t\t\t\t\t\tGUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)", file=config)

    print("", file=config)
    scenario_vm_num(vm_info.load_order_cnt, config)
    print("", file=config)
    for i in range(scenario_cfg_lib.VM_COUNT):
        cpu_affinity_output(vm_info, i, config)
        clos_config_output(vm_info, i, config)

    print("#define VM0_CONFIG_MEM_START_HPA\t{0}UL".format(
        vm_info.mem_info.mem_start_hpa[0]), file=config)
    print("#define VM0_CONFIG_MEM_SIZE\t\t{0}UL".format(vm_info.mem_info.mem_size[0]), file=config)
    print("#define VM0_CONFIG_MEM_START_HPA2\t{0}UL".format(
        vm_info.mem_info.mem_start_hpa2[0]), file=config)
    print("#define VM0_CONFIG_MEM_SIZE_HPA2\t{0}UL".format(vm_info.mem_info.mem_size_hpa2[0]), file=config)
    print("", file=config)
    print("#define SOS_VM_BOOTARGS\t\t\tSOS_ROOTFS\t\\", file=config)
    print("\t\t\t\t\tSOS_CONSOLE\t\\", file=config)
    print("\t\t\t\t\tSOS_BOOTARGS_DIFF", file=config)

    print("", file=config)
    print("{0}".format(VM_END_DEFINE), file=config)


def generate_file(scenario, vm_info, config):
    """
    Start to generate vm_configurations.h
    :param scenario: it is scenario name
    :param vm_info: it is the class which contain all user setting information
    :param config: it is a file pointer of board information for writing to
    """
    if scenario == 'sdc':
        gen_sdc_header(vm_info, config)
    elif scenario == 'sdc2':
        gen_sdc2_header(vm_info, config)
    elif scenario == 'logical_partition':
        gen_logical_partition_header(vm_info, config)
    elif scenario == 'industry':
        gen_industry_header(vm_info, config)
    else:
        # scenario is 'hybrid'
        gen_hybrid_header(vm_info, config)
