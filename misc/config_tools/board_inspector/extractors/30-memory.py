# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import logging
import lxml.etree

from memmapparser import parse_e820, e820
from extractors.helpers import get_node, add_child

def extract_layout(memory_node):
    e820_table = parse_e820()
    for e820_entry in e820_table:
        if e820_entry.type == e820.E820_TYPE_RAM:
            start = "0x{:016x}".format(e820_entry.start)
            end = "0x{:016x}".format(e820_entry.end)
            size = e820_entry.end - e820_entry.start + 1
            add_child(memory_node, "range", start=start, end=end, size=str(size))

def extract(args, board_etree):
    memory_node = get_node(board_etree, "//memory")
    extract_layout(memory_node)
