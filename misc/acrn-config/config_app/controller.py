# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""Controller for config app.

"""

import os
import xml.etree.ElementTree as ElementTree


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
            tree = ElementTree.parse(xml_file)
            root = tree.getroot()
            if 'uos_launcher' in root.attrib:
                xml_type = 'uos_launcher'
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
        if self._xml_path is None:
            return
        try:
            self._curr_xml = xml

            xml_path = os.path.join(self._xml_path, self._curr_xml + '.xml') \
                if self._default \
                else os.path.join(self._xml_path, 'user_defined', self._curr_xml + '.xml')

            tree = ElementTree.parse(xml_path)
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
        for node in dest_node.getchildren():
            dest_node.remove(node)
        for value in values:
            new_node = ElementTree.SubElement(dest_node, tag)
            new_node.text = value

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
            ElementTree.SubElement(dest_node, key, attrib={'id': value, 'desc': desc})
        else:
            new_node = ElementTree.SubElement(dest_node, key, attrib={'desc': desc})
            new_node.text = value

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

    def save(self, xml=None, user_defined=True):
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

        self._format_xml(self._curr_xml_tree.getroot())
        self._curr_xml_tree.write(os.path.join(xml_path, xml+'.xml'), encoding='utf-8',
                                  xml_declaration=True, method='xml')

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
