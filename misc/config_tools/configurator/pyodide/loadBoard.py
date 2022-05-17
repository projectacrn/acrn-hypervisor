#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

import json
from copy import deepcopy

import elementpath
import lxml.etree as etree
from bs4 import BeautifulSoup

from . import convert_result, nuc11_board, scenario_json_schema


def get_dynamic_scenario(board):
    """

    :type board: str
    :param board: board xml text
    """
    board_xml = etree.fromstring(board)

    def get_enum(source, options, obj_type):
        elements = [str(x) for x in elementpath.select(source, options) if x]
        elements = list(set(elements))
        if not elements:
            elements = ['']
        # TODO: Add more converters if needed
        enum_type_convert = {'integer': int}
        if obj_type in enum_type_convert.keys():
            elements = [enum_type_convert[obj_type](x) for x in elements]
        return elements

    def dynamic_enum(**enum_setting):
        # value from env
        function, source = [
            {"get_enum": get_enum, "board_xml": board_xml}[enum_setting[key]]
            for key in ['function', 'source']
        ]
        # value from given
        selector, sorted_func, obj_type = [enum_setting[key] for key in ['selector', 'sorted', 'type']]

        # get enum data
        enum = function(source, selector, obj_type)
        if sorted_func:
            enum = sorted(enum, key=eval(sorted_func))
        return enum

    def dynamic_enum_apply(obj):
        # get json schema enum obj
        if 'enum' in obj and isinstance(obj['enum'], dict):
            enum_setting = obj['enum']
            # check enum obj type
            if enum_setting['type'] == 'dynamicEnum':
                enum_setting['type'] = obj.get('type', '')
                # replace json schema obj enum field data
                obj['enum'] = dynamic_enum(**enum_setting)
        return obj

    data = json.loads(scenario_json_schema, object_hook=dynamic_enum_apply)

    form_schemas = {}
    tab_types = ['HV', 'PreLaunchedVM', 'ServiceVM', 'PostLaunchedVM']
    form_types = ['BasicConfigType', 'AdvancedConfigType']
    for tab_type in tab_types:
        form_schemas[tab_type] = {}
        for form_type in form_types:
            form_schema = deepcopy(data)
            current_form_type_schema_obj = form_schema['definitions'][f'{tab_type}{form_type}']
            for key in ['type', 'required', 'properties']:
                form_schema[key] = current_form_type_schema_obj[key]
            form_schemas[tab_type][form_type] = form_schema

    return form_schemas


def get_board_info(board):
    soup = BeautifulSoup(board, 'xml')
    result = {
        'name': soup.select_one('acrn-config').attrs['board'] + '.board.xml',
        'content': board,
        'BIOS_INFO': soup.select_one('BIOS_INFO').text,
        'BASE_BOARD_INFO': soup.select_one('BASE_BOARD_INFO').text
    }
    return result


def load_board(board):
    result = {
        'scenarioJSONSchema': get_dynamic_scenario(board),
        'boardInfo': get_board_info(board)
    }
    return convert_result(result)


def test():
    load_board(nuc11_board)


main = load_board

if __name__ == '__main__':
    test()
