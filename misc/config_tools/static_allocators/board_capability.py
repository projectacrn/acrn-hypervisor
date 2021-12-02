#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common

def fn(board_etree, scenario_etree, allocation_etree):
    pci_bus_nums =  board_etree.xpath("//bus[@type='pci']/@address")
    common.append_node("/acrn-config/platform/MAX_PCI_BUS_NUM", hex(max(map(lambda x: int(x, 16), pci_bus_nums)) + 1), allocation_etree)
