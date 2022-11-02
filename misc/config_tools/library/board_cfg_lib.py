# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import re
import sys
import acrn_config_utilities
import collections

BOARD_NAME = ''
BIOS_INFO = ['BIOS Information', 'Vendor:', 'Version:', 'Release Date:', 'BIOS Revision:']
BASE_BOARD = ['Base Board Information', 'Manufacturer:', 'Product Name:', 'Version:']

LEGACY_TTYS = {
    'ttyS0':'0x3F8',
    'ttyS1':'0x2F8',
    'ttyS2':'0x3E8',
    'ttyS3':'0x2E8',
}

VALID_LEGACY_IRQ = []
ERR_LIST = {}
USED_RAM_RANGE = {}

HEADER_LICENSE = acrn_config_utilities.open_license() + "\n"

# The data base contains hide pci device
KNOWN_HIDDEN_PDEVS_BOARD_DB = {
    'apl-mrb':['00:0d:0'],
    'apl-up2':['00:0d:0'],
}

TSN_DEVS = ["8086:4b30", "8086:4b31", "8086:4b32", "8086:4ba0", "8086:4ba1", "8086:4ba2",
            "8086:4bb0", "8086:4bb1", "8086:4bb2", "8086:a0ac", "8086:43ac", "8086:43a2"]
GPIO_DEVS = ["8086:4b88", "8086:4b89"]
TPM_PASSTHRU_BOARD = ['whl-ipc-i5', 'whl-ipc-i7', 'tgl-rvp', 'ehl-crb-b', 'cfl-k700-i7']

KNOWN_CAPS_PCI_DEVS_DB = {
    "VMSIX":TSN_DEVS + GPIO_DEVS,
}

P2SB_PASSTHRU_BOARD = ('ehl-crb-b')

def get_info(board_info, msg_s, msg_e):
    """
    Get information which specify by argument
    :param board_info: it is a file what contains information for script to read from
    :param msg_s: it is a pattern of key stings what start to match from board information
    :param msg_e: it is a pattern of key stings what end to match from board information
    """
    info_start = False
    info_end = False
    info_lines = []
    num = len(msg_s.split())

    with open(board_info, 'rt') as f_board:
        while True:

            line = f_board.readline()
            if not line:
                break

            if " ".join(line.split()[0:num]) == msg_s:
                info_start = True
                info_end = False
                continue

            if " ".join(line.split()[0:num]) == msg_e:
                info_start = False
                info_end = True
                continue

            if info_start and not info_end:
                info_lines.append(line)
                continue

            if not info_start and info_end:
                return info_lines


def handle_bios_info(config):
    """
    Handle bios information
    :param config: it is a file pointer of bios information for writing to
    """
    bios_lines = get_info(acrn_config_utilities.BOARD_INFO_FILE, "<BIOS_INFO>", "</BIOS_INFO>")
    board_lines = get_info(acrn_config_utilities.BOARD_INFO_FILE, "<BASE_BOARD_INFO>", "</BASE_BOARD_INFO>")
    print("/*", file=config)

    if not bios_lines or not board_lines:
        print(" * DMI info is not found", file=config)
    else:
        i_cnt = 0
        bios_board = BIOS_INFO + BASE_BOARD

        # remove the same value and keep origin sort
        bios_board_info = list(set(bios_board))
        bios_board_info.sort(key=bios_board.index)

        bios_board_lines = bios_lines + board_lines
        bios_info_len = len(bios_lines)
        for line in bios_board_lines:
            if i_cnt == bios_info_len:
                print(" *", file=config)

            i_cnt += 1

            for misc_info in bios_board_info:
                if misc_info == " ".join(line.split()[0:1]) or misc_info == \
                        " ".join(line.split()[0:2]) or misc_info == " ".join(line.split()[0:3]):
                    print(" * {0}".format(line.strip()), file=config)

    print(" */", file=config)


def handle_root_dev(line):
    """Handle if it match root device information pattern
    :param line: one line of information which had decoded to 'ASCII'
    """
    for root_type in line.split():
        # only support ext4 rootfs
        if "ext4" in root_type:
            return True

    return False


def get_max_clos_mask(board_file):
    """
    Parse CLOS information
    :param board_file: it is a file what contains board information for script to read from
    :return: type of rdt resource supported and their corresponding clos max.
    """
    rdt_res=[]
    rdt_res_clos_max=[]
    rdt_res_mask_max=[]

    clos_lines = get_info(board_file, "<CLOS_INFO>", "</CLOS_INFO>")
    for line in clos_lines:
        if line.split(':')[0].strip() == "rdt resources supported":
            rdt_res = line.split(':')[1].strip()
        elif line.split(':')[0].strip() == "rdt resource clos max":
            rdt_res_clos_max = line.split(':')[1].strip()
        elif line.split(':')[0].strip() == "rdt resource mask max":
            rdt_res_mask_max = line.split(':')[1].strip()

    if (len(rdt_res) == 0) or (len(rdt_res_clos_max) == 0):
        return rdt_res, rdt_res_clos_max, rdt_res_mask_max
    else:
        return list(re.split(', |\s |,', rdt_res)), list(map(int, rdt_res_clos_max.split(','))), list(re.split(', |\s |,', rdt_res_mask_max))


def get_rootfs(config_file):
    """
    This will get rootfs partition from board information
    :param config_file: it is a file which contain board information
    :return: rootfs partition list
    """
    root_dev_list = []
    rootfs_lines = get_info(config_file, "<BLOCK_DEVICE_INFO>", "</BLOCK_DEVICE_INFO>")

    # none 'BLOCK_DEVICE_INFO' tag
    if rootfs_lines == None:
        return root_dev_list

    for rootfs_line in rootfs_lines:
        if not rootfs_line:
            break

        if not handle_root_dev(rootfs_line):
            continue

        root_dev = rootfs_line.strip().split(':')[0]
        root_dev_list.append(root_dev)

    return (root_dev_list, len(root_dev_list))


def clos_info_parser(board_info):
    """ Parse CLOS information """
    return get_max_clos_mask(board_info)


def get_valid_irq(board_info):
    """
     This is get available irq from board info file
     :param board_info:  it is a file what contains board information for script to read from
     :return: None
     """
    global VALID_LEGACY_IRQ
    val_irq = []
    irq_info_lines = get_info(board_info, "<AVAILABLE_IRQ_INFO>", "</AVAILABLE_IRQ_INFO>")
    for irq_string in irq_info_lines:
        val_irq = [x.strip() for x in irq_string.split(',')]

    VALID_LEGACY_IRQ = val_irq


def alloc_irq():
    """
      This is allocated an available irq
      :return: free irq
      """
    irq_val = VALID_LEGACY_IRQ.pop(0)

    return irq_val


def parser_hv_console():
    """
    There may be 3 types in the console item
    1. BDF:(00:18.2) seri:/dev/ttyS2
    2. /dev/ttyS2
    3. ttyS2
    """
    ttys_n = ''
    err_dic = {}
    ttys = acrn_config_utilities.get_hv_item_tag(acrn_config_utilities.SCENARIO_INFO_FILE, "DEBUG_OPTIONS", "SERIAL_CONSOLE")

    if not ttys or ttys == None:
        return (err_dic, ttys_n)

    if ttys and 'BDF' in ttys or '/dev' in ttys:
        ttys_n = ttys.split('/')[2]
    else:
        ttys_n = ttys

    return (err_dic, ttys_n)


def get_processor_info():
    """
    Get cpu processor list
    :param board_info: it is a file what contains board information
    :return: cpu processor list
    """
    processor_list = []
    tmp_list = []
    processor_info = get_info(acrn_config_utilities.BOARD_INFO_FILE, "<CPU_PROCESSOR_INFO>", "</CPU_PROCESSOR_INFO>")

    if not processor_info:
        key = "CPU PROCESSOR_INFO error:"
        ERR_LIST[key] = "CPU core is not exists"
        return processor_list

    for processor_line in processor_info:
        if not processor_line:
            break

        processor_list = processor_line.strip().split(',')
        for processor in processor_list:
            tmp_list.append(processor.strip())
        break

    return tmp_list


def get_native_ttys_info(board_info):
    """
    Get ttySn from board info
    :param board_info: it is a file what contains board information for script to read from
    :return: serial port list
    """
    ttys_list = []
    ttys_info = get_info(board_info, "<TTYS_INFO>", "</TTYS_INFO>")

    for ttys_line in ttys_info:
        if not ttys_line:
            break

        ttys_dev = ttys_line.split()[0].split(':')[1]
        ttysn = ttys_dev.split('/')[-1]
        # currently Service VM OS console can only support legacy serial port
        if ttysn not in list(LEGACY_TTYS.keys()):
            continue
        ttys_list.append(ttys_dev)

    return ttys_list

def get_total_mem():
    """
    get total memory size from config file which is dumped from native board
    :return: integer number of total memory size, Unit: MByte
    """
    scale_to_mb = 1
    total_mem_mb = scale_to_mb
    mem_lines = get_info(acrn_config_utilities.BOARD_INFO_FILE, "<TOTAL_MEM_INFO>", "</TOTAL_MEM_INFO>")
    for mem_line in mem_lines:
        mem_info_list = mem_line.split()

    if len(mem_info_list) <= 1:
        return total_mem_mb

    if mem_info_list[1] == "kB":
        scale_to_mb = 1024

    total_mem_mb = int(mem_info_list[0]) / scale_to_mb
    return total_mem_mb


def get_pci_info(board_info):
    pci_bdf_vpid = {}
    pci_vid_start = False
    pci_vid_end = False
    pci_desc = {}
    pci_start = False
    pci_end = False

    with open(board_info, 'r') as f:
        while True:
            line = f.readline()
            if not line:
                break

            s = " "
            if s.join(line.split()[0:2]) == "<PCI_DEVICE>":
                pci_start = True
                pci_end = False
                continue

            if s.join(line.split()[0:2]) == "</PCI_DEVICE>":
                pci_start = False
                pci_end = True
                continue

            # all pci device wiht description
            if pci_start and not pci_end:
                if "Region" in line and "Memory at" in line:
                    continue
                bdf = line.split()[0]
                pci_desc[bdf] = line

            if s.join(line.split()[0:2]) == "<PCI_VID_PID>":
                pci_vid_start = True
                pci_vid_end = False
                continue

            if s.join(line.split()[0:2]) == "</PCI_VID_PID>":
                pci_vid_start = False
                pci_vid_end = True
                continue

            # all pci device with vid/pid and bdf
            if pci_vid_start and not pci_vid_end:
                bdf_str = line.split()[0]
                vid_pid = line.split()[2]
                pci_bdf_vpid[bdf_str] = vid_pid

    return (pci_desc, pci_bdf_vpid)

def get_p_state_count():
    """
    Get cpu p-state count
    :return: p-state count
    """
    px_info = get_info(acrn_config_utilities.BOARD_INFO_FILE, "<PX_INFO>", "</PX_INFO>")
    if px_info != None:
        for line in px_info:
            if re.search("{.*}", line) == None:
                px_info.remove(line)

    return len(px_info)

def get_p_state_index_from_ratio(ratio):
    """
    Get the closest p-state index that is lesser than or equel to given ratio
    :return: p-state index; If no px_info found in board file, return 0;
    """
    closest_index = 0
    px_info = get_info(acrn_config_utilities.BOARD_INFO_FILE, "<PX_INFO>", "</PX_INFO>")
    if px_info != None:
        for line in px_info:
            if re.search("{.*}", line) == None:
                px_info.remove(line)

        i = 0
        closest_index = 1
        for line in px_info:
            l = re.search("0x(\w*)UL}", line)
            if l != None:
                state_ratio = int(l.group(1), 16) >> 8
                if state_ratio <= ratio:
                    closest_index = i
                    break
            i += 1

    return closest_index

HI_MMIO_OFFSET = 0

class Bar_Mem:
    def __init__(self):
        self.addr = 0
        self.remapped = False


class Bar_Attr:
    def __init__(self):
        self.name = 0
        self.remappable = True
        self.name_w_i_cnt = 0

class Pci_Dev_Bar_Desc:
    def __init__(self):
        self.pci_dev_dic = collections.OrderedDict()
        self.pci_bar_dic = collections.OrderedDict()
        self.shm_bar_dic = collections.OrderedDict()

PCI_DEV_BAR_DESC = Pci_Dev_Bar_Desc()
SUB_NAME_COUNT = collections.OrderedDict()


def get_value_after_str(line, key):
    """ Get the value after cstate string """
    idx = 0
    line_in_list = line.split()
    for idx_key, val in enumerate(line_in_list):
        if val == key:
            idx = idx_key
            break

    return line_in_list[idx + 1]


def check_bar_remappable(line):
    #TODO: check device BAR remappable per ACPI table

    return True


def get_size(line):

    # get size string from format, Region n: Memory at x ... [size=NK]
    size_str = line.split()[-1].strip(']').split('=')[1]
    if 'G' in size_str:
        size = int(size_str.strip('G')) * acrn_config_utilities.SIZE_G
    elif 'M' in size_str:
        size = int(size_str.strip('M')) * acrn_config_utilities.SIZE_M
    elif 'K' in size_str:
        size = int(size_str.strip('K')) * acrn_config_utilities.SIZE_K
    else:
        size = int(size_str)

    return size

# round up the running bar_addr to the size of the incoming bar "line"
def remap_bar_addr_to_high(bar_addr, line):
    """Generate vbar address"""
    global HI_MMIO_OFFSET
    size = get_size(line)
    cur_addr = acrn_config_utilities.round_up(bar_addr, size)
    HI_MMIO_OFFSET = cur_addr + size
    return cur_addr


def parser_pci():
    """ Parse PCI lines """
    global SUB_NAME_COUNT, HI_MMIO_OFFSET
    cur_bdf = 0
    prev_bdf = 0
    tmp_bar_dic = {}
    bar_addr = bar_num = '0'
    cal_sub_pci_name = []

    pci_lines = get_info(acrn_config_utilities.BOARD_INFO_FILE, "<PCI_DEVICE>", "</PCI_DEVICE>")

    for line in pci_lines:
        tmp_bar_mem = Bar_Mem()
        # get pci bar information into board_cfg_lib.PCI_DEV_BAR_DESC
        if "Region" in line and "Memory at" in line:
            #ignore memory region from SR-IOV capabilities
            if "size=" not in line:
                 continue

            try:
                bar_addr = int(get_value_after_str(line, "at"), 16)
            except ValueError:
                continue

            bar_num = line.split()[1].strip(':')
            if bar_addr >= acrn_config_utilities.SIZE_4G or bar_addr < acrn_config_utilities.SIZE_2G:
                if not tmp_bar_attr.remappable:
                    continue

                bar_addr = remap_bar_addr_to_high(HI_MMIO_OFFSET, line)
                tmp_bar_mem.remapped = True

            tmp_bar_mem.addr = hex(bar_addr)
            tmp_bar_dic[int(bar_num)] = tmp_bar_mem
        else:
            tmp_bar_attr = Bar_Attr()
            prev_bdf = cur_bdf
            pci_bdf = line.split()[0]
            tmp_bar_attr.name = " ".join(line.split(':')[1].split()[1:])

            # remove '[*]' in pci subname
            if '[' in tmp_bar_attr.name:
                tmp_bar_attr.name = tmp_bar_attr.name.rsplit('[', 1)[0]

            cal_sub_pci_name.append(tmp_bar_attr.name)
            tmp_bar_attr.remappable = check_bar_remappable(line)
            PCI_DEV_BAR_DESC.pci_dev_dic[pci_bdf] = tmp_bar_attr
            cur_bdf = pci_bdf

            if not prev_bdf:
                prev_bdf = cur_bdf

            if tmp_bar_dic and cur_bdf != prev_bdf:
                PCI_DEV_BAR_DESC.pci_bar_dic[prev_bdf] = tmp_bar_dic

            # clear the tmp_bar_dic before store the next dic
            tmp_bar_dic = {}

    # output all the pci device list to pci_device.h
    for item in cal_sub_pci_name:
        SUB_NAME_COUNT[item] = SUB_NAME_COUNT.get(item, 0) + 1

    if tmp_bar_dic:
        PCI_DEV_BAR_DESC.pci_bar_dic[cur_bdf] = tmp_bar_dic


def parse_mem():
    raw_shmem_regions = acrn_config_utilities.get_hv_item_tag(acrn_config_utilities.SCENARIO_INFO_FILE, "FEATURES", "IVSHMEM", "IVSHMEM_REGION")

    global USED_RAM_RANGE
    for shm_name, shm_bar_dic in PCI_DEV_BAR_DESC.shm_bar_dic.items():
        if 0 in shm_bar_dic.keys() and int(shm_bar_dic[0].addr, 16) in USED_RAM_RANGE.keys():
            del USED_RAM_RANGE[int(shm_bar_dic[0].addr, 16)]
        if 2 in shm_bar_dic.keys() and int(shm_bar_dic[2].addr, 16)-0xC in USED_RAM_RANGE.keys():
            del USED_RAM_RANGE[int(shm_bar_dic[2].addr, 16)-0xC]

    idx = 0
    for shm in raw_shmem_regions:
        if shm is None or shm.strip() == '':
            continue
        shm_splited = shm.split(',')
        name = shm_splited[0].strip()
        size = shm_splited[1].strip()

        try:
            int_size = int(size) * 0x100000
        except:
            int_size = 0
        ram_range = get_ram_range()
        tmp_bar_dict  = {}
        hv_start_offset = 0x80000000
        ret_start_addr = find_avl_memory(ram_range, str(0x200100), hv_start_offset)
        bar_mem_0 = Bar_Mem()
        bar_mem_0.addr = hex(acrn_config_utilities.round_up(int(ret_start_addr, 16), 0x200000))
        USED_RAM_RANGE[int(bar_mem_0.addr, 16)] = 0x100
        tmp_bar_dict[0] = bar_mem_0
        ram_range = get_ram_range()
        hv_start_offset2 = 0x100000000
        ret_start_addr2 = find_avl_memory(ram_range, str(int_size + 0x200000), hv_start_offset2)
        bar_mem_2 = Bar_Mem()
        bar_mem_2.addr = hex(acrn_config_utilities.round_up(int(ret_start_addr2, 16), 0x200000) + 0xC)
        USED_RAM_RANGE[acrn_config_utilities.round_up(int(ret_start_addr2, 16), 0x20000)] = int_size
        tmp_bar_dict[2] = bar_mem_2
        PCI_DEV_BAR_DESC.shm_bar_dic[str(idx)+'_'+name] = tmp_bar_dict
        idx += 1


def is_rdt_supported():
    """
    Returns True if platform supports RDT else False
    """
    (rdt_resources, rdt_res_clos_max, _) = clos_info_parser(acrn_config_utilities.BOARD_INFO_FILE)
    if len(rdt_resources) == 0 or len(rdt_res_clos_max) == 0:
        return False
    else:
        return True


def is_rdt_enabled():
    """
    Returns True if RDT enabled else False
    """
    rdt_enabled = acrn_config_utilities.get_hv_item_tag(acrn_config_utilities.SCENARIO_INFO_FILE, "FEATURES", "RDT", "RDT_ENABLED")
    if is_rdt_supported() and rdt_enabled == 'y':
        return True
    return False


def is_cdp_enabled():
    """
    Returns True if platform supports RDT/CDP else False
    """
    rdt_enabled = is_rdt_enabled()
    cdp_enabled = acrn_config_utilities.get_hv_item_tag(acrn_config_utilities.SCENARIO_INFO_FILE, "FEATURES", "RDT", "CDP_ENABLED")
    if rdt_enabled and cdp_enabled == 'y':
        return True

    return False


def get_rdt_select_opt():

    support_sel = ['n']
    if is_rdt_supported():
        support_sel.append('y')
    return support_sel


def get_common_clos_max():

    common_clos_max = 0
    (res_info, rdt_res_clos_max, clos_max_mask_list) = clos_info_parser(acrn_config_utilities.BOARD_INFO_FILE)
    if is_rdt_enabled() and not is_cdp_enabled():
        common_clos_max = min(rdt_res_clos_max)

    if is_cdp_enabled():
        tmp_clos_max_list = []
        for res, clos_max in zip(res_info, rdt_res_clos_max):
            if res == 'MBA':
                tmp_clos_max_list.append(clos_max)
            else:
                tmp_clos_max_list.append(clos_max//2)
        common_clos_max = min(tmp_clos_max_list)

    return common_clos_max


def get_sub_pci_name(i_cnt, bar_attr):
    tmp_sub_name = ''
    # if there is only one host bridge, then will discard the index of suffix
    if i_cnt == 0 and bar_attr.name.upper() == "HOST BRIDGE":
        tmp_sub_name = "_".join(bar_attr.name.split()).upper()
    else:
        if '-' in bar_attr.name:
            tmp_sub_name = acrn_config_utilities.undline_name(bar_attr.name) + "_" + str(i_cnt)
        else:
            tmp_sub_name = "_".join(bar_attr.name.split()).upper() + "_" + str(i_cnt)

    return tmp_sub_name

def get_known_caps_pci_devs():
    known_caps_pci_devs = {}
    vpid_lines = get_info(acrn_config_utilities.BOARD_INFO_FILE, "<PCI_VID_PID>", "</PCI_VID_PID>")
    for dev,known_dev in KNOWN_CAPS_PCI_DEVS_DB.items():
        if dev not in known_caps_pci_devs:
            known_caps_pci_devs[dev] = []
        for k_dev in known_dev:
            for vpid_line in vpid_lines:
                if k_dev in vpid_line:
                    bdf = vpid_line.split()[0]
                    known_caps_pci_devs[dev].append(bdf)
                    break

    return known_caps_pci_devs


def is_tpm_passthru():

    tpm_passthru = False
    (_, board) = acrn_config_utilities.get_board_name()
    tpm2_passthru_enabled = acrn_config_utilities.get_leaf_tag_map(acrn_config_utilities.SCENARIO_INFO_FILE, "mmio_resources", "TPM2")
    if board in TPM_PASSTHRU_BOARD and tpm2_passthru_enabled and 'y' in tpm2_passthru_enabled.values():
        tpm_passthru = True

    return tpm_passthru


def find_avl_memory(ram_range, hpa_size, hv_start_offset):
    """
    This is get hv address from System RAM as host physical size
    :param ram_range: System RAM mapping
    :param hpa_size: fixed host physical size
    :param hv_start_offset: base address of HV RAM start
    :return: start host physical address
    """
    ret_start_addr = 0
    tmp_order_key = 0
    int_hpa_size = int(hpa_size, 10)

    tmp_order_key = sorted(ram_range)
    for start_addr in tmp_order_key:
        mem_range = ram_range[start_addr]
        if start_addr <= hv_start_offset and hv_start_offset + int_hpa_size <= start_addr + mem_range:
            ret_start_addr = hv_start_offset
            break
        elif start_addr >= hv_start_offset and mem_range >= int_hpa_size:
            ret_start_addr = start_addr
            break

    return hex(ret_start_addr)


def get_ram_range():
    """ Get System RAM range mapping """
    # read system ram from board_info.xml
    ram_range = {}

    io_mem_lines = get_info(
        acrn_config_utilities.BOARD_INFO_FILE, "<IOMEM_INFO>", "</IOMEM_INFO>")

    for line in io_mem_lines:
        if 'System RAM' not in line:
            continue
        start_addr = int(line.split('-')[0], 16)
        end_addr = int(line.split('-')[1].split(':')[0], 16)
        mem_range = end_addr - start_addr
        ram_range[start_addr] = mem_range

    global USED_RAM_RANGE
    tmp_order_key_used = sorted(USED_RAM_RANGE)
    for start_addr_used in tmp_order_key_used:
        mem_range_used = USED_RAM_RANGE[start_addr_used]
        tmp_order_key = sorted(ram_range)
        for start_addr in tmp_order_key:
            mem_range = ram_range[start_addr]
            if start_addr < start_addr_used and start_addr_used + mem_range_used < start_addr + mem_range:
                ram_range[start_addr] = start_addr_used - start_addr
                ram_range[start_addr_used+mem_range_used] = start_addr + mem_range - start_addr_used - mem_range_used
                break
            elif start_addr == start_addr_used and start_addr_used + mem_range_used < start_addr + mem_range:
                del ram_range[start_addr]
                ram_range[start_addr_used + mem_range_used] = start_addr + mem_range - start_addr_used - mem_range_used
                break
            elif start_addr < start_addr_used and start_addr_used + mem_range_used == start_addr + mem_range:
                ram_range[start_addr] = start_addr_used - start_addr
                break
            elif start_addr == start_addr_used and start_addr_used + mem_range_used == start_addr + mem_range:
                del ram_range[start_addr]
                break
            else:
                continue

    return ram_range


def is_p2sb_passthru_possible():

    p2sb_passthru = False
    (_, board) = acrn_config_utilities.get_board_name()
    if board in P2SB_PASSTHRU_BOARD:
        p2sb_passthru = True

    return p2sb_passthru


def is_matched_board(boardlist):

    (_, board) = acrn_config_utilities.get_board_name()

    return board in boardlist


def find_p2sb_bar_addr():
    if not is_matched_board(('ehl-crb-b')):
        acrn_config_utilities.print_red('find_p2sb_bar_addr() can only be called for board ehl-crb-b', err=True)
        sys.exit(1)

    iomem_lines = get_info(acrn_config_utilities.BOARD_INFO_FILE, "<IOMEM_INFO>", "</IOMEM_INFO>")

    for line in iomem_lines:
        if 'INTC1020:' in line:
            start_addr = int(line.split('-')[0], 16) & 0xFF000000
            return start_addr

    acrn_config_utilities.print_red('p2sb device is not found in board file %s!\n' % acrn_config_utilities.BOARD_INFO_FILE, err=True)
    sys.exit(1)
