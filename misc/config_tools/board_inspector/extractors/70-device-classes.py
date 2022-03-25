# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import re, os
import logging
from extractors.helpers import add_child, get_node

SYS_INPUT_DEVICES_CLASS_PATH = "/sys/class/input/"
SYS_TTY_DEVICES_CLASS_PATH = "/sys/class/tty/"

def add_child_with_file_contents(parent_node, tag, filepath, translations = {}):
    try:
        with open(filepath, "r") as f:
            res = f.read().strip()
            if res in translations.keys():
                add_child(parent_node, tag, translations[res])
            else:
                add_child(parent_node, tag, res)
    except Exception as e:
        logging.warning(f"Failed to read the data from {filepath}: {e}")

def get_input_ids():
    input_ids = list()
    root_regex = re.compile("input([0-9]+)")
    for root in filter(lambda x: x.startswith("input"), os.listdir(SYS_INPUT_DEVICES_CLASS_PATH)):
        m = root_regex.match(root)
        if m:
            input_ids.append(int(m.group(1)))
    return sorted(input_ids)

def extract_inputs(device_classes_node):
    inputs_node = add_child(device_classes_node, "inputs", None)
    input_ids = get_input_ids()
    for id in input_ids:
        input_node = add_child(inputs_node, "input", None)
        add_child_with_file_contents(input_node, "name", f"/sys/class/input/input{id}/name")
        add_child_with_file_contents(input_node, "phys", f"/sys/class/input/input{id}/phys")

def get_serial_devs():
    return sorted(filter(lambda x: x.startswith("ttyS"), os.listdir(SYS_TTY_DEVICES_CLASS_PATH)), key=lambda x:int(x[4:]))

def extract_ttys(device_classes_node):
    ttys_node = add_child(device_classes_node, "ttys", None)
    for serial_dev in get_serial_devs():
        serial_node = add_child(ttys_node, "serial")
        add_child(serial_node, "dev_path", f"/dev/{serial_dev}")
        add_child_with_file_contents(serial_node, "type", f"{SYS_TTY_DEVICES_CLASS_PATH}{serial_dev}/type")

def extract_topology(device_classes_node):
    extract_inputs(device_classes_node)
    extract_ttys(device_classes_node)

def extract(args, board_etree):
    device_classes_node = get_node(board_etree, "//device-classes")
    extract_topology(device_classes_node)
