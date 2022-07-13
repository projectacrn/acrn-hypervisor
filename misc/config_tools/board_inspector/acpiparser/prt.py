# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from collections import namedtuple
from .aml import datatypes, context

class PRTMappingPackage(namedtuple("PRTMappingPackage", ["address", "pin", "source", "source_index"])):
    def __repr__(self):
        if isinstance(self.source, context.DeviceDecl):
            s = self.source.name
        else:
            s = str(self.source)
        return "address=0x{0:08x}, pin=0x{1:02x}, source={2}, source_index=0x{3:08x}".format(
            self.address, self.pin, s, self.source_index)

def parse_prt_mapping(x):
    address = x.elements[0].get()
    pin = x.elements[1].get()
    source = x.elements[2]
    if isinstance(source, datatypes.Device):
        source = source.get_sym()
    elif isinstance(source, datatypes.Integer):
        source = source.get()
    else:
        source = "unknown"
    source_index = x.elements[3].get()

    return PRTMappingPackage(address, pin, source, source_index)

def parse_pci_routing(package):
    """Parse ACPI PCI routing table returned by _PRT control methods."""
    return list(map(parse_prt_mapping, package.elements))
