#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import argparse
import lxml.etree as etree
from scenario_transformer import ScenarioTransformer

class DefaultValuePopulator(ScenarioTransformer):
    def add_missing_nodes(self, xsd_element_node, xml_parent_node, new_node_index):
        element_name = xsd_element_node.get("name")
        default_value = xsd_element_node.get("default")

        # If the node is neither of a complex type (i.e. it does not have an child node) nor has a default value, do not
        # create the node at all. Users are required to fill in proper values in such nodes, and missing any of them
        # shall trigger a validation error.
        if self.complex_type_of_element(xsd_element_node) is None and default_value is None:
            return []

        new_node = etree.Element(element_name)
        new_node.text = default_value

        if new_node_index is not None:
            xml_parent_node.insert(new_node_index, new_node)
        else:
            xml_parent_node.append(new_node)

        return [new_node]

def main(xsd_file, xml_file, out_file):
    xsd_etree = etree.parse(xsd_file)
    xsd_etree.xinclude()
    populator = DefaultValuePopulator(xsd_etree)

    xml_etree = etree.parse(xml_file, etree.XMLParser(remove_blank_text=True))
    populator.transform(xml_etree)

    xml_etree.write(out_file, pretty_print=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Populate a given scenario XML with default values of nonexistent nodes")
    parser.add_argument("xsd", help="Path to the schema of scenario XMLs")
    parser.add_argument("xml", help="Path to the scenario XML file from users")
    parser.add_argument("out", nargs="?", default="out.xml", help="Path where the output is placed")
    args = parser.parse_args()

    main(args.xsd, args.xml, args.out)
