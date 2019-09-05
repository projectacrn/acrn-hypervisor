# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import scenario_cfg_lib
PCI_DEV_TYPE = ['PCI_DEV_TYPE_HVEMUL', 'PCI_DEV_TYPE_PTDEV']


def generate_file(config):
    """
    Generate pci_dev.c while logical_partition scenario
    :param config: it is pointer for for file write to
    :return: None
    """
    print("{}".format(scenario_cfg_lib.HEADER_LICENSE), file=config)
    print("#include <vm_config.h>", file=config)
    print("#include <pci_devices.h>", file=config)
    print("#include <vpci.h>", file=config)
    print("", file=config)
    print("/* The vbar_base info of pt devices is included in device MACROs which defined in",
          file=config)
    print(" *           arch/x86/configs/$(CONFIG_BOARD)/pci_devices.h.", file=config)
    print(" * The memory range of vBAR should exactly match with the e820 layout of VM.",
          file=config)
    print(" */", file=config)
    for i in range(scenario_cfg_lib.VM_COUNT):
        print("struct acrn_vm_pci_dev_config " +
              "vm{}_pci_devs[VM{}_CONFIG_PCI_DEV_NUM] = {{".format(i, i), file=config)
        print("\t{", file=config)
        print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[0]), file=config)
        print("\t\t.vbdf.bits = {.b = 0x00U, .d = 0x00U, .f = 0x00U},", file=config)
        print("\t\t.vdev_ops = &vhostbridge_ops,", file=config)
        print("\t},", file=config)
        print("\t{", file=config)
        print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[1]), file=config)
        print("\t\t.vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x00U},", file=config)
        print("\t\tVM{}_STORAGE_CONTROLLER".format(i), file=config)
        print("\t},", file=config)
        if i != 0:
            print("#if defined(VM{}_NETWORK_CONTROLLER)".format(i), file=config)
        print("\t{", file=config)
        print("\t\t.emu_type = {},".format(PCI_DEV_TYPE[1]), file=config)
        print("\t\t.vbdf.bits = {.b = 0x00U, .d = 0x02U, .f = 0x00U},", file=config)
        print("\t\tVM{}_NETWORK_CONTROLLER".format(i), file=config)
        print("\t},", file=config)
        if i != 0:
            print("#endif", file=config)
        print("};", file=config)
        print("", file=config)
