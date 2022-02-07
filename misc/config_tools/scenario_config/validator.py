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

try:
    import xmlschema
except ImportError:
    logging.error("Python package `xmlschema` is not installed.\n" +
                  "The scenario XML file will NOT be validated against the schema, which may cause build-time or runtime errors.\n" +
                  "To enable the validation, install the python package by executing: pip3 install xmlschema.")
    sys.exit(0)

from default_populator import DefaultValuePopulator

def existing_file_type(parser):
    def aux(arg):
        if not os.path.exists(arg):
            parser.error(f"can't open {arg}: No such file or directory")
        elif not os.path.isfile(arg):
            parser.error(f"can't open {arg}: Is not a file")
        else:
            return arg
    return aux

def log_level_type(parser):
    def aux(arg):
        arg = arg.lower()
        if arg in ["critical", "error", "warning", "info", "debug"]:
            return arg
        else:
            parser.error(f"{arg} is not a valid log level")
    return aux

def load_schema(xsd_xml, datachecks_xml):
    global schema, schema_etree, datachecks

    schema_etree = etree.parse(xsd_xml)
    schema_etree.xinclude()
    schema = xmlschema.XMLSchema11(etree.tostring(schema_etree, encoding="unicode"))

    datachecks_etree = etree.parse(datachecks_xml)
    datachecks_etree.xinclude()
    datachecks = xmlschema.XMLSchema11(etree.tostring(datachecks_etree, encoding="unicode"))

config_tools_dir = os.path.join(os.path.dirname(__file__), "..")
schema_dir = os.path.join(config_tools_dir, "schema")
schema = None
schema_etree = None
datachecks = None
load_schema(os.path.join(schema_dir, "config.xsd"), os.path.join(schema_dir, "datachecks.xsd"))

def validate_one(board_xml, scenario_xml):
    nr_schema_errors = 0
    nr_check_errors = 0
    nr_check_warnings = 0
    board_name = os.path.basename(board_xml)
    scenario_name = os.path.basename(scenario_xml)

    scenario_etree = etree.parse(scenario_xml, etree.XMLParser(remove_blank_text=True))
    DefaultValuePopulator(schema_etree).transform(scenario_etree)

    it = schema.iter_errors(scenario_etree)
    for error in it:
        logging.debug(error)
        nr_schema_errors += 1

    if nr_schema_errors == 0:
        main_etree = etree.parse(board_xml)
        main_etree.getroot().extend(scenario_etree.getroot()[:])

        it = datachecks.iter_errors(main_etree)
        for error in it:
            logging.debug(error)

            anno = error.validator.annotation
            severity = anno.elem.get("{https://projectacrn.org}severity")

            if severity == "error":
                nr_check_errors += 1
            elif severity == "warning":
                nr_check_warnings += 1

        if nr_check_errors > 0:
            logging.error(f"Board {board_name} and scenario {scenario_name} have inconsistent data: {nr_check_errors} errors, {nr_check_warnings} warnings.")
        elif nr_check_warnings > 0:
            logging.warning(f"Board {board_name} and scenario {scenario_name} have inconsistent data: {nr_check_warnings} warnings.")
        else:
            logging.info(f"Board {board_name} and scenario {scenario_name} are valid and consistent.")
    else:
        logging.warning(f"Scenario {scenario_name} is invalid: {nr_schema_errors} schema errors.")

    return nr_schema_errors + nr_check_errors + nr_check_warnings

def validate_board(board_xml):
    board_dir = os.path.dirname(board_xml)
    nr_violations = 0

    for f in os.listdir(board_dir):
        if not f.endswith(".xml"):
            continue
        if f == os.path.basename(board_xml) or "launch" in f:
            continue

        nr_violations += validate_one(board_xml, os.path.join(board_dir, f))

    return nr_violations

def validate_all(data_dir):
    nr_violations = 0

    for f in os.listdir(data_dir):
        board_xml = os.path.join(data_dir, f, f"{f}.xml")
        if os.path.isfile(board_xml):
            nr_violations += validate_board(board_xml)
        else:
            logging.warning(f"Cannot find a board XML under {os.path.join(data_dir, f)}")

    return nr_violations

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("board", nargs="?", type=existing_file_type(parser), help="the board XML file to be validated")
    parser.add_argument("scenario", nargs="?", type=existing_file_type(parser), help="the scenario XML file to be validated")
    parser.add_argument("--loglevel", default="warning", type=log_level_type(parser), help="choose log level, e.g. debug, info, warning or error")
    args = parser.parse_args()

    logging.basicConfig(level=args.loglevel.upper())

    if args.board and args.scenario:
        nr_violations = validate_one(args.board, args.scenario)
    elif args.board:
        nr_violations = validate_board(args.board)
    else:
        nr_violations = validate_all(os.path.join(config_tools_dir, "data"))

    sys.exit(1 if nr_violations > 0 else 0)
