# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import ctypes
from collections import namedtuple

from pcieparser.header import header
from pcieparser.caps import capabilities
from pcieparser.extcaps import extended_capabilities

class PCIConfigSpace(namedtuple("PCIConfigSpace", ["header", "caps", "extcaps"])):
    def __repr__(self):
        acc = str(self.header)
        for cap in self.caps:
            acc += "\n"
            acc += str(cap)
        for extcap in self.extcaps:
            acc += "\n"
            acc += str(extcap)
        return acc

    def has_cap(self, cap_name):
        for cap in self.caps:
            if cap_name == cap.name:
                return True
        for cap in self.extcaps:
            if cap_name == cap.name:
                return True
        return False

def parse_config_space(path):
    try:
        data = open(os.path.join(path, "config"), mode='rb').read()
        hdr = header(data)
        caps = capabilities(data, hdr.capability_pointer)
        config_space = PCIConfigSpace(hdr, caps, [])
        # While PCI Express specification requires that a PCIe endpoint must have an extended capability header at
        # offset 100h of its configuration space, we do see real PCIe endpoints not meeting this requirement
        # occasionally. Thus, check the length of the configuration space as well before trying to parse its extended
        # capability list.
        if config_space.has_cap("PCI Express") and len(data) >= 260:
            extcaps = extended_capabilities(data)
            config_space = PCIConfigSpace(hdr, caps, extcaps)
        return config_space
    except FileNotFoundError:
        return None
