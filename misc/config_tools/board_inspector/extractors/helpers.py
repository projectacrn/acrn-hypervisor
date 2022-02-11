# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import lxml

def add_child(element, tag, text=None, **kwargs):
    child = lxml.etree.Element(tag)
    child.text = text
    for k,v in kwargs.items():
        child.set(k, v)
    element.append(child)
    return child

def get_node(etree, xpath):
    result = etree.xpath(xpath)
    assert len(result) <= 1, \
        "Internal error: cannot get texts from multiple nodes at a time.  " \
        "Rerun the Board Inspector with `--loglevel debug`.  If this issue persists, " \
        "log a new issue at https://github.com/projectacrn/acrn-hypervisor/issues and attach the full logs."
    return result[0] if len(result) == 1 else None
