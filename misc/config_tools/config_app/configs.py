# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""Configurations for config app.

"""

import os

BOARD_INFO = None
BOARD_TYPE = None
SCENARIO = None
LAUNCH = None
DEFAULT_CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data')
CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', 'user_config')

if not os.path.isdir(CONFIG_PATH):
    os.makedirs(CONFIG_PATH)
