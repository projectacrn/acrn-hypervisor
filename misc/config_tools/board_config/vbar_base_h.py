# Copyright (C) 2020-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import re
from collections import namedtuple

import acrn_config_utilities
import board_cfg_lib
import scenario_cfg_lib

VBAR_INFO_DEFINE="""#ifndef VBAR_BASE_H_
#define VBAR_BASE_H_
"""

VBAR_INFO_ENDIF="""#endif /* VBAR_BASE_H_ */"""

# Constants for ivshmem
BAR0_SHEMEM_SIZE = 4*acrn_config_utilities.SIZE_K
BAR1_SHEMEM_SIZE = 4*acrn_config_utilities.SIZE_K
BAR0_SHEMEM_ALIGNMENT = 4*acrn_config_utilities.SIZE_K
BAR1_SHEMEM_ALIGNMENT = 4*acrn_config_utilities.SIZE_K
BAR2_SHEMEM_ALIGNMENT = 2*acrn_config_utilities.SIZE_M

# Constants for pci vuart
PCI_VUART_VBAR0_SIZE = 4*acrn_config_utilities.SIZE_K
PCI_VUART_VBAR1_SIZE = 4*acrn_config_utilities.SIZE_K
PCI_VUART_VBAR0_ALIGNMENT = 4*acrn_config_utilities.SIZE_K
PCI_VUART_VBAR1_ALIGNMENT = 4*acrn_config_utilities.SIZE_K

# Constants for vmsix bar
VMSIX_VBAR_SIZE = 4*acrn_config_utilities.SIZE_K
VMSIX_VBAR_ALIGNMENT = VMSIX_VBAR_SIZE


class MmioWindow(namedtuple(
        "MmioWindow", [
            "start",
            "end"])):

    PATTERN = re.compile(r"\s*(?P<start>[0-9a-f]+)-(?P<end>[0-9a-f]+) ")

    @classmethod
    def from_str(cls, value):
        if not isinstance(value, str):
            raise ValueError("value must be a str: {}".format(type(value)))

        match = cls.PATTERN.fullmatch(value)
        if match:
            return MmioWindow(
                start=int(match.group("start"), 16),
                end=int(match.group("end"), 16))
        else:
            raise ValueError("not an mmio window: {!r}".format(value))

    def overlaps(self, other):
        if not isinstance(other, MmioWindow):
            raise TypeError('overlaps() other must be an MmioWindow: {}'.format(type(other)))
        if other.end < self.start:
            return False
        if self.end < other.start:
            return False
        return True


def get_devs_per_vm_with_key(pci_devs, keywords):
    devicelist = {}
    for vm_i, pci_devs_list in pci_devs.items():
        devicelist[vm_i] = [
            d for d in pci_devs_list if d in keywords
        ]
    return devicelist


def write_vbar(i_cnt, bdf, pci_bar_dic, bar_attr, \
    pci_devs_per_vm, mmiolist_per_vm, sos_mmio_range,config):
    """
    Parser and generate vbar
    :param i_cnt: the number of pci devices have the same PCI sub class name
    :param bdf: it is a string what contains BDF
    :param pci_bar_dic: it is a dictionary of pci vbar for those BDF
    :param bar_attr: it is a class, contains PIC bar attribute
    :param config: it is a file pointer of pci information for writing to
    """
    align = ' ' * 54
    ptdev_mmio_str = ''

    tmp_sub_name = board_cfg_lib.get_sub_pci_name(i_cnt, bar_attr)
    if bdf in pci_bar_dic.keys():
        bar_list = list(pci_bar_dic[bdf].keys())
        bar_len = len(bar_list)
        bar_num = 0
        bar_val = ""
        free = MmioWindow(0, 0)
        is_vmsix = False
        # If the device is vmsix device, find a free mmio window up to 4k size
        if board_cfg_lib.is_matched_board(('ehl-crb-b')):
            for vm_i in pci_devs_per_vm:
                if bdf in pci_devs_per_vm[vm_i]:
                    if scenario_cfg_lib.VM_DB[acrn_config_utilities.VM_TYPES[vm_i]]['load_type'] == "PRE_LAUNCHED_VM":
                        is_vmsix = True
                        bar_len += 1
                        # For pre-launched VM, the windows range is form 2G to 4G
                        free = get_free_mmio([MmioWindow(start=acrn_config_utilities.SIZE_2G, end=acrn_config_utilities.SIZE_4G-1)], \
                            mmiolist_per_vm[vm_i], VMSIX_VBAR_ALIGNMENT + VMSIX_VBAR_SIZE)
                        free_vbar_start_addr = acrn_config_utilities.round_up(free.start, VMSIX_VBAR_ALIGNMENT)
                        free_vbar_end_addr = free_vbar_start_addr + VMSIX_VBAR_SIZE - 1
                        free = MmioWindow(free_vbar_start_addr, free_vbar_end_addr)
                        mmiolist_per_vm[vm_i].append(free)
                        mmiolist_per_vm[vm_i].sort()
                        break
        for bar_i in bar_list:
            if not bar_attr.remappable:
                print("/* TODO: add {} 64bit BAR support */".format(tmp_sub_name), file=config)

            bar_num += 1
            bar_val = pci_bar_dic[bdf][bar_i].addr
            if pci_bar_dic[bdf][bar_i].remapped:
                ptdev_mmio_str = 'HI_MMIO_START + '

            if bar_num == bar_len:
                if bar_len == 1:
                    print("#define %-38s" % (tmp_sub_name+"_VBAR"), "       .vbar_base[{}] = {}{}UL" \
                            .format(bar_i, ptdev_mmio_str, bar_val), file=config)
                else:
                    print("{}.vbar_base[{}] = {}{}UL" \
                            .format(align, bar_i, ptdev_mmio_str, bar_val), file=config)
            elif bar_num == 1:
                print("#define %-38s" % (tmp_sub_name+"_VBAR"), "       .vbar_base[{}] = {}{}UL, \\".format(bar_i, ptdev_mmio_str, bar_val), file=config)
            else:
                print("{}.vbar_base[{}] = {}{}UL, \\".format(align, bar_i, ptdev_mmio_str, bar_val), file=config)
        if is_vmsix:
            next_bar_idx = find_next_bar(bar_val, bar_list)
            print("{}.vbar_base[{}] = {}{}UL".format(align, next_bar_idx, ptdev_mmio_str, hex(free.start)), file=config)
        print("", file=config)


def find_next_bar(bar_val, bar_list):
    pci_lines = board_cfg_lib.get_info(acrn_config_utilities.BOARD_INFO_FILE, "<PCI_DEVICE>", "</PCI_DEVICE>")
    idx = bar_list[-1]
    for line in pci_lines:
        if bar_val.split('x')[1] in line:
            if "32-bit" in line:
                idx += 1
                break
            elif "64-bit" in line:
                idx += 2
                break
    if int(idx) > 5:
        raise ValueError("Not enough bar region, last bar region is {}".format(idx))
    return idx


def write_vuart_vbar(mmiolist_per_vm, sos_mmio_range, config):
    # get legacy vuart information
    vuart0_setting = acrn_config_utilities.get_vuart_info_id(acrn_config_utilities.SCENARIO_INFO_FILE, 0)
    vuart1_setting = acrn_config_utilities.get_vuart_info_id(acrn_config_utilities.SCENARIO_INFO_FILE, 1)
    # get pci vuart information
    vuarts = acrn_config_utilities.get_vuart_info(acrn_config_utilities.SCENARIO_INFO_FILE)
    for vm_id in vuarts.keys():
        vm_type = acrn_config_utilities.VM_TYPES[vm_id]
        if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "POST_LAUNCHED_VM":
            continue
        for vuart_id in vuarts[vm_id].keys():
            if vuarts[vm_id][vuart_id]['base'] == "INVALID_PCI_BASE":
                continue
            # Skip pci vuart 0 if the legacy vuart 0 is enabled
            if vuart_id == 0 and vm_id in vuart0_setting \
                 and vuart0_setting[vm_id]['base'] != "INVALID_COM_BASE":
                continue
            # Skip pci vuart 1 if the legacy vuart 1 is enabled
            if vuart_id == 1 and vm_id in vuart1_setting \
                 and vuart1_setting[vm_id]['base'] != "INVALID_COM_BASE":
                continue
            free_bar0 = []
            free_bar1 = []
            # vuart decice requires 2 bars
            if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "SERVICE_VM":
                free_bar0 = get_free_mmio(sos_mmio_range, mmiolist_per_vm[vm_id], \
                             PCI_VUART_VBAR0_SIZE + PCI_VUART_VBAR0_ALIGNMENT)
                free_bar0_start_addr = acrn_config_utilities.round_up(free_bar0.start, PCI_VUART_VBAR0_ALIGNMENT)
                free_bar0_end_addr = free_bar0_start_addr + PCI_VUART_VBAR0_SIZE - 1
                free_bar0 = MmioWindow(free_bar0_start_addr, free_bar0_end_addr)
                mmiolist_per_vm[vm_id].append(free_bar0)
                mmiolist_per_vm[vm_id].sort()
                free_bar1 = get_free_mmio(sos_mmio_range, mmiolist_per_vm[vm_id], \
                             PCI_VUART_VBAR1_SIZE + PCI_VUART_VBAR1_ALIGNMENT)
                free_bar1_start_addr = acrn_config_utilities.round_up(free_bar1.start, PCI_VUART_VBAR1_ALIGNMENT)
                free_bar1_end_addr = free_bar1_start_addr + PCI_VUART_VBAR1_SIZE - 1
                free_bar1 = MmioWindow(free_bar1_start_addr, free_bar1_end_addr)
                mmiolist_per_vm[vm_id].append(free_bar1)
                mmiolist_per_vm[vm_id].sort()
            elif scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
                free_bar0 = get_free_mmio([MmioWindow(start=acrn_config_utilities.SIZE_2G, end=acrn_config_utilities.SIZE_4G-1)], \
                                mmiolist_per_vm[vm_id], PCI_VUART_VBAR0_SIZE + PCI_VUART_VBAR0_ALIGNMENT)
                free_bar0_start_addr = acrn_config_utilities.round_up(free_bar0.start, PCI_VUART_VBAR0_ALIGNMENT)
                free_bar0_end_addr = free_bar0_start_addr + PCI_VUART_VBAR0_SIZE - 1
                free_bar0 = MmioWindow(free_bar0_start_addr, free_bar0_end_addr)
                mmiolist_per_vm[vm_id].append(free_bar0)
                mmiolist_per_vm[vm_id].sort()
                free_bar1 = get_free_mmio([MmioWindow(start=acrn_config_utilities.SIZE_2G, end=acrn_config_utilities.SIZE_4G-1)], \
                                mmiolist_per_vm[vm_id], PCI_VUART_VBAR1_SIZE + PCI_VUART_VBAR1_ALIGNMENT)
                free_bar1_start_addr = acrn_config_utilities.round_up(free_bar1.start, PCI_VUART_VBAR1_ALIGNMENT)
                free_bar1_end_addr = free_bar1_start_addr + PCI_VUART_VBAR1_SIZE - 1
                free_bar1 = MmioWindow(free_bar1_start_addr, free_bar1_end_addr)
                mmiolist_per_vm[vm_id].append(free_bar1)
                mmiolist_per_vm[vm_id].sort()
            print("#define VM%s" %(str(vm_id) + "_VUART_%-28s") % (str(vuart_id) + "_VBAR"),
                  "       .vbar_base[0] = {:#x}UL, \\".format(free_bar0.start), file=config)
            print("{}.vbar_base[1] = {:#x}UL".format(' ' * 54, free_bar1.start), file=config)
            print("", file=config)


def write_ivshmem_vbar(mmiolist_per_vm, sos_mmio_range, config):
    for vm_id,vm_type in acrn_config_utilities.VM_TYPES.items():
        ivshmem_region = acrn_config_utilities.get_hv_item_tag(acrn_config_utilities.SCENARIO_INFO_FILE,
                    "FEATURES", "IVSHMEM", "IVSHMEM_REGION")
        shmem_regions = scenario_cfg_lib.get_shmem_regions(ivshmem_region)
        if vm_id not in shmem_regions:
            continue

        shmems = shmem_regions.get(vm_id)
        idx = 0
        for shm in shmems:
            if shm is None or shm.strip() == '':
                continue

            shm_splited = shm.split(',')
            size = shm_splited[1].strip()
            try:
                int_size = int(size) * 0x100000
            except:
                int_size = 0
            if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "SERVICE_VM":
                # vbar[0] for shared memory is 4k
                free_bar0 = get_free_mmio(sos_mmio_range, mmiolist_per_vm[vm_id], BAR0_SHEMEM_ALIGNMENT + BAR0_SHEMEM_SIZE)
                free_bar0_start_addr = acrn_config_utilities.round_up(free_bar0.start, BAR0_SHEMEM_ALIGNMENT)
                free_bar0_end_addr = free_bar0_start_addr + BAR0_SHEMEM_SIZE - 1
                free_bar0 = MmioWindow(free_bar0_start_addr, free_bar0_end_addr)
                mmiolist_per_vm[vm_id].append(free_bar0)
                mmiolist_per_vm[vm_id].sort()
                # vbar[1] for shared memory is 4K
                free_bar1 = get_free_mmio(sos_mmio_range, mmiolist_per_vm[vm_id], BAR1_SHEMEM_ALIGNMENT + BAR1_SHEMEM_SIZE)
                free_bar1_start_addr = acrn_config_utilities.round_up(free_bar1.start, BAR1_SHEMEM_ALIGNMENT)
                free_bar1_end_addr = free_bar1_start_addr + BAR1_SHEMEM_SIZE - 1
                free_bar1 = MmioWindow(free_bar1_start_addr, free_bar1_end_addr)
                mmiolist_per_vm[vm_id].append(free_bar1)
                mmiolist_per_vm[vm_id].sort()
                # vbar[2] for shared memory is specified size in MB
                free_bar2 = get_free_mmio(sos_mmio_range, mmiolist_per_vm[vm_id], BAR2_SHEMEM_ALIGNMENT + int_size)
                free_bar2_start_addr = acrn_config_utilities.round_up(free_bar2.start, BAR2_SHEMEM_ALIGNMENT) + 0xC
                free_bar2_end_addr = free_bar2_start_addr + int_size - 1
                free_bar2 = MmioWindow(free_bar2_start_addr, free_bar2_end_addr)
                mmiolist_per_vm[vm_id].append(free_bar2)
                mmiolist_per_vm[vm_id].sort()
                print("#define SOS_IVSHMEM_DEVICE_%-19s" % (str(idx) + "_VBAR"),
                        "       .vbar_base[0] = {:#x}UL, \\".format(free_bar0.start), file=config)
                print("{}.vbar_base[1] = {:#x}UL, \\".format(' ' * 54, free_bar1.start), file=config)
                print("{}.vbar_base[2] = {:#x}UL".format(' ' * 54, free_bar2.start), file=config)
                print("", file=config)
                idx += 1
            elif scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
                mmioRange = [MmioWindow(start=acrn_config_utilities.SIZE_2G, end=acrn_config_utilities.SIZE_4G-1)]
                # vbar[0] for shared memory is 4k
                free_bar0 = get_free_mmio(mmioRange, mmiolist_per_vm[vm_id], BAR0_SHEMEM_ALIGNMENT + BAR0_SHEMEM_SIZE)
                free_bar0_start_addr = acrn_config_utilities.round_up(free_bar0.start, BAR0_SHEMEM_ALIGNMENT)
                free_bar0_end_addr = free_bar0_start_addr + BAR0_SHEMEM_SIZE - 1
                free_bar0 = MmioWindow(free_bar0_start_addr, free_bar0_end_addr)
                mmiolist_per_vm[vm_id].append(free_bar0)
                mmiolist_per_vm[vm_id].sort()
                # vbar[1] for shared memory is 4K
                free_bar1 = get_free_mmio(mmioRange, mmiolist_per_vm[vm_id], BAR1_SHEMEM_ALIGNMENT + BAR1_SHEMEM_SIZE)
                free_bar1_start_addr = acrn_config_utilities.round_up(free_bar1.start, BAR1_SHEMEM_ALIGNMENT)
                free_bar1_end_addr = free_bar1_start_addr + BAR1_SHEMEM_SIZE - 1
                free_bar1 = MmioWindow(free_bar1_start_addr, free_bar1_end_addr)
                mmiolist_per_vm[vm_id].append(free_bar1)
                mmiolist_per_vm[vm_id].sort()
                # vbar[2] for shared memory is specified size in MB
                free_bar2 = get_free_mmio(mmioRange, mmiolist_per_vm[vm_id], BAR2_SHEMEM_ALIGNMENT + int_size)
                free_bar2_start_addr = acrn_config_utilities.round_up(free_bar2.start, BAR2_SHEMEM_ALIGNMENT) + 0xC
                free_bar2_end_addr = free_bar2_start_addr + int_size - 1
                free_bar2 = MmioWindow(free_bar2_start_addr, free_bar2_end_addr)
                mmiolist_per_vm[vm_id].append(free_bar2)
                mmiolist_per_vm[vm_id].sort()
                print("#define IVSHMEM_DEVICE_%-23s" % (str(idx) + "_VBAR"),
                        "       .vbar_base[0] = {:#x}UL, \\".format(free_bar0.start), file=config)
                print("{}.vbar_base[1] = {:#x}UL, \\".format(' ' * 54, free_bar1.start), file=config)
                print("{}.vbar_base[2] = {:#x}UL".format(' ' * 54, free_bar2.start), file=config)
                print("", file=config)
                idx += 1



def is_mmio_window_used(devinfo, keywords):
    for k in keywords:
        if k in devinfo:
            return True
    return False


def get_mmio_windows_with_key(keywords):
    keyword_mmiolist = []
    exclusive_mmiolist = []
    iomem_lines = board_cfg_lib.get_info(acrn_config_utilities.BOARD_INFO_FILE, "<IOMEM_INFO>", "</IOMEM_INFO>")

    for line in iomem_lines:
        mmio_range = line.split(':')[0]
        devmmio_tuple = MmioWindow.from_str(mmio_range)
        if is_mmio_window_used(line, keywords):
            keyword_mmiolist.append(devmmio_tuple)
        else:
            exclusive_mmiolist.append(devmmio_tuple)
    return sorted(keyword_mmiolist), sorted(exclusive_mmiolist)


def removed_nested(list1, list2):
    if not list1 or not list2:
        return list1

    resolvedList = list1[:]
    for w1 in resolvedList:
        for w2 in list2:
            if w2.start <= w1.start <= w2.end and w2.start <= w1.end <= w2.end:
                if w1 not in resolvedList:
                    continue
                resolvedList.remove(w1)
    return sorted(resolvedList)


def merged_windows(windowslist):
    if not windowslist:
        return None

    sortedlist = sorted(windowslist)
    resolvedList = []
    last = sortedlist[0]
    for cur in sortedlist:
        if cur.start <= last.end + 1:
            last = MmioWindow(start=last.start, end=max(last.end, cur.end))
        else:
            resolvedList.append(last)
            last = cur
    resolvedList.append(last)
    return sorted(resolvedList)


def get_free_mmio(windowslist, used, size):
    if not size:
        raise ValueError("allocate size cannot be {}".format(size))
    if not windowslist:
        raise ValueError("No mmio range is specified:{}".format(windowslist))
    for w in windowslist:
        window = MmioWindow(start=w.start, end=w.start+size-1)
        for u in used:
            if window.overlaps(u):
                window = MmioWindow(start=u.end+1, end=u.end+size)
                continue
        if window.overlaps(w):
            return window
    raise ValueError("Not enough mmio window for a device size {}: {}".format(size, window))


def generate_file(config):
    matching_mmios, non_matching_mmios = get_mmio_windows_with_key(['PCI Bus 0000:00'])
    matching_mmios = removed_nested(matching_mmios, non_matching_mmios)
    non_matching_mmios = [
        w for w in non_matching_mmios
        if any((w.overlaps(w2) for w2 in matching_mmios))
        ]
    non_matching_mmios = merged_windows(non_matching_mmios)

    # list of all vmsix supported device list in bdf format
    bdf_list = board_cfg_lib.get_known_caps_pci_devs().get('VMSIX', [])
    # list of all PRE_LAUNCHED_VMs' vmsix supported passthrough devices in bdf format
    pci_items = acrn_config_utilities.get_leaf_tag_map(acrn_config_utilities.SCENARIO_INFO_FILE, "pci_devs", "pci_dev")
    pci_devs = scenario_cfg_lib.get_pt_pci_devs(pci_items)
    pci_devs_per_vm = get_devs_per_vm_with_key(pci_devs, bdf_list)
    # list Service VM vmsix supported devices without other PRE_LAUNCHED_VMs' in bdf format
    sos_bdf_list = [
        d for d in bdf_list
        if all((d not in pci_devs_per_vm[i] for i in pci_devs_per_vm))
        ]

    for vm_i in pci_devs_per_vm:
        vm_type = acrn_config_utilities.VM_TYPES[vm_i]
        if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "SERVICE_VM":
            pci_devs_per_vm[vm_i] = sos_bdf_list

    mmiolist_per_vm = {}
    for vm_i,vm_type in acrn_config_utilities.VM_TYPES.items():
        if vm_i not in mmiolist_per_vm.keys():
            mmiolist_per_vm[vm_i] = []
        if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "SERVICE_VM":
            mmiolist_per_vm[vm_i] = non_matching_mmios
        else:
            if vm_i in pci_devs.keys():
                match, _ = get_mmio_windows_with_key(pci_devs[vm_i])
                mmiolist_per_vm[vm_i] = match
            if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
                if vm_i not in mmiolist_per_vm.keys():
                    mmiolist_per_vm[vm_i] = []
                # TSN reserved region
                mmiolist_per_vm[vm_i].append(MmioWindow(start = 0xffff0000 , end = 0xffffffff))
                # For the pre-launched vm, if the TPM is passtrough, this address is used
                if vm_i == 0 and board_cfg_lib.is_tpm_passthru():
                    mmiolist_per_vm[vm_i].append(MmioWindow(start = 0xfed40000, end = 0xfed40000 + 0x5000 - 1))
                # For the pre-launched vm o ehl-crb-b, if the p2sb is passtrough, this address is used
                if board_cfg_lib.is_matched_board(('ehl-crb-b')):
                    p2sb_start = board_cfg_lib.find_p2sb_bar_addr()
                    mmiolist_per_vm[vm_i].append(MmioWindow(start = p2sb_start, end = p2sb_start + 0x1000000 - 1))
                mmiolist_per_vm[vm_i].sort()

    # start to generate board_info.h
    print("{0}".format(board_cfg_lib.HEADER_LICENSE), file=config)
    print(VBAR_INFO_DEFINE, file=config)
    acrn_config_utilities.get_vm_types()
    pre_vm = False
    sos_vm = False
    for vm_type in acrn_config_utilities.VM_TYPES.values():
        if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
            pre_vm = True
        if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "SERVICE_VM":
            sos_vm = True

    if not pre_vm and not sos_vm:
        print(VBAR_INFO_ENDIF, file=config)
        return

    ivshmem_enabled = acrn_config_utilities.get_hv_item_tag(acrn_config_utilities.SCENARIO_INFO_FILE, "FEATURES", "IVSHMEM", "IVSHMEM_ENABLED")
    if ivshmem_enabled == 'y':
        write_ivshmem_vbar(mmiolist_per_vm, matching_mmios, config)

    # Get passthrough devices vbar bases
    compared_bdf = []
    for cnt_sub_name in board_cfg_lib.SUB_NAME_COUNT.keys():
        i_cnt = 0
        for bdf, bar_attr in board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic.items():
            if cnt_sub_name == bar_attr.name and bdf not in compared_bdf:
                compared_bdf.append(bdf)
            else:
                continue

            write_vbar(i_cnt, bdf, board_cfg_lib.PCI_DEV_BAR_DESC.pci_bar_dic, bar_attr, \
                pci_devs_per_vm, mmiolist_per_vm, matching_mmios, config)

            i_cnt += 1

    write_vuart_vbar(mmiolist_per_vm, matching_mmios, config)
    print(VBAR_INFO_ENDIF, file=config)
