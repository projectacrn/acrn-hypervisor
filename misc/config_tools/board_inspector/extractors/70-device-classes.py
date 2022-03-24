# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import re, os
import logging
from extractors.helpers import add_child, get_node

SYS_DEVICES_CLASS_PATH = "/sys/class/input/"

def get_input_ids():
    input_ids = list()
    root_regex = re.compile("input([0-9]+)")
    for root in filter(lambda x: x.startswith("input"), os.listdir(SYS_DEVICES_CLASS_PATH)):
        m = root_regex.match(root)
        if m:
            input_ids.append(int(m.group(1)))
    return sorted(input_ids)

def extract_topology(device_classes_node):
    inputs_node = add_child(device_classes_node, "inputs", None)
    input_ids = get_input_ids()
    for id in input_ids:
        input_node = add_child(inputs_node, "input", None)
        try:
            with open("/sys/class/input/input{}/name".format(id), "r") as f:
                res = f.read().strip()
                add_child(input_node, "name", res)
        except Exception as e:
            logging.warning(f"Failed to read the data of /sys/class/input/input{id}/name: {e}")

        try:
            with open("/sys/class/input/input{}/phys".format(id), "r") as f:
                res = f.read().strip()
                add_child(input_node, "phys", res)
        except Exception as e:
            logging.warning(f"Failed to read the data of /sys/class/input/input{id}/phys: {e}")

def extract(args, board_etree):
    device_classes_node = get_node(board_etree, "//device-classes")
    extract_topology(device_classes_node)