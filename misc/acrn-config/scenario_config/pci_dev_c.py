# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common
import scenario_cfg_lib
import board_cfg_lib

PCI_DEV_TYPE = ['PCI_DEV_TYPE_HVEMUL', 'PCI_DEV_TYPE_PTDEV']

def add_instance_to_name(i_cnt, bdf, bar_attr):
    if i_cnt == 0 and bar_attr.name.upper() == "HOST BRIDGE":
        tmp_sub_name = "_".join(bar_attr.name.split()).upper()
    else:
        if '-' in bar_attr.name:
            tmp_sub_name = common.undline_name(bar_attr.name) + "_" + str(i_cnt)
        else:
            tmp_sub_name = "_".join(bar_attr.name.split()).upper() + "_" + str(i_cnt)

    board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic[bdf].name_w_i_cnt = tmp_sub_name

def generate_file(vm_info, config):
    """
    Generate pci_dev.c for Pre-Launched VMs in a scenario.
    :param config: it is pointer for for file write to
    :return: None
    """
    board_cfg_lib.parser_pci()
    board_cfg_lib.parse_mem()

    compared_bdf = []

    for cnt_sub_name in board_cfg_lib.SUB_NAME_COUNT.keys():
        i_cnt = 0
        for bdf, bar_attr in board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic.items():
            if cnt_sub_name == bar_attr.name and bdf not in compared_bdf:
                compared_bdf.append(bdf)
            else:
                continue

            add_instance_to_name(i_cnt, bdf, bar_attr)

            i_cnt += 1

    idx = 0
    print("{}".format(scenario_cfg_lib.HEADER_LICENSE), file=config)
    print("", file=config)
    print("#include <vm_config.h>", file=config)
    print("#include <pci_devices.h>", file=config)
    print("#include <vpci.h>", file=config)
    print("#include <vbar_base.h>", file=config)
    print("#include <mmu.h>", file=config)
    print("#include <page.h>", file=config)
    if vm_info.shmem.shmem_enabled == 'y':
        print("#include <ivshmem_cfg.h>", file=config)
    for vm_i, pci_bdf_devs_list in vm_info.cfg_pci.pci_devs.items():
        if not pci_bdf_devs_list:
            continue
        pci_cnt = 1
        if idx == 0:
            print("", file=config)
            print("/*", file=config)
            print(" * TODO: remove PTDEV macro and add DEV_PRIVINFO macro to initialize pbdf for", file=config)
            print(" * passthrough device configuration and shm_name for ivshmem device configuration.", file=config)
            print(" */", file=config)
            print("#define PTDEV(PCI_DEV)\t\tPCI_DEV, PCI_DEV##_VBAR",file=config)
        print("", file=config)
        print("/*", file=config)
        print(" * TODO: add DEV_PCICOMMON macro to initialize emu_type, vbdf and vdev_ops", file=config)
        print(" * to simplify the code.", file=config)
        print(" */", file=config)
        print("struct acrn_vm_pci_dev_config " +
              "vm{}_pci_devs[VM{}_CONFIG_PCI_DEV_NUM] = {{".format(vm_i, vm_i), file=config)
        print("\t{", file=config)
        print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[0]), file=config)
        print("\t\t.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},", file=config)
        print("\t\t.vdev_ops = &vhostbridge_ops,", file=config)
        print("\t},", file=config)

        idx += 1
        for pci_bdf_dev in pci_bdf_devs_list:
            if not pci_bdf_dev:
                continue
            bus = int(pci_bdf_dev.split(':')[0], 16)
            dev = int(pci_bdf_dev.split(':')[1].split('.')[0], 16)
            fun = int(pci_bdf_dev.split('.')[1], 16)
            print("\t{", file=config)
            print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[1]), file=config)
            print("\t\t.vbdf.bits = {{.b = 0x00U, .d = 0x{0:02d}U, .f = 0x00U}},".format(pci_cnt), file=config)
            for bdf, bar_attr in board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic.items():
                if bdf == pci_bdf_dev:
                    print("\t\tPTDEV({}),".format(board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic[bdf].name_w_i_cnt), file=config)
                else:
                    continue
            print("\t},", file=config)
            pci_cnt += 1

        if vm_info.shmem.shmem_enabled == 'y' and vm_i in vm_info.shmem.shmem_regions.keys() \
                and len(vm_info.shmem.shmem_regions[vm_i]) > 0:
            raw_shm_list = vm_info.shmem.shmem_regions[vm_i]
            for shm in raw_shm_list:
                shm_splited = shm.split(',')
                print("\t{", file=config)
                print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[0]), file=config)
                print("\t\t.vbdf.bits = {{.b = 0x00U, .d = 0x{0:02d}U, .f = 0x00U}},".format(pci_cnt), file=config)
                print("\t\t.vdev_ops = &vpci_ivshmem_ops,", file=config)
                for shm_name, bar_attr in board_cfg_lib.PCI_DEV_BAR_DESC.shm_bar_dic.items():
                    index = shm_name[:shm_name.find('_')]
                    shm_name = shm_name[shm_name.find('_') + 1:]
                    if shm_name == shm_splited[0].strip():
                        print("\t\t.shm_region_name = IVSHMEM_SHM_REGION_{},".format(index), file=config)
                        print("\t\tIVSHMEM_DEVICE_{}_VBAR".format(index), file=config)
                # print("\t\t.vbar_size[0] = 0x100,", file=config)
                # print("\t\t.vbar_size[2] = {},".format(shm_splited[1].strip()), file=config)
                # print('\t\t.shm_name = "{}",'.format(shm_splited[0].strip()), file=config)
                print("\t},", file=config)
                pci_cnt += 1

        print("};", file=config)

    if vm_info.shmem.shmem_enabled == 'y':
        for shm_i, raw_shm_list in vm_info.shmem.shmem_regions.items():
            shm_cnt = 0
            if shm_i not in vm_info.cfg_pci.pci_devs.keys() and len(raw_shm_list) > 0:
                print("", file=config)
                print("struct acrn_vm_pci_dev_config " +
                      "vm{}_pci_devs[VM{}_CONFIG_PCI_DEV_NUM] = {{".format(shm_i, shm_i), file=config)
                for shm in raw_shm_list:
                    shm_splited = shm.split(',')
                    print("\t{", file=config)
                    print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[0]), file=config)
                    if shm_i in common.VM_TYPES.keys() and common.VM_TYPES[shm_i] in ['PRE_RT_VM', 'PRE_STD_VM', 'SAFETY_VM']:
                        print("\t\t.vbdf.bits = {{.b = 0x00U, .d = 0x{0:02d}U, .f = 0x00U}},".format(shm_cnt), file=config)
                    else:
                        print("\t\t.vbdf.value = UNASSIGNED_VBDF,".format(shm_cnt), file=config)
                    print("\t\t.vdev_ops = &vpci_ivshmem_ops,", file=config)
                    for shm_name, bar_attr in board_cfg_lib.PCI_DEV_BAR_DESC.shm_bar_dic.items():
                        index = shm_name[:shm_name.find('_')]
                        shm_name = shm_name[shm_name.find('_')+1:]
                        if shm_name == shm_splited[0].strip():
                            if shm_i in common.VM_TYPES.keys() and common.VM_TYPES[shm_i] in ['PRE_RT_VM', 'PRE_STD_VM',
                                                                                              'SAFETY_VM']:
                                print("\t\t.shm_region_name = IVSHMEM_SHM_REGION_{},".format(index), file=config)
                                print("\t\tIVSHMEM_DEVICE_{}_VBAR".format(index), file=config)
                                break
                            else:
                                print("\t\t.shm_region_name = IVSHMEM_SHM_REGION_{}".format(index), file=config)
                                break
                    shm_cnt += 1
                    print("\t},", file=config)
                print("};", file=config)