#!/usr/bin/env python3
#
# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
import re
import logging
import tempfile
import subprocess # nosec
import lxml.etree
import argparse
from tqdm import tqdm
from collections import namedtuple
from importlib import import_module

script_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(script_dir))

from cpuparser import parse_cpuid, get_online_cpu_ids, get_offline_cpu_ids
from inspectorlib import external_tools, validator

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
    had_error = not external_tools.locate_tools(["cpuid", "rdmsr", "lspci", "dmidecode", "blkid", "stty", "modprobe"])

    try:
        cpuid_min_ver = 20170122
        res = external_tools.run("cpuid -v")
        line = res.stdout.readline().decode("ascii")
        version = line.split()[2]
        if int(version) < cpuid_min_ver:
            logger.critical("This tool requires CPUID version >= {}.  Try updating and upgrading the OS" \
            "on this system and reruning the Board Inspector.  If that fails, install a newer CPUID tool" \
            "from https://github.com/tycho/cpuid.".format(cpuid_min_ver))
            had_error = True
    except external_tools.ExecutableNotFound:
        pass

    if had_error:
        sys.exit(1)

    # Try updating pci.ids for latest PCI device descriptions
    external_tools.locate_tools(["update-pciids"])
    try:
        logger.info("Updating pci.ids for latest PCI device descriptions.")
        res = external_tools.run("update-pciids -q", stderr=subprocess.DEVNULL)
        if res.wait(timeout=40) != 0:
            logger.warning(f"Failed to invoke update-pciids. No functional impact is foreseen, but descriptions of PCI devices may be inaccurate.")
    except Exception as e:
        logger.warning(f"Failed to invoke update-pciids: {e}. No functional impact is foreseen, but descriptions of PCI devices may be unavailable.")

def native_check():
    cpu_ids = get_online_cpu_ids()
    cpu_id = cpu_ids.pop(0)
    leaf_1 = parse_cpuid(1, 0, cpu_id)
    if leaf_1.hypervisor != 0:
        logger.error("Board inspector is running inside an unsupported Virtual Machine (VM). " \
        "Only KVM or QEMU is supported. Unexpected results may occur.")

def check_pci_domains():
    root_buses = os.listdir("/sys/bus/pci/devices/")
    domain_ids = set(map(lambda x: x.split(":")[0], root_buses))
    if len(domain_ids) > 1:
        logger.fatal(f"ACRN does not support platforms with multiple PCI domains {domain_ids}. " \
        "Check if the BIOS has any configuration that consolidates those domains into one. " \
        "Known causes of multiple PCI domains include: VMD (Volume Management Device) being enabled.")
        sys.exit(1)

def bring_up_cores():
    cpu_ids = get_offline_cpu_ids()
    for id in cpu_ids:
        try:
            with open("/sys/devices/system/cpu/cpu{}/online".format(id), "w") as f:
                f.write("1")
        except :
            logger.warning("Cannot bring up core with cpu id {}.".format(id))

def summary_loginfo(board_xml):
    length = 120
    warning_list = []
    error_list = []
    critical_list = []
    log_line = open(str(tmpfile.name), "r", encoding='UTF-8')
    for line in log_line:
        if "WARNING" in line:
            warning_list.append(line)
        elif "ERROR" in line:
            error_list.append(line)
        elif "CRITICAL" in line:
            critical_list.append(line)

    if len(warning_list) != 0:
        print("="*length)
        print("\033[1;37mWARNING\033[0m")
        print("These issues affect optional features. You can ignore them if they don't apply to you.\n")
        for warning in warning_list:
            print("\033[1;33m{0}\033[0m".format(warning.strip('\n')))

    if len(error_list) != 0:
        print("="*length)
        print("\033[1;37mERROR\033[0m")
        print("You must resolve these issues to generate a VALID board configuration file for building and boot ACRN.\n")
        for error in error_list:
            print("\033[1;31m{0}\033[0m".format(error.strip('\n')))

    if len(critical_list) != 0:
        print("="*length)
        print("\033[1;37mCRITICAL\033[0m")
        print("You must resolve these issues to generate a board configuration file.\n")
        for critical in critical_list:
            print("\033[1;31m{0}\033[0m".format(critical.strip('\n')))

    print("="*length)
    if len(critical_list) == 0:
        print(
            f"\033[1;32mSUCCESS: Board configuration file {board_xml} generated successfully and saved to {os.path.dirname(os.path.abspath(board_xml))}\033[0m\n")
    if len(error_list) != 0:
        print("\033[1;36mNOTE: Board configuration file lacks important features, which will cause ACRN to fail build or boot. Resolve ERROR messages then run the tool again.\033[0m")
    tmpfile.close()

def main(board_name, board_xml, args):
    print(f"Generating board XML {board_name}. This may take a few minutes...")

    with tqdm(total=100) as pbar:
        # Check that the dependencies are met
        check_deps()
        pbar.update(10)

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
            pbar.update(10)

            extractors_path = os.path.join(script_dir, "extractors")
            extractors = [f for f in os.listdir(extractors_path) if f[:2].isdigit()]
            for extractor in sorted(extractors):
                module_name = os.path.splitext(extractor)[0]
                module = import_module(f"extractors.{module_name}")
                if args.basic and getattr(module, "advanced", False):
                    continue
                module.extract(args, board_etree)
                if "50-acpi-namespace.py" in module_name:
                    pbar.update(30)
                else:
                    pbar.update(10)

            # Validate the XML against XSD assertions
            count = validator.validate_board(os.path.join(script_dir, 'schema', 'boardchecks.xsd'), board_etree)
            if count == 0:
                logger.info("All board checks passed.")

            # Finally overwrite the output with the updated XML
            board_etree.write(board_xml, pretty_print=True)

            #Format and out put the log info
            summary_loginfo(board_xml)
            pbar.update(10)

        except subprocess.CalledProcessError as e:
            logger.critical(e)
            sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("board_name", help="the name of the board that runs the ACRN hypervisor")
    parser.add_argument("--out", help="the name of board info file")
    parser.add_argument("--basic", action="store_true", default=False, help="do not extract advanced information such as ACPI namespace")
    parser.add_argument("--loglevel", default="warning", help="choose log level, e.g. debug, info, warning, error or critical")
    parser.add_argument("--check-device-status", action="store_true", default=False, help="filter out devices whose _STA object evaluates to 0")
    parser.add_argument("--add-llc-cat", default=None, action=AddLLCCATAction,
                        metavar="<capacity_mask_length:int>,<clos_number:int>,<has_CDP:bool>", help="manually set the Cache Allocation Technology capability of the last level cache")
    args = parser.parse_args()
    try:
        tmpfile = tempfile.NamedTemporaryFile(delete=True)
        logger = logging.getLogger()
        logger.setLevel(args.loglevel.upper())
        formatter = logging.Formatter('%(asctime)s-%(name)s-%(levelname)s:-%(message)s', datefmt='%Y-%m-%d %H:%M:%S')
        fh = logging.FileHandler(str(tmpfile.name))
        fh.setLevel(args.loglevel.upper())
        fh.setFormatter(formatter)

        sh = logging.StreamHandler()
        sh.setLevel(args.loglevel.upper())

        sh.setFormatter(formatter)
        logger.addHandler(fh)
        logger.addHandler(sh)

    except ValueError:
        print(f"{args.loglevel} is not a valid log level")
        print(f"Valid log levels (non case-sensitive): critical, error, warning, info, debug")
        sys.exit(1)

    board_xml = args.out if args.out else f"{args.board_name}.xml"
    main(args.board_name, board_xml, args)
