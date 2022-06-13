#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
import argparse
import logging
from copy import copy
from collections import namedtuple
import re

try:
    import elementpath
    import elementpath_overlay
    from elementpath.xpath_context import XPathContext
    import xmlschema
except ImportError:
    logging.error("Python package `xmlschema` is not installed.\n" +
                  "The scenario XML file will NOT be validated against the schema, which may cause build-time or runtime errors.\n" +
                  "To enable the validation, install the python package by executing: pip3 install xmlschema.")
    sys.exit(0)

from pipeline import PipelineObject, PipelineStage, PipelineEngine
from schema_slicer import SlicingSchemaByVMTypeStage
from default_populator import DefaultValuePopulatingStage

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

class ValidationError(dict):
    logging_fns = {
        "critical": logging.critical,
        "error": logging.error,
        "warning": logging.warning,
        "info": logging.info,
        "debug": logging.debug,
    }

    def __init__(self, paths, message, severity):
        super().__init__(paths = paths, message = message, severity = severity)

    def __str__(self):
        return f"{', '.join(self['paths'])}: {self['message']}"

    def log(self):
        try:
            self.logging_fns[self['severity']](self)
        except KeyError:
            logging.debug(self)

class ScenarioValidator:
    def __init__(self, schema_etree, datachecks_etree):
        """Initialize the validator with preprocessed schemas in ElementTree."""
        self.schema = xmlschema.XMLSchema11(schema_etree)
        self.datachecks = xmlschema.XMLSchema11(datachecks_etree) if datachecks_etree else None

    def check_syntax(self, scenario_etree):
        errors = []

        it = self.schema.iter_errors(scenario_etree)
        for error in it:
            # Syntactic errors are always critical.
            e = ValidationError([error.path], error.reason, "critical")
            e.log()
            errors.append(e)

        return errors

    def check_semantics(self, board_etree, scenario_etree):
        errors = []

        if self.datachecks:
            unified_node = copy(scenario_etree.getroot())
            parent_map = {c : p for p in unified_node.iter() for c in p}
            unified_node.extend(board_etree.getroot())
            it = self.datachecks.iter_errors(unified_node)
            for error in it:
                e = self.format_error(unified_node, parent_map, error)
                e.log()
                errors.append(e)

        return errors

    @staticmethod
    def format_paths(unified_node, parent_map, report_on, variables):
        elems = elementpath.select(unified_node, report_on, variables = variables, parser = elementpath.XPath2Parser)
        paths = []
        for elem in elems:
            path = []
            while elem is not None:
                path_segment = elem.tag
                parent = parent_map.get(elem, None)
                if parent is not None:
                    children = parent.findall(elem.tag)
                    if len(children) > 1:
                        path_segment += f"[{children.index(elem) + 1}]"
                path.insert(0, path_segment)
                elem = parent
            paths.append(f"/{'/'.join(path)}")
        return paths

    @staticmethod
    def get_counter_example(error):
        assertion = error.validator
        if not isinstance(assertion, xmlschema.validators.assertions.XsdAssert):
            return {}

        elem = error.obj
        context = XPathContext(elem, variables={'value': None})
        context.counter_example = {}
        result = assertion.token.evaluate(context)

        if result == False:
            return context.counter_example
        else:
            return {}

    @staticmethod
    def format_error(unified_node, parent_map, error):
        def format_node(n):
            if isinstance(n, str):
                return n
            elif isinstance(n, (int, float)):
                return str(n)
            elif isinstance(n, object) and n.__class__.__name__.endswith("Element"):
                return n.text
            else:
                return str(n)

        anno = error.validator.annotation
        counter_example = ScenarioValidator.get_counter_example(error)
        variables = {k.obj.source.strip("$"): v for k,v in counter_example.items()}

        paths = ScenarioValidator.format_paths(unified_node, parent_map, anno.elem.get("{https://projectacrn.org}report-on"), variables)
        description = anno.elem.find("{http://www.w3.org/2001/XMLSchema}documentation").text
        severity = anno.elem.get("{https://projectacrn.org}severity")

        expr_regex = re.compile("{[^{}]*}")
        exprs = set(expr_regex.findall(description))
        for expr in exprs:
            result = elementpath.select(unified_node, expr.strip("{}"), variables = variables, parser = elementpath.XPath2Parser)
            if isinstance(result, list):
                if len(result) == 1:
                    value = format_node(result[0])
                elif len(result) > 1:
                    s = ', '.join(map(format_node, result))
                    value = f"[{s}]"
                else:
                    value = "{unknown}"
            else:
                value = str(result)
            description = description.replace(expr, value)

        return ValidationError(paths, description, severity)

class ValidatorConstructionStage(PipelineStage):
    # The schema etree may still useful for schema-based transformation. Do not consume it.
    uses = {"schema_etree"}
    consumes = {"datachecks_etree"}
    provides = {"validator"}

    def run(self, obj):
        validator = ScenarioValidator(obj.get("schema_etree"), obj.get("datachecks_etree"))
        obj.set("validator", validator)

class ValidatorConstructionByFileStage(PipelineStage):
    uses = {"schema_path", "datachecks_path"}
    provides = {"validator"}

    def run(self, obj):
        validator = ScenarioValidator(obj.get("schema_path"), obj.get("datachecks_path"))
        obj.set("validator", validator)

class SyntacticValidationStage(PipelineStage):
    provides = {"syntactic_errors"}

    def __init__(self, etree_tag = "scenario"):
        self.etree_tag = f"{etree_tag}_etree"
        self.uses = {"validator", self.etree_tag}

    def run(self, obj):
        errors = obj.get("validator").check_syntax(obj.get(self.etree_tag))
        obj.set("syntactic_errors", errors)

class SemanticValidationStage(PipelineStage):
    uses = {"validator", "board_etree", "scenario_etree"}
    provides = {"semantic_errors"}

    def run(self, obj):
        errors = obj.get("validator").check_semantics(obj.get("board_etree"), obj.get("scenario_etree"))
        obj.set("semantic_errors", errors)

class ReportValidationResultStage(PipelineStage):
    consumes = {"board_etree", "scenario_etree", "syntactic_errors", "semantic_errors"}
    provides = {"nr_all_errors"}

    def run(self, obj):
        board_name = obj.get("board_etree").getroot().get("board")
        scenario_name = obj.get("scenario_etree").getroot().get("scenario")

        nr_critical = len(obj.get("syntactic_errors"))
        nr_error = len(list(filter(lambda e: e["severity"] == "error", obj.get("semantic_errors"))))
        nr_warning = len(list(filter(lambda e: e["severity"] == "warning", obj.get("semantic_errors"))))

        if nr_critical > 0 or nr_error > 0:
            logging.error(f"Board {board_name} and scenario {scenario_name} are inconsistent: {nr_critical} syntax errors, {nr_error} data errors, {nr_warning} warnings.")
        elif nr_warning > 0:
            logging.warning(f"Board {board_name} and scenario {scenario_name} are potentially inconsistent: {nr_warning} warnings.")
        else:
            logging.info(f"Board {board_name} and scenario {scenario_name} are valid and consistent.")

        obj.set("nr_all_errors", nr_critical + nr_error + nr_warning)

def validate_one(validation_pipeline, pipeline_obj, board_xml, scenario_xml):
    pipeline_obj.set("board_path", board_xml)
    pipeline_obj.set("scenario_path", scenario_xml)
    validation_pipeline.run(pipeline_obj)
    return pipeline_obj.consume("nr_all_errors")

def validate_board(validation_pipeline, pipeline_obj, board_xml):
    board_dir = os.path.dirname(board_xml)
    nr_all_errors = 0

    for f in os.listdir(board_dir):
        if not f.endswith(".xml"):
            continue
        if f == os.path.basename(board_xml) or "launch" in f:
            continue
        nr_all_errors += validate_one(validation_pipeline, pipeline_obj, board_xml, os.path.join(board_dir, f))

    return nr_all_errors

def validate_all(validation_pipeline, pipeline_obj, data_dir):
    nr_all_errors = 0

    for f in os.listdir(data_dir):
        board_xml = os.path.join(data_dir, f, f"{f}.xml")
        if os.path.isfile(board_xml):
            nr_all_errors += validate_board(validation_pipeline, pipeline_obj, board_xml)
        else:
            logging.warning(f"Cannot find a board XML under {os.path.join(data_dir, f)}")

    return nr_all_errors

def main(args):
    from lxml_loader import LXMLLoadStage

    validator_construction_pipeline = PipelineEngine(["schema_path", "datachecks_path"])
    validator_construction_pipeline.add_stages([
        LXMLLoadStage("schema"),
        LXMLLoadStage("datachecks"),
        SlicingSchemaByVMTypeStage(),
        ValidatorConstructionStage(),
    ])

    validation_pipeline = PipelineEngine(["board_path", "scenario_path", "schema_etree", "validator"])
    validation_pipeline.add_stages([
        LXMLLoadStage("board"),
        LXMLLoadStage("scenario"),
        DefaultValuePopulatingStage(),
        SyntacticValidationStage(),
        SemanticValidationStage(),
        ReportValidationResultStage(),
    ])

    obj = PipelineObject(schema_path = args.schema, datachecks_path = args.datachecks)
    validator_construction_pipeline.run(obj)
    if args.board and args.scenario:
        nr_all_errors = validate_one(validation_pipeline, obj, args.board, args.scenario)
    elif args.board:
        nr_all_errors = validate_board(validation_pipeline, obj, args.board)
    else:
        nr_all_errors = validate_all(validation_pipeline, obj, os.path.join(config_tools_dir, "data"))

    sys.exit(1 if nr_all_errors > 0 else 0)

if __name__ == "__main__":
    config_tools_dir = os.path.join(os.path.dirname(__file__), "..")
    schema_dir = os.path.join(config_tools_dir, "schema")

    parser = argparse.ArgumentParser()
    parser.add_argument("board", nargs="?", type=existing_file_type(parser), help="the board XML file to be validated")
    parser.add_argument("scenario", nargs="?", type=existing_file_type(parser), help="the scenario XML file to be validated")
    parser.add_argument("--loglevel", default="warning", type=log_level_type(parser), help="choose log level, e.g. debug, info, warning or error")
    parser.add_argument("--schema", default=os.path.join(schema_dir, "config.xsd"), help="the XML schema that defines the syntax of scenario XMLs")
    parser.add_argument("--datachecks", default=os.path.join(schema_dir, "datachecks.xsd"), help="the XML schema that defines the semantic rules against board and scenario data")
    args = parser.parse_args()

    logging.basicConfig(level=args.loglevel.upper())
    main(args)
