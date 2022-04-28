#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import argparse
import elementpath

from scenario_transformer import ScenarioTransformer

from pipeline import PipelineObject, PipelineStage, PipelineEngine
from schema_slicer import SlicingSchemaByVMTypeStage

class DefaultValuePopulator(ScenarioTransformer):
    def get_default_value(self, xsd_element_node, xml_parent_node):
        # The attribute @default of the xsd:element node
        v = xsd_element_node.get("default")
        if v is not None:
            return v

        # The acrn:defaults and acrn:unique-among annotations which define a set of default values that shall be unique
        # among a collection of nodes
        annot_node = self.get_node(xsd_element_node, "xs:annotation")
        if annot_node is not None:
            defaults = annot_node.get("{https://projectacrn.org}defaults")
            unique_among = annot_node.get("{https://projectacrn.org}unique-among")
            if defaults is not None and unique_among is not None:
                try:
                    default_values = set(eval(defaults))
                    existing_values = set(elementpath.select(self.xml_etree, unique_among, variables={"parent": xml_parent_node}))
                    available_defaults = default_values - existing_values
                    return sorted(list(available_defaults))[0]
                except:
                    pass

        return None

    def add_missing_nodes(self, xsd_element_node, xml_parent_node, new_node_index):
        element_name = xsd_element_node.get("name")
        default_value = self.get_default_value(xsd_element_node, xml_parent_node)

        # If the node is neither of a complex type (i.e. it does not have an child node) nor has a default value, do not
        # create the node at all. Users are required to fill in proper values in such nodes, and missing any of them
        # shall trigger a validation error.
        if self.complex_type_of_element(xsd_element_node) is None and default_value is None:
            return []

        new_node = xml_parent_node.makeelement(element_name, {})
        new_node.text = default_value

        if new_node_index is not None:
            xml_parent_node.insert(new_node_index, new_node)
        else:
            xml_parent_node.append(new_node)

        return [new_node]

    def fill_empty_node(self, xsd_element_node, xml_parent_node, xml_empty_node):
        default_value = self.get_default_value(xsd_element_node, xml_parent_node)
        if default_value is not None:
            xml_empty_node.text = default_value

class DefaultValuePopulatingStage(PipelineStage):
    uses = {"schema_etree", "scenario_etree"}
    provides = {"scenario_etree"}

    def run(self, obj):
        populator = DefaultValuePopulator(obj.get("schema_etree"))
        etree = obj.get("scenario_etree")
        populator.transform(etree)
        obj.set("scenario_etree", etree)

def main(args):
    from xml_loader import XMLLoadStage
    from lxml_loader import LXMLLoadStage

    pipeline = PipelineEngine(["schema_path", "scenario_path"])
    pipeline.add_stages([
        LXMLLoadStage("schema"),
        XMLLoadStage("scenario"),
        SlicingSchemaByVMTypeStage(),
        DefaultValuePopulatingStage(),
    ])

    obj = PipelineObject(schema_path = args.schema, scenario_path = args.scenario)
    pipeline.run(obj)
    obj.get("scenario_etree").write(args.out)

if __name__ == "__main__":
    config_tools_dir = os.path.join(os.path.dirname(__file__), "..")
    schema_dir = os.path.join(config_tools_dir, "schema")

    parser = argparse.ArgumentParser(description="Populate a given scenario XML with default values of nonexistent nodes")
    parser.add_argument("scenario", help="Path to the scenario XML file from users")
    parser.add_argument("out", nargs="?", default="out.xml", help="Path where the output is placed")
    parser.add_argument("--schema", default=os.path.join(schema_dir, "config.xsd"), help="the XML schema that defines the syntax of scenario XMLs")
    args = parser.parse_args()

    main(args)
