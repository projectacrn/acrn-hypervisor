# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""Configurations for config app.

"""

import os

BOARD_INFO = None
BOARD_TYPE = None
SCENARIO = None
CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'vm_configs', 'xmls', 'config-xmls')
