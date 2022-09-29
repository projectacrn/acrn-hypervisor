# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import lxml, re
from pathlib import Path

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

def get_realpath(pathstr):
    assert isinstance(pathstr, str), f"Internal error: pathstr must be a str: {type(pathstr)}"
    path = Path(pathstr)
    assert path.exists(), f"Internal error: {path} does not exist"
    return str(path.resolve())

def get_bdf_from_realpath(pathstr):
    realpath = get_realpath(pathstr)
    bdf_regex = re.compile(r"^([0-9a-f]{4}):([0-9a-f]{2}):([0-9a-f]{2}).([0-7]{1})$")
    m = bdf_regex.match(realpath.split('/')[-1])
    assert m, f"Internal error: {realpath} contains no matched pattern: {bdf_regex}"
    return int(m.group(2), base=16), int(m.group(3), base=16), int(m.group(4), base=16)
