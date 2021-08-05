#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
import logging
import subprocess
import lxml.etree
import argparse
from importlib import import_module
from cpuparser import parse_cpuid, get_online_cpu_ids

script_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(script_dir))

def native_check():
    cpu_ids = get_online_cpu_ids()
    cpu_id = cpu_ids.pop(0)
    leaf_1 = parse_cpuid(1, 0, cpu_id)
    if leaf_1.hypervisor != 0:
        logging.warning(f"Board inspector is running inside a Virtual Machine (VM). Running ACRN inside a VM is only" \
        "supported under KVM/QEMU. Unexpected results may occur when deviating from that combination.")

def main(board_name, board_xml, args):
    # Check if this is native os
    native_check()

    try:
        # First invoke the legacy board parser to create the board XML ...
        legacy_parser = os.path.join(script_dir, "legacy", "board_parser.py")
        env = { "PYTHONPATH": script_dir }
        subprocess.run([sys.executable, legacy_parser, args.board_name, "--out", board_xml], check=True, env=env)

        # ... then load the created board XML and append it with additional data by invoking the extractors.
        board_etree = lxml.etree.parse(board_xml)
        root_node = board_etree.getroot()

        # Clear the whitespaces between adjacent children under the root node
        root_node.text = None
        for elem in root_node:
            elem.tail = None

        # Create nodes for each kind of resource
        root_node.append(lxml.etree.Element("processors"))
        root_node.append(lxml.etree.Element("caches"))
        root_node.append(lxml.etree.Element("memory"))
        root_node.append(lxml.etree.Element("devices"))

        extractors_path = os.path.join(script_dir, "extractors")
        extractors = [f for f in os.listdir(extractors_path) if f[:2].isdigit()]
        for extractor in sorted(extractors):
            module_name = os.path.splitext(extractor)[0]
            module = import_module(f"extractors.{module_name}")
            if not args.advanced and getattr(module, "advanced", False):
                continue
            module.extract(args, board_etree)

        # Finally overwrite the output with the updated XML
        board_etree.write(board_xml, pretty_print=True)

    except subprocess.CalledProcessError as e:
        print(e)
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("board_name", help="the name of the board that runs the ACRN hypervisor")
    parser.add_argument("--out", help="the name of board info file")
    parser.add_argument("--advanced", action="store_true", default=False, help="extract advanced information such as ACPI namespace")
    parser.add_argument("--loglevel", default="warning", help="choose log level, e.g. info, warning or error")
    parser.add_argument("--check-device-status", action="store_true", default=False, help="filter out devices whose _STA object evaluates to 0")
    args = parser.parse_args()
    try:
        logging.basicConfig(level=args.loglevel.upper())
    except ValueError:
        print(f"{args.loglevel} is not a valid log level")
        print(f"Valid log levels (non case-sensitive): critical, error, warning, info, debug")
        sys.exit(1)

    board_xml = args.out if args.out else f"{args.board_name}.xml"
    main(args.board_name, board_xml, args)
