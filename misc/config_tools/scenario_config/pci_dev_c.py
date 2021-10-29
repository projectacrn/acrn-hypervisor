# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import re
from collections import namedtuple

import common
import scenario_cfg_lib
import board_cfg_lib

PCI_DEV_TYPE = ['PCI_DEV_TYPE_HVEMUL', 'PCI_DEV_TYPE_PTDEV']


class BusDevFunc(namedtuple(
        "BusDevFunc", [
            "bus",
            "dev",
            "func"])):

    PATTERN = re.compile(r"(?P<bus>[0-9a-f]{2}):(?P<dev>[0-9a-f]{2})\.(?P<func>[0-7]{1})")

    @classmethod
    def from_str(cls, value):
        if not(isinstance(value, str)):
            raise ValueError("value must be a str: {}".format(type(value)))

        match = cls.PATTERN.fullmatch(value)
        if match:
            return BusDevFunc(
                bus=int(match.group("bus"), 16),
                dev=int(match.group("dev"), 16),
                func=int(match.group("func"), 16))
        else:
            raise ValueError("not a bdf: {!r}".format(value))

    def __init__(self, *args, **kwargs):
        if not (0x00 <= self.bus <= 0xff):
            raise ValueError("Invalid bus number (0x00 ~ 0xff): {:#04x}".format(self.bus))
        if not (0x00 <= self.dev <= 0x1f):
            raise ValueError("Invalid device number (0x00 ~ 0x1f): {:#04x}".format(self.dev))
        if not (0x0 <= self.func <= 0x7):
            raise ValueError("Invalid function number (0 ~ 7): {:#x}".format(self.func))

    def __str__(self):
        return "{:02x}:{:02x}.{:x}".format(self.bus, self.dev, self.func)

    def __repr__(self):
        return "BusDevFunc.from_str({!r})".format(str(self))


def find_unused_bdf(used_bdf, case, last_bdf):
    if case == "vuart":
        # vuart device cannot detect function difference, find vbdf based on dev increment
        for dev in range(0x20):
            bdf = BusDevFunc(bus=0x00, dev=dev, func=0x0)
            #if bdf not in used_bdf:
            if all((bdf.dev != in_use_bdf.dev for in_use_bdf in used_bdf)):
                return bdf
    else:
        if last_bdf == BusDevFunc(bus=0x00, dev=0x00, func=0x0) or last_bdf.func == 0x7:
            for dev in range(0x20):
                bdf = BusDevFunc(bus=0x00, dev=dev, func=0x0)
                #if bdf not in used_bdf:
                if all((bdf.dev != in_use_bdf.dev for in_use_bdf in used_bdf)):
                    return bdf
        else:
            bdf = last_bdf
            for func in range(0x8):
                bdf = BusDevFunc(bdf.bus, bdf.dev, func)
                #if bdf not in used_bdf:
                if bdf not in used_bdf:
                    return bdf
    raise ValueError("Cannot find free bdf")


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
    sos_used_bdf = []

    for cnt_sub_name in board_cfg_lib.SUB_NAME_COUNT.keys():
        i_cnt = 0
        for bdf, bar_attr in board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic.items():
            if cnt_sub_name == bar_attr.name and bdf not in compared_bdf:
                compared_bdf.append(bdf)
            else:
                continue

            add_instance_to_name(i_cnt, bdf, bar_attr)

            i_cnt += 1

    for bdf in compared_bdf:
        bdf_tuple = BusDevFunc.from_str(bdf)
        sos_used_bdf.append(bdf_tuple)

    # BDF 00:01.0 cannot be used in tgl
    bdf_tuple = BusDevFunc(bus=0,dev=1,func=0)
    sos_used_bdf.append(bdf_tuple)

    vuarts = common.get_vuart_info(common.SCENARIO_INFO_FILE)
    pci_vuarts_num = scenario_cfg_lib.get_pci_vuart_num(vuarts)
    pci_vuart_enabled = False
    for vm_i in common.VM_TYPES:
        if pci_vuarts_num[vm_i] > 0:
            pci_vuart_enabled = True
            break



    print("{}".format(scenario_cfg_lib.HEADER_LICENSE), file=config)
    print("", file=config)
    print("#include <asm/vm_config.h>", file=config)
    print("#include <pci_devices.h>", file=config)
    print("#include <vpci.h>", file=config)
    print("#include <vbar_base.h>", file=config)
    print("#include <asm/mmu.h>", file=config)
    print("#include <asm/page.h>", file=config)
    if pci_vuart_enabled:
        print("#include <vmcs9900.h>", file=config)
    # Insert header for share memory
    if vm_info.shmem.shmem_enabled == 'y':
        print("#include <ivshmem_cfg.h>", file=config)

    # Insert comments and macros for passthrough devices
    if any((p for _,p in vm_info.cfg_pci.pci_devs.items())):
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
    if pci_vuart_enabled:
        print("#define INVALID_PCI_BASE\t0U",file=config)

    for vm_i, vm_type in common.VM_TYPES.items():
        vm_used_bdf = []
        # bdf 00:00.0 is reserved for pci host bridge of any type of VM
        bdf_tuple = BusDevFunc.from_str("00:00.0")
        vm_used_bdf.append(bdf_tuple)

        # Skip this vm if there is no any pci device and virtual device
        if not scenario_cfg_lib.get_pci_dev_num_per_vm()[vm_i] and \
             scenario_cfg_lib.VM_DB[vm_type]['load_type'] != "SERVICE_VM":
            continue
        if not scenario_cfg_lib.get_pci_dev_num_per_vm()[vm_i] and \
             scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "SERVICE_VM":
            print("", file=config)
            print("struct acrn_vm_pci_dev_config " +
                  "sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM];", file=config)
            continue

        pci_cnt = 1
        # Insert device structure and bracket
        print("", file=config)
        if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "SERVICE_VM":
            print("struct acrn_vm_pci_dev_config " +
                  "sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM] = {", file=config)
        else:
            print("struct acrn_vm_pci_dev_config " +
                  "vm{}_pci_devs[VM{}_CONFIG_PCI_DEV_NUM] = {{".format(vm_i, vm_i), file=config)

        # If a pre-launched vm has either passthrough pci devices or ivshmem devices, hostbridge is needed
        if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
            pciHostbridge = False
            # Check if there is a passtrhough pci devices
            pci_bdf_devs_list = vm_info.cfg_pci.pci_devs[vm_i]
            if pci_bdf_devs_list:
                pciHostbridge = True
            # Check if the ivshmem is enabled
            if vm_info.shmem.shmem_enabled == 'y' and vm_i in vm_info.shmem.shmem_regions \
             and len(vm_info.shmem.shmem_regions[vm_i]) > 0:
                pciHostbridge = True
            # Check if there is pci vuart is enabled
            if pci_vuarts_num[vm_i] > 0:
                pciHostbridge = True
            if pciHostbridge:
                print("\t{", file=config)
                print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[0]), file=config)
                print("\t\t.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},", file=config)
                print("\t\t.vdev_ops = &vhostbridge_ops,", file=config)
                print("\t},", file=config)

        # Insert passtrough devices data
        if vm_i in vm_info.cfg_pci.pci_devs.keys():
            pci_bdf_devs_list = vm_info.cfg_pci.pci_devs[vm_i]
            if pci_bdf_devs_list:
                for pci_bdf_dev in pci_bdf_devs_list:
                    if not pci_bdf_dev:
                        continue
                    bus = int(pci_bdf_dev.split(':')[0], 16)
                    dev = int(pci_bdf_dev.split(':')[1].split('.')[0], 16)
                    fun = int(pci_bdf_dev.split('.')[1], 16)
                    print("\t{", file=config)
                    print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[1]), file=config)
                    print("\t\t.vbdf.bits = {{.b = 0x00U, .d = 0x{0:02x}U, .f = 0x00U}},".format(pci_cnt), file=config)
                    for bdf, bar_attr in board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic.items():
                        if bdf == pci_bdf_dev:
                            print("\t\tPTDEV({}),".format(board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic[bdf].name_w_i_cnt), file=config)
                        else:
                            continue
                    print("\t},", file=config)
                    bdf_tuple = BusDevFunc(0,pci_cnt,0)
                    vm_used_bdf.append(bdf_tuple)
                    pci_cnt += 1

        # Insert ivshmem information
        if vm_info.shmem.shmem_enabled == 'y' and vm_i in vm_info.shmem.shmem_regions \
             and len(vm_info.shmem.shmem_regions[vm_i]) > 0:
            raw_shm_list = vm_info.shmem.shmem_regions[vm_i]
            index = 0

            last_bdf = BusDevFunc(bus=0x00, dev=0x00, func=0x0)
            for shm in raw_shm_list:
                shm_splited = shm.split(',')
                print("\t{", file=config)
                print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[0]), file=config)

                if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "SERVICE_VM":
                    free_bdf = find_unused_bdf(sos_used_bdf, "ivshmem", last_bdf)
                    last_bdf = free_bdf
                    print("\t\t.vbdf.bits = {{.b = 0x00U, .d = 0x{:02x}U, .f = 0x{:02x}U}}," \
                            .format(free_bdf.dev,free_bdf.func), file=config)
                    sos_used_bdf.append(free_bdf)
                elif scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
                    print("\t\t.vbdf.bits = {{.b = 0x00U, .d = 0x{0:02x}U, .f = 0x00U}},".format(pci_cnt), file=config)
                    bdf_tuple = BusDevFunc(0,pci_cnt,0)
                    vm_used_bdf.append(bdf_tuple)
                elif scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "POST_LAUNCHED_VM":
                    print("\t\t.vbdf.value = UNASSIGNED_VBDF,", file=config)
                print("\t\t.vdev_ops = &vpci_ivshmem_ops,", file=config)

                for shm_name,_ in board_cfg_lib.PCI_DEV_BAR_DESC.shm_bar_dic.items():
                    region = shm_name[:shm_name.find('_')]
                    shm_name = shm_name[shm_name.find('_') + 1:]
                    if shm_name == shm_splited[0].strip():
                        if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
                            print("\t\t.shm_region_name = IVSHMEM_SHM_REGION_{},".format(region), file=config)
                            print("\t\tIVSHMEM_DEVICE_{}_VBAR,".format(index), file=config)
                        elif scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "SERVICE_VM":
                            print("\t\t.shm_region_name = IVSHMEM_SHM_REGION_{},".format(region), file=config)
                            print("\t\tSOS_IVSHMEM_DEVICE_{}_VBAR,".format(index), file=config)
                        else:
                            print("\t\t.shm_region_name = IVSHMEM_SHM_REGION_{},".format(region), file=config)
                pci_cnt += 1
                index += 1
                print("\t},", file=config)

        if vm_i in vuarts.keys():
            # get legacy vuart information
            vuart0_setting = common.get_vuart_info_id(common.SCENARIO_INFO_FILE, 0)
            vuart1_setting = common.get_vuart_info_id(common.SCENARIO_INFO_FILE, 1)

            last_bdf = BusDevFunc(bus=0x00, dev=0x00, func=0x0)
            for vuart_id in vuarts[vm_i].keys():
                if vuarts[vm_i][vuart_id]['base'] == "INVALID_PCI_BASE":
                    continue
                # skip pci vuart 0 for post-launched vm
                if vuart_id == 0 and scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "POST_LAUNCHED_VM":
                    continue
                # Skip pci vuart 0 if the legacy vuart 0 is enabled
                if vuart_id == 0 and vm_i in vuart0_setting and vuart0_setting[vm_i]['base'] != "INVALID_COM_BASE":
                    continue
                # Skip pci vuart 1 if the legacy vuart 1 is enabled
                if vuart_id == 1 and vm_i in vuart1_setting and vuart1_setting[vm_i]['base'] != "INVALID_COM_BASE":
                    continue

                print("\t{", file=config)
                print("\t\t.vuart_idx = {:1d},".format(vuart_id), file=config)
                print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[0]), file=config)
                print("\t\t.vdev_ops = &vmcs9900_ops,", file=config)

                if vuart_id != 0 and scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "POST_LAUNCHED_VM":
                    print("\t\t.vbar_base[0] = INVALID_PCI_BASE,", file=config)
                    print("\t\t.vbdf.value = UNASSIGNED_VBDF,", file=config)

                if scenario_cfg_lib.VM_DB[vm_type]['load_type'] != "POST_LAUNCHED_VM":
                    print("\t\tVM{:1d}_VUART_{:1d}_VBAR,".format(vm_i, vuart_id), file=config)
                    if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
                        free_bdf = find_unused_bdf(vm_used_bdf, "vuart", last_bdf)
                        vm_used_bdf.append(free_bdf)
                    elif scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "SERVICE_VM":
                        free_bdf = find_unused_bdf(sos_used_bdf, "vuart", last_bdf)
                        sos_used_bdf.append(free_bdf)
                    print("\t\t.vbdf.bits = {{.b = 0x00U, .d = 0x{:02x}U, .f = 0x00U}},".format(free_bdf.dev,free_bdf.func), file=config)

                if vuart_id != 0:
                    print("\t\t.t_vuart.vm_id = {},".format(vuarts[vm_i][vuart_id]['target_vm_id']), file=config)
                    print("\t\t.t_vuart.vuart_id = {},".format(vuarts[vm_i][vuart_id]['target_uart_id']), file=config)
                pci_cnt += 1
                print("\t},", file=config)

        # Insert the end bracket of the pci_dev.c file
        print("};", file=config)
