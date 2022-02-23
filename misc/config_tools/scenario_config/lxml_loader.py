#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from lxml.etree import parse, XMLParser
from pipeline import PipelineStage

class LXMLLoadStage(PipelineStage):
    def __init__(self, tag):
        self.consumes = f"{tag}_path"
        self.provides = f"{tag}_etree"

    def run(self, obj):
        xml_path = obj.get(self.consumes)
        etree = parse(xml_path, XMLParser(remove_blank_text=True))
        etree.xinclude()
        obj.set(self.provides, etree)
