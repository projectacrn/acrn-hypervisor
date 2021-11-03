#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

class ResourceError(Exception):
    """Raise this error when it is out of resource"""

class SettingError(Exception):
    """Raise this error when manual scenario configuration has a conflict with board information"""