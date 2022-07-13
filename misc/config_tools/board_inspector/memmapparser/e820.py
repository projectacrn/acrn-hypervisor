# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
from collections import namedtuple

E820_TYPE_RAM = 1
E820_TYPE_RESERVED = 2
E820_TYPE_ACPI = 3
E820_TYPE_NVS = 4
E820_TYPE_UNKNOWN = 0xff

name_of_types = {
    E820_TYPE_RAM: "System RAM",
    E820_TYPE_RESERVED: "Reserved",
    E820_TYPE_ACPI: "ACPI Tables",
    E820_TYPE_NVS: "ACPI Non-volatile Storage",
    E820_TYPE_UNKNOWN: "Unknown E820 type",
}

type_of_names = { v: k for (k,v) in name_of_types.items() }

class E820Entry(namedtuple("E820Entry", ["start", "end", "type"])):
    def __repr__(self):
        return "{0}(start=0x{1:016x}, end=0x{2:016x}, type='{3}')".format(
            self.__class__.__name__, self.start, self.end, name_of_types.get(self.type))

sysfs_memmap_path = "/sys/firmware/memmap"

def read_file(path):
    with open(path, "r") as f:
        return f.read()
    return ""

def parse_e820():
    acc = list()

    for i in os.listdir(sysfs_memmap_path):
        start = int(read_file(os.path.join(sysfs_memmap_path, i, "start")), base=16)
        end = int(read_file(os.path.join(sysfs_memmap_path, i, "end")), base=16)
        type_name = read_file(os.path.join(sysfs_memmap_path, i, "type")).strip()
        ty = type_of_names.get(type_name, E820_TYPE_UNKNOWN)
        acc.append(E820Entry(start, end, ty))

    return sorted(acc, key=lambda x: x.start)
