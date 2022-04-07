#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
import re
import logging
import subprocess # nosec
import lxml.etree
import argparse
from collections import namedtuple
from importlib import import_module

script_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(script_dir))

from cpuparser import parse_cpuid, get_online_cpu_ids, get_offline_cpu_ids
from inspectorlib import validator

class AddLLCCATAction(argparse.Action):
    CATInfo = namedtuple("CATInfo", ["capacity_mask_length", "clos_number", "has_CDP"])

    def __call__(self, parser, namespace, values, option_string=None):
        pattern = re.compile("([0-9]+),([0-9]+),(true|false|y|n|yes|no)")
        if option_string:
            m = pattern.match(values.lower())
            if not m:
                parser.error(f"{values} is ill-formed. The expected format is: <capacity_mask_length:int>,<clos_number:int>,<has_CDP:bool>")
            v = self.CATInfo(int(m.group(1)), int(m.group(2)), m.group(3) in ["true", "y", "yes"])
        else:
            v = None
        setattr(namespace, self.dest, v)

def check_deps():
    # Check that the required tools are installed on the system
    BIN_LIST = ['cpuid', 'rdmsr', 'lspci', ' dmidecode', 'blkid', 'stty']
    cpuid_min_ver = 20170122
    had_error = False
    for execute in BIN_LIST:
        res = subprocess.Popen("which {}".format(execute),
                               shell=True, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, close_fds=True)

        line = res.stdout.readline().decode('ascii')
        if not line:
            logging.critical("'{}' cannot be found. Please install it and run the Board Inspector again.".format(execute))
            had_error = True

        if execute == 'cpuid':
            res = subprocess.Popen("cpuid -v",
                                   shell=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, close_fds=True)
            line = res.stdout.readline().decode('ascii')
            version = line.split()[2]
            if int(version) < cpuid_min_ver:
                logging.critical("This tool requires CPUID version >= {}.  Try updating and upgrading the OS" \
                "on this system and reruning the Board Inspector.  If that fails, install a newer CPUID tool" \
                "from https://github.com/tycho/cpuid.".format(cpuid_min_ver))
                had_error = True
    if had_error:
        sys.exit(1)

    # Try updating pci.ids for latest PCI device descriptions
    try:
        logging.info("Updating pci.ids for latest PCI device descriptions.")
        res = subprocess.Popen(["update-pciids", "-q"])
        if res.wait() != 0:
            logging.warning(f"Failed to invoke update-pciids. No functional impact is foreseen, but descriptions of PCI devices may be inaccurate.")
    except Exception as e:
        logging.warning(f"Failed to invoke update-pciids: {e}. No functional impact is foreseen, but descriptions of PCI devices may be unavailable.")

def native_check():
    cpu_ids = get_online_cpu_ids()
    cpu_id = cpu_ids.pop(0)
    leaf_1 = parse_cpuid(1, 0, cpu_id)
    if leaf_1.hypervisor != 0:
        logging.error("Board inspector is running inside an unsupported Virtual Machine (VM). " \
        "Only KVM or QEMU is supported. Unexpected results may occur.")

def check_pci_domains():
    root_buses = filter(lambda x: x.startswith("pci"), os.listdir("/sys/devices"))
    domain_ids = set(map(lambda x: x.split(":")[0].replace("pci", ""), root_buses))
    if len(domain_ids) > 1:
        logging.fatal(f"ACRN does not support platforms with multiple PCI domains {domain_ids}. Check if the BIOS has any configuration that consolidates those domains into one.")
        sys.exit(1)

def bring_up_cores():
    cpu_ids = get_offline_cpu_ids()
    for id in cpu_ids:
        try:
            with open("/sys/devices/system/cpu/cpu{}/online".format(id), "w") as f:
                f.write("1")
        except :
            logging.warning("Cannot bring up core with cpu id {}.".format(id))

def main(board_name, board_xml, args):
    # Check that the dependencies are met
    check_deps()

    # Check if this is native os
    native_check()

    # Check if there exists multiple PCI domains (which is not supported)
    check_pci_domains()

    # Bring up all cores
    bring_up_cores()

    try:
        # First invoke the legacy board parser to create the board XML ...
        legacy_parser = os.path.join(script_dir, "legacy", "board_parser.py")
        env = { "PYTHONPATH": script_dir, "PATH": os.environ["PATH"] }
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
        root_node.append(lxml.etree.Element("ioapics"))
        root_node.append(lxml.etree.Element("devices"))
        root_node.append(lxml.etree.Element("device-classes"))

        extractors_path = os.path.join(script_dir, "extractors")
        extractors = [f for f in os.listdir(extractors_path) if f[:2].isdigit()]
        for extractor in sorted(extractors):
            module_name = os.path.splitext(extractor)[0]
            module = import_module(f"extractors.{module_name}")
            if args.basic and getattr(module, "advanced", False):
                continue
            module.extract(args, board_etree)

        # Validate the XML against XSD assertions
        count = validator.validate_board(os.path.join(script_dir, 'schema', 'boardchecks.xsd'), board_etree)
        if count == 0:
            logging.info("All board checks passed.")

        # Finally overwrite the output with the updated XML
        board_etree.write(board_xml, pretty_print=True)
        print("SUCCESS: Board configuration file {} generated successfully and saved to {}" \
              .format(board_xml, os.path.dirname(os.path.abspath(board_xml))))

    except subprocess.CalledProcessError as e:
        logging.critical(e)
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("board_name", help="the name of the board that runs the ACRN hypervisor")
    parser.add_argument("--out", help="the name of board info file")
    parser.add_argument("--basic", action="store_true", default=False, help="do not extract advanced information such as ACPI namespace")
    parser.add_argument("--loglevel", default="warning", help="choose log level, e.g. info, warning or error")
    parser.add_argument("--check-device-status", action="store_true", default=False, help="filter out devices whose _STA object evaluates to 0")
    parser.add_argument("--add-llc-cat", default=None, action=AddLLCCATAction,
                        metavar="<capacity_mask_length:int>,<clos_number:int>,<has_CDP:bool>", help="manually set the Cache Allocation Technology capability of the last level cache")
    args = parser.parse_args()
    try:
        logging.basicConfig(level=args.loglevel.upper())
    except ValueError:
        print(f"{args.loglevel} is not a valid log level")
        print(f"Valid log levels (non case-sensitive): critical, error, warning, info, debug")
        sys.exit(1)

    board_xml = args.out if args.out else f"{args.board_name}.xml"
    main(args.board_name, board_xml, args)
