#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

import json
import logging
import re
import os
from copy import deepcopy

import elementpath
import lxml.etree as etree
from bs4 import BeautifulSoup

from . import convert_result, nuc11_board, scenario_json_schema, nuc11_board_path


def get_dynamic_scenario(board):
    """

    :type board: str
    :param board: board xml text
    """
    board_xml = etree.fromstring(board)

    def get_enum(source, options, option_names, obj_type):
        elements = [str(x) for x in elementpath.select(source, options) if x]
        element_names = [str(x) for x in elementpath.select(source, option_names) if x]
        elements = list(set(zip(elements, element_names)))
        if not elements:
            elements = [('', '')]
        # TODO: Add more converters if needed
        enum_type_convert = {'integer': lambda x: int(x) if x else 0}
        if obj_type in enum_type_convert.keys():
            elements = [(enum_type_convert[obj_type](x[0]), x[1]) for x in elements]
        return elements

    def dynamic_enum(**enum_setting):
        # value from env
        function, source = [
            {"get_enum": get_enum, "board_xml": board_xml}[enum_setting[key]]
            for key in ['function', 'source']
        ]
        # value from given
        selector, name_selector, sorted_func, obj_type = [enum_setting[key] for key in ['selector', 'name-selector', 'sorted', 'type']]

        # get enum data
        enum = function(source, selector, name_selector, obj_type)
        if sorted_func:
            fn = eval(sorted_func)
            enum = sorted(enum, key=lambda x: fn(x[0]))
        return zip(*enum)

    def dynamic_enum_apply(obj):
        # get json schema enum obj
        if 'enum' in obj and isinstance(obj['enum'], dict):
            enum_setting = obj['enum']
            # check enum obj type
            if enum_setting['type'] == 'dynamicEnum':
                enum_setting['type'] = obj.get('type', '')
                # replace json schema obj enum field data
                enum, enum_names = dynamic_enum(**enum_setting)
                obj['enum'] = enum
                obj['enumNames'] = enum_names
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
                if key == 'required' and key not in current_form_type_schema_obj:
                    form_schema[key] = []
                    continue
                form_schema[key] = current_form_type_schema_obj[key]
            form_schemas[tab_type][form_type] = form_schema

    return form_schemas


def get_cat_info(soup):
    threads = soup.select('core thread')
    threads = {thread.attrs['id']: thread.select_one('cpu_id').text for thread in threads}
    caches = soup.select('caches cache')
    cat_info = []
    for cache in caches:
        cache_level = int(cache.attrs['level'])

        # ignore cache_level 1 and single core cache region
        if cache_level == 1 or len(processors := cache.select('processors processor')) <= 1:
            continue
        # ignore no CAT capability cache region
        if cache.select_one('#CAT') is None:
            continue

        capacity_mask_length = cache.select_one('capability capacity_mask_length')
        capacity_mask_length = int(capacity_mask_length.text)

        processors = [int(threads[processor.text]) for processor in processors]
        processors.sort()
        cache_info = {
            'id': cache.attrs['id'],
            'level': cache_level,
            'type': cache.attrs['type'],
            'cache_size': int(cache.select_one('cache_size').text),
            'capacity_mask_length': capacity_mask_length,
            'processors': processors,
        }
        cat_info.append(cache_info)
    cat_info.sort(key=lambda x: int(x['id'], 16))
    cat_info.sort(key=lambda x: x['level'], reverse=True)
    return cat_info


def get_board_info(board, path):
    soup = BeautifulSoup(board, 'xml')
    # Workaround: The pyodide thinks it runs under os.name == posix.
    # So os.path.basename will not work on windows.
    # Here we replace all '\' with '/' to make it work (most of the time).
    try:
        path = path.replace('\\', '/')
        fname = os.path.basename(path)
        basename, _ = os.path.splitext(fname)
        board_name = basename + '.xml' if basename.endswith('.board') \
            else basename + '.board.xml'
    except Exception as e:
        logging.warning(e)
        board_name = 'default'
    result = {
        'name': board_name,
        'content': board,
        'CAT_INFO': get_cat_info(soup),
        'BIOS_INFO': soup.select_one('BIOS_INFO').text,
        'BASE_BOARD_INFO': soup.select_one('BASE_BOARD_INFO').text
    }
    return result


def load_board(board, path):
    result = {
        'scenarioJSONSchema': get_dynamic_scenario(board),
        'boardInfo': get_board_info(board, path)
    }
    return convert_result(result)


def test():
    load_board(nuc11_board, nuc11_board_path)


main = load_board

if __name__ == '__main__':
    test()
