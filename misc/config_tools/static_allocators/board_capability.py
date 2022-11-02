#!/usr/bin/env python3
#
# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#
import logging
import acrn_config_utilities
from acrn_config_utilities import get_node

def powerof2_roundup(value):
    return 0 if value == 0 else (1 << (value - 1).bit_length())

# Make sure all PT IRQs work w/ interrupt remapping or post interrupt
def create_max_ir_entries(scenario_etree, allocation_etree):
    pt_irq_entries = get_node(f"//MAX_PT_IRQ_ENTRIES/text()", scenario_etree)
    if (pt_irq_entries is not None) and (int(pt_irq_entries) > 256):
        ir_entries = powerof2_roundup(int(pt_irq_entries))
    else:
        ir_entries = 256

    acrn_config_utilities.append_node("/acrn-config/hv/MAX_IR_ENTRIES", ir_entries, allocation_etree)

def fn(board_etree, scenario_etree, allocation_etree):
    pci_bus_nums =  board_etree.xpath("//bus[@type='pci']/@address")
    calc_pci_bus_nums = (max(map(lambda x: int(x, 16), pci_bus_nums)) + 1)
    user_def_pci_bus_nums = get_node(f"//MAX_PCI_BUS_NUM/text()", scenario_etree)
    if user_def_pci_bus_nums == '0':
        acrn_config_utilities.append_node("/acrn-config/platform/MAX_PCI_BUS_NUM", hex(calc_pci_bus_nums), allocation_etree)
    else:
        if calc_pci_bus_nums > int(user_def_pci_bus_nums):
            logging.error(f"MAX_PCI_BUS_NUM should be greater than {calc_pci_bus_nums}")
            sys.exit(1)
        else:
            acrn_config_utilities.append_node("/acrn-config/platform/MAX_PCI_BUS_NUM", hex(int(user_def_pci_bus_nums)), allocation_etree)
    create_max_ir_entries(scenario_etree, allocation_etree)
