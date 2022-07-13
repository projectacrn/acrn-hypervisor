# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import cpuparser.cpuids

dispatch_table = {
    0x0: cpuparser.cpuids.LEAF_0,
    0x1: cpuparser.cpuids.LEAF_1,
    0x2: cpuparser.cpuids.LEAF_2,
    0x4: cpuparser.cpuids.LEAF_4,
    0x5: cpuparser.cpuids.LEAF_5,
    0x6: cpuparser.cpuids.LEAF_6,
    0x7: cpuparser.cpuids.LEAF_7,
    0x9: cpuparser.cpuids.LEAF_9,
    0xA: cpuparser.cpuids.LEAF_A,
    0xB: cpuparser.cpuids.LEAF_B,
    # 0xD: multiple parsers
    # 0xF: multiple parsers
    # 0x10: multiple parsers
    0x1A: cpuparser.cpuids.LEAF_1A,
    0x1F: cpuparser.cpuids.LEAF_1F,
    0x80000000: cpuparser.cpuids.LEAF_80000000,
    0x80000001: cpuparser.cpuids.LEAF_80000001,
    0x80000002: cpuparser.cpuids.LEAF_80000002,
    0x80000003: cpuparser.cpuids.LEAF_80000003,
    0x80000004: cpuparser.cpuids.LEAF_80000004,
    0x80000006: cpuparser.cpuids.LEAF_80000006,
    0x80000007: cpuparser.cpuids.LEAF_80000007,
    0x80000008: cpuparser.cpuids.LEAF_80000008,
}

def parse_cpuid(leaf, subleaf, cpu_id):
    if leaf in dispatch_table.keys():
        return dispatch_table[leaf].read(cpu_id, subleaf)
    elif leaf == 0xD:
        if subleaf == 0:
            return cpuparser.cpuids.LEAF_D.read(cpu_id, subleaf)
        elif subleaf == 1:
            return cpuparser.cpuids.LEAF_D_1.read(cpu_id, subleaf)
        else:
            return cpuparser.cpuids.LEAF_D_n.read(cpu_id, subleaf)
    elif leaf == 0xF:
        if subleaf == 0:
            return cpuparser.cpuids.LEAF_F.read(cpu_id, subleaf)
        elif subleaf == 1:
            return cpuparser.cpuids.LEAF_F_1.read(cpu_id, subleaf)
        else:
            return cpuparser.cpuids.LEAF_F_n.read(cpu_id, subleaf)
    elif leaf == 0x10:
        if subleaf == 0:
            return cpuparser.cpuids.LEAF_10.read(cpu_id, subleaf)
        elif subleaf == 1 or subleaf == 2:
            return cpuparser.cpuids.LEAF_10_1.read(cpu_id, subleaf)
        elif subleaf == 3:
            return cpuparser.cpuids.LEAF_10_3.read(cpu_id, subleaf)
        else:
            return None
    else:
        return None

def parse_cpu_ids(file):
    acc = list()
    with open(file, "r") as f:
        line = f.read().strip()
        for r in line.split(","):
            if r.find("-") > 0:
                first, last = tuple(map(int, r.split("-")))
                acc.extend(range(first, last + 1))
            else:
                if r:
                    acc.append(int(r))
    return acc

def get_online_cpu_ids():
    return parse_cpu_ids("/sys/devices/system/cpu/online")

def get_offline_cpu_ids():
    return parse_cpu_ids("/sys/devices/system/cpu/offline")
