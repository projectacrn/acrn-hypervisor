#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
import argparse
import lxml.etree as etree
import logging
import xmlschema

logging_fn = {
    "error": logging.error,
    "warning": logging.warning,
    "info": logging.info,
}

def validate_board(xsd_path, board_etree):
    schema_etree = etree.parse(xsd_path)
    schema_etree.xinclude()
    schema = xmlschema.XMLSchema11(schema_etree)

    it = schema.iter_errors(board_etree)
    count = 0
    for error in it:
        anno = error.validator.annotation
        severity = anno.elem.get("{https://projectacrn.org}severity")
        description = anno.elem.find("{http://www.w3.org/2001/XMLSchema}documentation").text
        logging_fn[severity](description)
        if severity in ["error", "warning"]:
            count += 1

    return count
