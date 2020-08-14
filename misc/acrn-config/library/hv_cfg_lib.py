# Copyright (C) 2020 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import common
import getopt
import board_cfg_lib


ERR_LIST = {}
N_Y = ['n', 'y']
SCHEDULER_TYPE = ['SCHED_NOOP', 'SCHED_IORR', 'SCHED_BVT']

RANGE_DB = {
    'LOG_LEVEL':{'min':0,'max':6},
    'LOG_DESTINATION_BITMAP':{'min':0,'max':7},
    'EMULATED_MMIO_REGIONS':{'min':0,'max':128},
    'PT_IRQ_ENTRIES':{'min':0,'max':256},
    'IOAPIC_NUM':{'min':1,'max':10},
    'IOAPIC_LINES':{'min':1,'max':120},
    'PCI_DEV_NUM':{'min':1,'max':1024},
    'MSIX_TABLE_NUM':{'min':1,'max':2048},
}


def empty_check(val, prime_item, item, sub_item=''):
    if not val or val == None:
        if sub_item:
            key = 'hv,{},{},{}'.format(prime_item, item, sub_item)
            ERR_LIST[key] = "{} should not empty".format(sub_item)
        else:
            key = 'hv,{},{}'.format(prime_item, item)
            ERR_LIST[key] = "{} should not empty".format(item)
        return True

    return False


def is_numeric_check(str_value, prime_item, item):

    # to skip for strip 0x/0X
    if str_value == '0':
        return True
    str_hex_0x = str_value.lstrip('0x')
    str_hex_0X = str_value.lstrip('0X')

    if not str_hex_0x.isnumeric() and not str_hex_0X.isnumeric():
        if not isinstance(int(str_hex_0x, 16), int) and not isinstance(int(str_hex_0X, 16), int):
            key = 'hv,{},{}'.format(prime_item, item)
            ERR_LIST[key] = "{} should be a numeric".format(item)
            return False
    return True


def range_check(str_value, prime_item, item, range_val):

    value = common.num2int(str_value)
    if value < range_val['min'] or value > range_val['max']:
        key = 'hv,{},{}'.format(prime_item, item)
        ERR_LIST[key] = "{} should be in range[{},{}]".format(item, range_val['min'], range_val['max'])


def release_check(sel_str, dbg_opt, rel_str):
    if empty_check(sel_str, dbg_opt, rel_str):
        return
    if sel_str not in N_Y:
        key = 'hv,{},{}'.format(dbg_opt, rel_str)
        ERR_LIST[key] = "{} should be in {}".format(rel_str, N_Y)


def hv_range_check(str_val, branch_tag, item, range_db, empty_check_enable=True):

    if empty_check_enable:
        if empty_check(str_val, branch_tag, item):
            return
    if not is_numeric_check(str_val, branch_tag, item):
        return
    range_check(str_val, branch_tag, item, range_db)


def hv_size_check(str_val, branch_tag, item):

    if empty_check(str_val, branch_tag, item):
        return
    if not is_numeric_check(str_val, branch_tag, item):
        return


def ir_entries_check(str_num, cap, cap_ir_entries):
    hv_size_check(str_num, cap, cap_ir_entries)
    val = common.num2int(str_num)
    if val % 2 != 0:
        key = 'hv,{},{}'.format(cap, cap_ir_entries)
        ERR_LIST[key] = "{} should be a value of 2^n".format(cap_ir_entries)


def uefi_load_name_check(str_name, mis, mis_uefi_name):

    name_len = len(str_name)
    if name_len > 256 or name_len < 0:
        key = 'hv,{},{}'.format(mis, mis_uefi_name)
        ERR_LIST[key] = "{} length should be in range[0, 256]".format(mis_uefi_name)


def ny_support_check(sel_str, feat, feat_item, feat_sub_leaf=''):
    if empty_check(sel_str, feat, feat_item, feat_sub_leaf):
        return
    if sel_str not in N_Y:
        key = 'hv,{},{}'.format(feat, feat_item)
        ERR_LIST[key] = "{} should be in {}".format(feat_item, N_Y)


def scheduler_check(sel_str, feat, feat_scheduler):
    if empty_check(sel_str, feat, feat_scheduler):
        return
    if sel_str not in SCHEDULER_TYPE:
        key = 'hv,{},{}'.format(feat, feat_scheduler)
        ERR_LIST[key] = "{} should be in {}".format(feat_scheduler, SCHEDULER_TYPE)


def get_select_range(branch_tag, range_key):

    range_list = []
    if range_key not in RANGE_DB.keys():
        key = "hv,{},{}".format(branch_tag, range_key)
        ERR_LIST[key] = "It is invalid for {}.".format(range_key)
        return range_list

    for range_i in range(RANGE_DB[range_key]['min'], RANGE_DB[range_key]['max'] + 1):
        range_list.append(str(range_i))

    return range_list


def is_contiguous_bit_set(value):

    bit_1_cnt = 0
    tmp_val = value
    is_contiguous = False

    first_p = 0
    last_p = 0

    while tmp_val > 0:
        tmp_val &= (tmp_val - 1)
        bit_1_cnt += 1

    for shift_i in range(32):
        mask = (0x1 << shift_i)
        if value & mask:
            if first_p == 0 and last_p == 0:
                first_p = shift_i + 1
            elif first_p != 0:
                last_p = shift_i + 1
        else:
            if first_p == 0 and last_p == 0:
                continue
            break


    contiguous_cnt = last_p - first_p + 1
    if bit_1_cnt == contiguous_cnt or bit_1_cnt in (0, 1):
        is_contiguous = True

    return is_contiguous


def cat_max_mask_check(cat_mask_list, feature, cat_str, max_mask_str):

    (res_info, rdt_res_clos_max, clos_max_mask_list) = board_cfg_lib.clos_info_parser(common.BOARD_INFO_FILE)
    if not board_cfg_lib.is_rdt_enabled() or ("L2" not in res_info and "L3" not in res_info):
        return

    if board_cfg_lib.is_cdp_enabled():
        clos_max_set_entry = 2 * board_cfg_lib.get_common_clos_max()
    else:
        clos_max_set_entry = board_cfg_lib.get_common_clos_max()

    cat_max_mask_settings_len = len(cat_mask_list)
    if clos_max_set_entry != cat_max_mask_settings_len:
        key = 'hv,{},{},{}'.format(feature, cat_str, max_mask_str)
        ERR_LIST[key] = "Number of Cache mask entries should be equal to MAX_PLATFORM_CLOS_NUM={}".format(clos_max_set_entry)
        return

    clos_max_mask_str = clos_max_mask_list[0].strip('"').strip("'")
    clos_max_mask = common.num2int(clos_max_mask_str)
    for val_str in cat_mask_list:
        if empty_check(val_str, feature, cat_str, max_mask_str):
            return
        value = common.num2int(val_str)
        if value < 0 or value > clos_max_mask:
            key = 'hv,{},{},{}'.format(feature, cat_str, max_mask_str)
            ERR_LIST[key] = "{} should be in range[0,{}]".format(max_mask_str, clos_max_mask_str)
            return

        if not is_contiguous_bit_set(value):
            key = 'hv,{},{},{}'.format(feature, cat_str, max_mask_str)
            ERR_LIST[key] = "CLOS_MASK {} should be contiguous bit set.".format(max_mask_str, clos_max_mask_str)
            return


def mba_delay_check(mba_delay_list, feature, mba_str, max_mask_str):

    (res_info, rdt_res_clos_max, clos_max_mask_list) = board_cfg_lib.clos_info_parser(common.BOARD_INFO_FILE)
    if not board_cfg_lib.is_rdt_enabled() or "MBA" not in res_info:
        return

    clos_max = board_cfg_lib.get_common_clos_max()
    mba_delay_settings_len = len(mba_delay_list)
    if clos_max != mba_delay_settings_len:
        key = 'hv,{},{},{}'.format(feature, mba_str, max_mask_str)
        ERR_LIST[key] = "Number of MBA delay entries should be equal to MAX_PLATFORM_CLOS_NUM={}".format(clos_max)
        return

    mba_idx = res_info.index("MBA")
    mba_delay_str = clos_max_mask_list[mba_idx].strip('"').strip("'")
    mba_delay = common.num2int(mba_delay_str)
    for val_str in mba_delay_list:
        if empty_check(val_str, feature, mba_str, max_mask_str):
            return
        value = common.num2int(val_str)
        if value > mba_delay:
            key = 'hv,{},{},{}'.format(feature, mba_str, max_mask_str)
            ERR_LIST[key] = "{} should be in range[0,{}]".format(max_mask_str, mba_delay_str)
            return


def max_msix_table_num_check(max_msix_table_num, cap_str, max_msi_num_str):
    native_max_msix_line = board_cfg_lib.get_info(common.BOARD_INFO_FILE, "<MAX_MSIX_TABLE_NUM>", "</MAX_MSIX_TABLE_NUM>")
    if not native_max_msix_line and not max_msix_table_num:
        empty_check(max_msix_table_num, cap_str, max_msi_num_str)
        return

    if max_msix_table_num:
        hv_range_check(max_msix_table_num, cap_str, max_msi_num_str, RANGE_DB['MSIX_TABLE_NUM'], False)
    if native_max_msix_line:
        native_max_msix_num = native_max_msix_line[0].strip()
        range_check(native_max_msix_num, "In board xml", max_msi_num_str, RANGE_DB['MSIX_TABLE_NUM'])
