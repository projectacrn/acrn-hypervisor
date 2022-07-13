# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import logging

import inspectorlib.cdata as cdata
from acpiparser._utils import TableHeader

def tpm2_optional_data(data_len):
    start_method_data_len = 0
    has_log_area = False
    if data_len <= 12:
        start_method_data_len = data_len
    elif data_len == 24:
        start_method_data_len = 12
        has_log_area = True
    else:
        start_method_data_len = 12
        logging.debug(f"TPM2 data length: {data_len + 52} is greater than 64 bytes but less than 76 bytes.")
        logging.debug(f"The TPM2 data is still processed but the 65 to {data_len + 52} bytes are discard.")
    return start_method_data_len, has_log_area

def tpm2_factory(start_method_data_len, has_log_area):
    class TPM2(cdata.Struct):
        _pack_ = 1
        _fields_ = [
            ('header', TableHeader),
            ('platform_class', ctypes.c_uint16),
            ('reserved', ctypes.c_uint16),
            ('address_of_control_area', ctypes.c_uint64),
            ('start_method', ctypes.c_uint32),
            ('start_method_specific_parameters', ctypes.c_ubyte * start_method_data_len),
        ] + ([
            ('log_area_minimum_length', ctypes.c_uint32),
            ('log_area_start_address', ctypes.c_uint64),
        ] if has_log_area else [])

    return TPM2

def TPM2(val):
    """Create class based on decode of a TPM2 table from filename."""
    if isinstance(val, str):
        base_length = 52
        data = open(val, mode='rb').read()
        start_method_data_len, has_log_area = tpm2_optional_data(len(data) - base_length)
        return tpm2_factory(start_method_data_len, has_log_area).from_buffer_copy(data)
    elif isinstance(val, bytearray):
        return tpm2_factory(12, True).from_buffer(val) if len(val) > 64 else tpm2_factory(12, False).from_buffer(val)
