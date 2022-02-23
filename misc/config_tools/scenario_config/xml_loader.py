#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from defusedxml.ElementTree import parse
from pipeline import PipelineStage

class XMLLoadStage(PipelineStage):
    def __init__(self, tag):
        self.consumes = f"{tag}_path"
        self.provides = f"{tag}_etree"

    def run(self, obj):
        xml_path = obj.get(self.consumes)
        etree = parse(xml_path)
        obj.set(self.provides, etree)
