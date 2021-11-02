# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""Controller for config app.

"""

import os
from xmlschema import XMLSchema11
from xmlschema.validators import Xsd11Element, XsdSimpleType, XsdAtomicBuiltin, Xsd11ComplexType, Xsd11Group, Xsd11Attribute
import lxml.etree as etree


class XmlConfig:
    """The core class to analyze and modify acrn config xml files"""
    def __init__(self, path=None, default=True):
        self._xml_path = path
        self._default = default
        self._curr_xml = None
        self._curr_xml_tree = None

    @staticmethod
    def _get_xml_type(xml_file):
        """
        get the config type by file.
        :param xml_file: the file path of xml file.
        :return: the xml type.
        :raises: ValueError, OSError, SyntaxError.
        """
        xml_type = ''
        if os.path.splitext(xml_file)[1] != '.xml':
            return xml_type
        try:
            tree = etree.parse(xml_file)
            root = tree.getroot()
            if 'user_vm_launcher' in root.attrib:
                xml_type = 'user_vm_launcher'
            elif 'scenario' in root.attrib:
                xml_type = 'scenario'
            elif 'board' in root.attrib:
                xml_type = 'board'
            elif 'board_setting' in root.attrib:
                xml_type = 'board_setting'
        except ValueError:
            print('xml parse error: {}'.format(xml_file))
            xml_type = ''
        except OSError:
            print('xml open error: {}'.format(xml_file))
            xml_type = ''
        except SyntaxError:
            print('xml syntax error: {}'.format(xml_file))
            xml_type = ''

        return xml_type

    def list_all(self, xml_type=None):
        """
        list all xml config files by type.
        :param xml_type: the xml type.
        :return: he list of xml config files.
        """
        xmls = []
        user_xmls = []

        if self._xml_path is None or not os.path.exists(self._xml_path):
            return xmls, user_xmls
        for test_file in os.listdir(self._xml_path):
            test_file_path = os.path.join(self._xml_path, test_file)
            if os.path.isfile(test_file_path):
                if XmlConfig._get_xml_type(test_file_path) == xml_type:
                    xmls.append(os.path.splitext(test_file)[0])
        user_path = os.path.join(self._xml_path, 'user_defined')
        if os.path.isdir(user_path):
            for test_file in os.listdir(user_path):
                test_file_path = os.path.join(user_path, test_file)
                if os.path.isfile(test_file_path):
                    if XmlConfig._get_xml_type(test_file_path) == xml_type:
                        user_xmls.append(os.path.splitext(test_file)[0])

        return xmls, user_xmls

    def set_curr(self, xml):
        """
        set current xml file to analyze.
        :param xml: the xml file.
        :return: None.
        :raises: ValueError, OSError, SyntaxError.
        """
        if self._xml_path is None or xml is None:
            return
        try:
            self._curr_xml = xml

            xml_path = os.path.join(self._xml_path, self._curr_xml + '.xml') \
                if self._default \
                else os.path.join(self._xml_path, 'user_defined', self._curr_xml + '.xml')

            parser = etree.XMLParser(remove_blank_text=True)
            tree = etree.parse(xml_path, parser)
            self._curr_xml_tree = tree
        except ValueError:
            print('xml parse error: {}'.format(xml))
            self._curr_xml = None
            self._curr_xml_tree = None
        except OSError:
            print('xml open error: {}'.format(xml))
            self._curr_xml = None
            self._curr_xml_tree = None
        except SyntaxError:
            print('xml syntax error: {}'.format(xml))
            self._curr_xml = None
            self._curr_xml_tree = None

    def get_curr(self):
        """
        get current xml config file.
        :return: current xml config file name.
        """
        return self._curr_xml

    def get_curr_root(self):
        """
        get the xml root of current xml config file.
        :return: the xml root of current xml config file.
        """
        if self._curr_xml_tree is None:
            return None
        return self._curr_xml_tree.getroot()

    def get_curr_value(self, *args):
        """
        get the value of the element by its path.
        :param args: the path of the element.
        :return: the value of the element.
        """
        if self._curr_xml_tree is None:
            return None
        dest_node = self._get_dest_node(*args)
        if dest_node is None:
            return None
        if dest_node.text is None or dest_node.text.strip() == '':
            return ''
        return dest_node.text

    def set_curr_value(self, value, *args):
        """
        set the value of the element by its path.
        :param value: the value of the element.
        :param args: the path of the element.
        :return: None.
        """
        if self._curr_xml_tree is None:
            return
        dest_node = self._get_dest_node(*args)
        dest_node.text = value

    def set_curr_list(self, values, *args):
        """
        set a list of sub element for the element by its path.
        :param values: the list of values of the element.
        :param args: the path of the element.
        :return: None.
        """
        if self._curr_xml_tree is None:
            return
        tag = args[-1]
        args = args[:-1]
        dest_node = self._get_dest_node(*args)
        new_node_desc = None
        for node in list(dest_node):
            if node.tag == tag:
                if 'desc' in node.attrib:
                    new_node_desc = node.attrib['desc']
                dest_node.remove(node)
        for value in values:
            new_node = etree.SubElement(dest_node, tag)
            new_node.text = value
            if new_node_desc is not None:
                new_node.attrib['desc'] = new_node_desc

    def set_curr_attr(self, attr_name, attr_value, *args):
        """
        set the attribute of the element by its path.
        :param attr_name: the attribute name of the element.
        :param attr_value: the attribute value of the element.
        :param args: the path of the element.
        :return: None.
        """
        if self._curr_xml_tree is None:
            return
        dest_node = self._get_dest_node(*args)
        dest_node.attrib[attr_name] = attr_value

    def add_curr_value(self, key, desc, value, *args):
        """
        add a sub element for the element by its path.
        :param key: the tag of the sub element.
        :param desc: the attribute desc of the sub element.
        :param value: the value of the sub element.
        :param args: the path of the element.
        :return: None.
        """
        if self._curr_xml_tree is None:
            return

        dest_node = self._get_dest_node(*args)

        if key in ['vm']:
            etree.SubElement(dest_node, key, attrib={'id': value, 'desc': desc})
        else:
            new_node = etree.SubElement(dest_node, key, attrib={'desc': desc})
            new_node.text = value

    def get_curr_elem(self, *args):
        """
        get elements for current path.
        :param args: the path of the element.
        :return: current element.
        """
        if self._curr_xml_tree is None:
            return

        dest_node = self._get_dest_node(*args)
        return dest_node

    def clone_curr_elem(self, elem, *args):
        """
        clone elements for current path.
        :param elem: the element to clone.
        :param args: the path of the element.
        :return: None.
        """
        if self._curr_xml_tree is None:
            return

        dest_node = self._get_dest_node(*args)
        dest_node.append(elem)

    def insert_curr_elem(self, index, elem, *args):
        """
        insert elements for current path.
        :param index: the location for the element to insert.
        :param elem: the element to insert.
        :param args: the path of the element.
        :return: None.
        """
        if self._curr_xml_tree is None:
            return

        dest_node = self._get_dest_node(*args)
        dest_node.insert(index, elem)

    def delete_curr_elem(self, *args):
        """
        delete the element by its path.
        :param args: the path of the element.
        :return: None.
        """
        if self._curr_xml_tree is None:
            return
        father_node = self._get_dest_node(*args[:-1])
        dest_node = self._get_dest_node(*args)
        father_node.remove(dest_node)

    def delete_curr_key(self, *args):
        """
        delete the element by its path.
        :param args: the path of the element.
        :return: None.
        """
        if self._curr_xml_tree is None:
            return
        dest_node = self._get_dest_node(*args)
        self._curr_xml_tree.getroot().remove(dest_node)

    def _get_dest_node(self, *args):
        """
        get the destination element by its path.
        :param args: the path of the element.
        :return: the destination element.
        """
        if self._curr_xml_tree is None:
            return None
        dest_node = self._curr_xml_tree.getroot()
        path = '.'
        for arg in args:
            # tag:attr=xxx
            # tag:attr
            # tag
            tag = None
            attr_name = None
            attr_value = None
            if ':' not in arg:
                tag = arg
            elif '=' not in arg:
                # tag = arg.split(':')[0]
                # attr_name = arg.split(':')[1]
                raise Exception('unsupported xml path: tag:attr')
            else:
                tag = arg.split(':')[0]
                attr = arg.split(':')[1]
                attr_name = attr.split('=')[0]
                attr_value = attr.split('=')[1]

            if attr_value is None:
                path += ("/" + tag)
            else:
                path += ("/" + tag + "[@" + attr_name + "='" + attr_value + "']")

        dest_node = dest_node.findall(path)
        if dest_node is not None and dest_node != []:
            return dest_node[0]

        raise Exception('can not find node by {} from xml'.format(args))

    def save(self, xml=None, user_defined=False):
        """
        save current xml to file.
        :param xml: the file name to save; if not specified, save current xml to default names.
        :param user_defined: save to user defined folder or default folder.
        :return: None.
        """
        if self._curr_xml_tree is None:
            return
        if xml is None:
            xml = self._curr_xml

        xml_path = self._xml_path
        if user_defined:
            xml_path = os.path.join(self._xml_path, 'user_defined')
        if not os.path.isdir(xml_path):
            os.makedirs(xml_path)

        self._curr_xml_tree.write(os.path.join(xml_path, xml+'.xml'), encoding='utf-8', pretty_print=True)

    def _format_xml(self, element, depth=0):
        i = "\n" + depth * "    "
        if element:
            if not element.text or not element.text.strip():
                element.text = i + "    "
            if not element.tail or not element.tail.strip():
                element.tail = i
            for element in element:
                self._format_xml(element, depth + 1)
            if not element.tail or not element.tail.strip():
                element.tail = i
        else:
            if depth and (not element.tail or not element.tail.strip()):
                element.tail = i


def get_acrn_config_element(xsd_file):
    """
    return the root element for the xsd file
    :param xsd_file: the input xsd schema file
    :return: the root element of the xsd file
    """
    # schema = XMLSchema11(xsd_file)
    xsd_doc = etree.parse(xsd_file)
    xsd_doc.xinclude()
    schema = XMLSchema11(etree.tostring(xsd_doc, encoding="unicode"))

    xsd_element_root = schema.root_elements[0]
    acrn_config_element_root = xsd_2_acrn_config_element(xsd_element_root)
    # doc_dict = acrn_config_element_2_doc_dict(acrn_config_element_root, {})
    # enum_dict = acrn_config_element_2_enum_dict(acrn_config_element_root, {})
    # xpath_dict = acrn_config_element_2_xpath_dict(acrn_config_element_root, {})

    # from pprint import pprint
    # pprint(acrn_config_element_root)
    # pprint(xpath_dict)

    return acrn_config_element_root


def xsd_2_acrn_config_element(xsd_element, layer=0, index=0, path=''):
    """
    translate XSD element to ACRN config element
    :param xsd_element: the xsd element
    :param layer: current layer
    :param index: current index of current layer
    :param path: path of current element
    :return: ACRN config element
    """
    acrn_config_element = {
        'name': xsd_element.name,
        'type': None,
        'path': path+'/'+xsd_element.name,
        'layer': layer,
        'index': index,
        'doc': None,
        'configurable': 'y',
        'readonly': 'n',
        'multiselect': 'n',
        'default': xsd_element.default,
        'attributes': None,
        # 'minOccurs': None,
        # 'maxOccurs': None,
        'enumeration': None,
        'sub_elements': None
    }
    if isinstance(xsd_element.type, Xsd11ComplexType):
        acrn_config_element['type'] = xsd_element.type.name
        for xsd_component in xsd_element.type.iter_components():
            if isinstance(xsd_component, Xsd11Group):
                if acrn_config_element['sub_elements'] is None:
                    acrn_config_element['sub_elements'] = {'all':[], 'choice':[], 'sequence':[]}
                index = 0
                for sub_xsd_component in xsd_component.iter_components():
                    if isinstance(sub_xsd_component, Xsd11Element):
                        sub_acrn_config_element = xsd_2_acrn_config_element(sub_xsd_component, layer+1,
                                                                            index, path+'/'+xsd_element.name)
                        acrn_config_element['sub_elements'][xsd_component.model].append(sub_acrn_config_element)
                        index += 1
    else:
        if isinstance(xsd_element.type, XsdAtomicBuiltin):
            acrn_config_element['type'] = xsd_element.type.name
        elif isinstance(xsd_element.type.base_type, XsdSimpleType):
            acrn_config_element['type'] = xsd_element.type.base_type.name
        else:
            acrn_config_element['type'] = xsd_element.type.name
        if xsd_element.type.enumeration:
            acrn_config_element['enumeration'] = xsd_element.type.enumeration

    annotation = None
    if hasattr(xsd_element, 'annotation') and xsd_element.annotation:
        annotation = xsd_element.annotation
    elif hasattr(xsd_element.type, 'annotation') and xsd_element.type.annotation:
        annotation = xsd_element.type.annotation
    if annotation:
        if annotation.documentation:
            doc_list = [documentation.text for documentation in annotation.documentation]
            acrn_config_element['doc'] = '\n'.join(doc_list)
        for key in annotation.elem.keys():
            if key.endswith('configurable'):
                acrn_config_element['configurable'] = annotation.elem.get(key)
            elif key.endswith('readonly'):
                acrn_config_element['readonly'] = annotation.elem.get(key)
            elif key.endswith('multiselect'):
                acrn_config_element['multiselect'] = annotation.elem.get(key)

    if xsd_element.attributes:
        attrs = []
        for attr in xsd_element.attributes.iter_components():
            if isinstance(attr, Xsd11Attribute):
                attrs.append({'name': attr.name, 'type': attr.type.name})
        acrn_config_element['attributes'] = attrs

    return acrn_config_element

def acrn_config_element_2_doc_dict(acrn_config_element, doc_dict):
    """
    get the dictionary for documentation of all configurable elements by ACRN config element.
    :param acrn_config_element: the ACRN config element
    :param doc_dict: the dictionary to save documentation of all configurable elements
    :return: the dictionary to save documentation of all configurable elements
    """
    if 'doc' in acrn_config_element and 'path' in acrn_config_element \
        and acrn_config_element['path'] not in doc_dict:
        if acrn_config_element['doc']:
            doc_dict[acrn_config_element['path']] = acrn_config_element['doc']
        else:
            doc_dict[acrn_config_element['path']] = acrn_config_element['name']

    if 'sub_elements' in acrn_config_element and acrn_config_element['sub_elements']:
        for order_type in acrn_config_element['sub_elements']:
            for element in acrn_config_element['sub_elements'][order_type]:
                doc_dict = acrn_config_element_2_doc_dict(element, doc_dict)
    return doc_dict

def acrn_config_element_2_enum_dict(acrn_config_element, enum_dict):
    """
    get the dictionary for enumeration of all configurable elements by ACRN config element.
    :param acrn_config_element: the ACRN config element
    :param enum_dict: the dictionary to save enumeration of all configurable elements
    :return: the dictionary to save enumeration of all configurable elements
    """
    if 'enumeration' in acrn_config_element and 'path' in acrn_config_element \
        and acrn_config_element['path'] not in enum_dict \
        and acrn_config_element['enumeration']:
        enum_dict[acrn_config_element['path']] = acrn_config_element['enumeration']
    if 'sub_elements' in acrn_config_element and acrn_config_element['sub_elements']:
        for order_type in acrn_config_element['sub_elements']:
            for element in acrn_config_element['sub_elements'][order_type]:
                enum_dict = acrn_config_element_2_enum_dict(element, enum_dict)
    return enum_dict

def acrn_config_element_2_xpath_dict(acrn_config_element, xpath_dict):
    """
    get the dictionary for xpath of all configurable elements by ACRN config element.
    :param acrn_config_element: the ACRN config element
    :param xpath_dict: the dictionary to save xpath of all configurable elements
    :return: the dictionary to save xpath of all configurable elements
    """
    if acrn_config_element['path'] not in xpath_dict.keys():
        xpath_dict[acrn_config_element['path']] = {
            'name': acrn_config_element['name'],
            'type': acrn_config_element['type'],
            'layer': acrn_config_element['layer'],
            'index': acrn_config_element['index'],
            'doc': acrn_config_element['doc'] if acrn_config_element['doc'] else acrn_config_element['name'],
            'configurable': acrn_config_element['configurable'],
            'readonly': acrn_config_element['readonly'],
            'multiselect': acrn_config_element['multiselect'],
            'default': acrn_config_element['default'],
            'attributes': acrn_config_element['attributes'],
            # 'minOccurs': None,
            # 'maxOccurs': None,
            'enumeration': acrn_config_element['enumeration']
        }
    if 'sub_elements' in acrn_config_element and acrn_config_element['sub_elements']:
        for order_type in acrn_config_element['sub_elements']:
            for element in acrn_config_element['sub_elements'][order_type]:
                enum_dict = acrn_config_element_2_xpath_dict(element, xpath_dict)
    return xpath_dict
