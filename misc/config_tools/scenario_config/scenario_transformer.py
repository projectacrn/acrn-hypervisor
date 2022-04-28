#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import elementpath

class ScenarioTransformer:
    xpath_ns = {
        "xs": "http://www.w3.org/2001/XMLSchema",
    }

    @classmethod
    def get_node(cls, element, xpath):
        return element.find(xpath, namespaces=cls.xpath_ns)

    def __init__(self, xsd_etree, visit_optional_node=False):
        self.xsd_etree = xsd_etree
        self.xml_etree = None

        self._visit_optional_node = visit_optional_node

    def type_of_element(self, type_tag, xsd_element_node, xml_node):
        xsd_alternative_node = xsd_element_node

        if xml_node is not None:
            for alternative in xsd_element_node.findall("xs:alternative", namespaces=self.xpath_ns):
                if elementpath.select(xml_node, alternative.get("test")):
                    xsd_alternative_node = alternative
                    break

        type_node = xsd_alternative_node.find(type_tag, namespaces=self.xpath_ns)
        if type_node is None:
            type_name = xsd_alternative_node.get("type")
            if type_name:
                type_node = self.get_node(self.xsd_etree, f".//{type_tag}[@name='{type_name}']")

        return type_node

    def simple_type_of_element(self, xsd_element_node, xml_node = None):
        return self.type_of_element("xs:simpleType", xsd_element_node, xml_node)

    def complex_type_of_element(self, xsd_element_node, xml_node = None):
        return self.type_of_element("xs:complexType", xsd_element_node, xml_node)

    def transform_node(self, xsd_element_node, xml_node):
        complex_type_node = self.complex_type_of_element(xsd_element_node, xml_node)
        if complex_type_node is not None:
            xsd_sequence_node = complex_type_node.find("xs:sequence", namespaces=self.xpath_ns)
            if xsd_sequence_node is not None:
                self.transform_sequence(xsd_sequence_node, xml_node)

            xsd_all_node = complex_type_node.find("xs:all", namespaces=self.xpath_ns)
            if xsd_all_node is not None:
                self.transform_all(xsd_all_node, xml_node)

    def transform_sequence(self, xsd_sequence_node, xml_node):
        children = list(enumerate(list(xml_node)))
        for xsd_element_node in xsd_sequence_node.findall("xs:element", namespaces=self.xpath_ns):
            element_name = xsd_element_node.get("name")
            element_min_occurs = xsd_element_node.get("minOccurs")

            if len(children) == 0 or children[0][1].tag != element_name:
                if self._visit_optional_node or element_min_occurs != "0":
                    index = children[0][0] if len(children) > 0 else None
                    self.add_and_transform_missing_node(xsd_element_node, xml_node, new_node_index=index)
            else:
                while len(children) > 0 and children[0][1].tag == element_name:
                    xml_child_node = children.pop(0)[1]
                    if self.complex_type_of_element(xsd_element_node, xml_child_node) is None and not xml_child_node.text:
                        self.fill_empty_node(xsd_element_node, xml_node, xml_child_node)
                    self.transform_node(xsd_element_node, xml_child_node)

    def transform_all(self, xsd_all_node, xml_node):
        for xsd_element_node in xsd_all_node.findall("xs:element", namespaces=self.xpath_ns):
            element_name = xsd_element_node.get("name")
            if not element_name:
                continue

            xml_children = xml_node.findall(element_name)
            if len(xml_children) == 0:
                element_min_occurs = xsd_element_node.get("minOccurs")
                if self._visit_optional_node or element_min_occurs != "0":
                    self.add_and_transform_missing_node(xsd_element_node, xml_node)
            else:
                for xml_child_node in xml_children:
                    if self.complex_type_of_element(xsd_element_node, xml_child_node) is None and not xml_child_node.text:
                        self.fill_empty_node(xsd_element_node, xml_node, xml_child_node)
                    self.transform_node(xsd_element_node, xml_child_node)

    def add_and_transform_missing_node(self, xsd_element_node, xml_parent_node, new_node_index=None):
        for new_node in self.add_missing_nodes(xsd_element_node, xml_parent_node, new_node_index):
            self.transform_node(xsd_element_node, new_node)

    def add_missing_nodes(self, xsd_element_node, xml_parent_node, new_node_index):
        return []

    def fill_empty_node(self, xsd_element_node, xml_parent_node, xml_empty_node):
        pass

    def transform(self, xml_etree):
        self.xml_etree = xml_etree

        xml_root_node = xml_etree.getroot()
        xsd_root_node = self.get_node(self.xsd_etree, f".//xs:element[@name='{xml_root_node.tag}']")
        if xsd_root_node is not None:
            self.transform_node(xsd_root_node, xml_root_node)

        self.xml_etree = None
