# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common
import scenario_cfg_lib

PCI_DEV_TYPE = ['PCI_DEV_TYPE_HVEMUL', 'PCI_DEV_TYPE_PTDEV']


def generate_file(vm_info, config):
    """
    Generate pci_dev.c while logical_partition scenario
    :param config: it is pointer for for file write to
    :return: None
    """
    print("{}".format(scenario_cfg_lib.HEADER_LICENSE), file=config)
    print("", file=config)
    print("#include <vm_config.h>", file=config)
    print("#include <pci_devices.h>", file=config)
    print("#include <vpci.h>", file=config)
    print("#include <mmu.h>", file=config)
    print("#include <page.h>", file=config)
    print("", file=config)
    print("/* The vbar_base info of pt devices is included in device MACROs which defined in",
          file=config)
    print(" *           arch/x86/configs/$(CONFIG_BOARD)/pci_devices.h.", file=config)
    print(" * The memory range of vBAR should exactly match with the e820 layout of VM.",
          file=config)
    print(" */", file=config)
    for vm_i, pci_bdf_devs_list in vm_info.cfg_pci.pci_devs.items():
        pci_cnt = 1
        if not pci_bdf_devs_list:
            continue

        print("", file=config)
        print("struct acrn_vm_pci_dev_config " +
              "vm{}_pci_devs[{}] = {{".format(vm_i, vm_info.cfg_pci.pci_dev_num[vm_i]), file=config)
        print("\t{", file=config)
        print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[0]), file=config)
        print("\t\t.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},", file=config)
        print("\t\t.vdev_ops = &vhostbridge_ops,", file=config)
        print("\t},", file=config)

        for pci_bdf_dev in pci_bdf_devs_list:
            if not pci_bdf_dev:
                continue
            bus = int(pci_bdf_dev.split(':')[0], 16)
            dev = int(pci_bdf_dev.split(':')[1].split('.')[0], 16)
            fun = int(pci_bdf_dev.split('.')[1], 16)
            print("\t{", file=config)
            print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[1]), file=config)
            print("\t\t.vbdf.bits = {{.b = 0x00U, .d = 0x0{}U, .f = 0x00U}},".format(pci_cnt), file=config)
            print("\t\t.pbdf.bits = {{.b = 0x{:02X}U, .d = 0x{:02X}U, .f = 0x{:02X}U}},".format(bus, dev, fun), file=config)
            print("\t},", file=config)
            pci_cnt += 1

        print("};", file=config)
