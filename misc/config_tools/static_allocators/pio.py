#!/usr/bin/env python3
#
# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os, logging
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import lib.lib, lib.error

def fn(board_etree, scenario_etree, allocation_etree):
    # With the console vUART explicitly specified as COM port and communication vUART with explicit I/O port base
    # addresses, there is no need to allocate any port I/O for now.
    #
    # This allocator is preserved here, though, as the implicit vUART connections for system-level power management,
    # which has to be port I/O based and are to be added later, will need I/O port allocation again.
    pass
