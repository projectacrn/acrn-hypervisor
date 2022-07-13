#!/usr/bin/env python3
#
# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common

def powerof2_roundup(value):
    return 0 if value == 0 else (1 << (value - 1).bit_length())

# Make sure all PT IRQs work w/ interrupt remapping or post interrupt
def create_max_ir_entries(scenario_etree, allocation_etree):
    pt_irq_entries = common.get_node(f"//MAX_PT_IRQ_ENTRIES/text()", scenario_etree)
    if (pt_irq_entries is not None) and (int(pt_irq_entries) > 256):
        ir_entries = powerof2_roundup(int(pt_irq_entries))
    else:
        ir_entries = 256

    common.append_node("/acrn-config/hv/MAX_IR_ENTRIES", ir_entries, allocation_etree)

def fn(board_etree, scenario_etree, allocation_etree):
    pci_bus_nums =  board_etree.xpath("//bus[@type='pci']/@address")
    common.append_node("/acrn-config/platform/MAX_PCI_BUS_NUM", hex(max(map(lambda x: int(x, 16), pci_bus_nums)) + 1), allocation_etree)
    create_max_ir_entries(scenario_etree, allocation_etree)
