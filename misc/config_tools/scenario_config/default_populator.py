#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import argparse
import lxml.etree as etree

xpath_ns = {
    "xs": "http://www.w3.org/2001/XMLSchema",
}

def get_node(element, xpath):
    return next(iter(element.xpath(xpath, namespaces=xpath_ns)), None)

def populate_sequence(xsd_etree, xsd_sequence_node, xml_node):
    children = list(xml_node)
    for element_node in xsd_sequence_node.findall("xs:element", namespaces=xpath_ns):
        element_name = element_node.get("name")
        element_min_occurs = element_node.get("minOccurs")

        if len(children) == 0:
            if element_min_occurs != "0":
                xml_child_node = etree.Element(element_name)
                xml_node.append(xml_child_node)
                populate(xsd_etree, element_node, xml_child_node, True)
        elif children[0].tag != element_name:
            if element_min_occurs != "0":
                xml_child_node = etree.Element(element_name)
                children[0].addprevious(xml_child_node)
                populate(xsd_etree, element_node, xml_child_node, True)
        else:
            while len(children) > 0 and children[0].tag == element_name:
                child_node = children.pop(0)
                populate(xsd_etree, element_node, child_node, False)

def populate_all(xsd_etree, xsd_all_node, xml_node):
    for element_node in xsd_all_node.findall("xs:element", namespaces=xpath_ns):
        element_name = element_node.get("name")
        if not element_name:
            continue
        xml_child_node = xml_node.find(element_name)
        if xml_child_node is None:
            element_min_occurs = element_node.get("minOccurs")
            if element_min_occurs == "0":
                continue

            xml_child_node = etree.Element(element_name)
            xml_node.append(xml_child_node)
            populate(xsd_etree, element_node, xml_child_node, True)
        else:
            populate(xsd_etree, element_node, xml_child_node, False)

def populate(xsd_etree, xsd_element_node, xml_node, is_new_node):
    complex_type_node = xsd_element_node.find("xs:complexType", namespaces=xpath_ns)
    if not complex_type_node:
        type_name = xsd_element_node.get("type")
        if type_name:
            complex_type_node = get_node(xsd_etree, f"//xs:complexType[@name='{type_name}']")

    if complex_type_node is not None:
        sequence_node = complex_type_node.find("xs:sequence", namespaces=xpath_ns)
        if sequence_node is not None:
            populate_sequence(xsd_etree, sequence_node, xml_node)

        all_node = complex_type_node.find("xs:all", namespaces=xpath_ns)
        if all_node is not None:
            populate_all(xsd_etree, all_node, xml_node)
    elif is_new_node:
        default_value = xsd_element_node.get("default")
        xml_node.text = default_value

def main(xsd_file, xml_file, out_file):
    xml_etree = etree.parse(xml_file, etree.XMLParser(remove_blank_text=True))
    xsd_etree = etree.parse(xsd_file)
    xsd_etree.xinclude()

    xml_root = xml_etree.getroot()
    xsd_root = get_node(xsd_etree, f"/xs:schema/xs:element[@name='{xml_root.tag}']")

    if xsd_root is not None:
        populate(xsd_etree, xsd_root, xml_root, False)

    xml_etree.write(out_file, pretty_print=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Populate a given scenario XML with default values of nonexistent nodes")
    parser.add_argument("xsd", help="Path to the schema of scenario XMLs")
    parser.add_argument("xml", help="Path to the scenario XML file from users")
    parser.add_argument("out", default="out.xml", help="Path where the output is placed")
    args = parser.parse_args()

    main(args.xsd, args.xml, args.out)
