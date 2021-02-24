#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
import lxml.etree
import argparse
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common
from importlib import import_module

def main(args):
    # Initialize configuration libraries for backward compatibility
    common.BOARD_INFO_FILE = args.board
    common.SCENARIO_INFO_FILE = args.scenario
    common.get_vm_num(args.scenario)
    common.get_vm_types()

    scripts_path = os.path.dirname(os.path.realpath(__file__))
    current = os.path.basename(__file__)

    board_etree = lxml.etree.parse(args.board)
    scenario_etree = lxml.etree.parse(args.scenario)
    allocation_etree = lxml.etree.ElementTree(element=lxml.etree.fromstring("<acrn-config></acrn-config>"))
    for script in [f for f in os.listdir(scripts_path) if f.endswith(".py") and f != current]:
        module_name = os.path.splitext(script)[0]
        module = import_module(f"{module_name}")
        module.fn(board_etree, scenario_etree, allocation_etree)
    allocation_etree.write(args.output, pretty_print=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", help="the XML file summarizing characteristics of the target board")
    parser.add_argument("--scenario", help="the XML file specifying the scenario to be set up")
    parser.add_argument("--output", help="location of the output XML")
    args = parser.parse_args()

    main(args)
