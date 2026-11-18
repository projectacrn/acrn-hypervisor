#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

import json

import xmltodict

from . import convert_result, nuc11_scenario, scenario_json_schema as default_scenario_json_schema


def get_array_and_int_keys(scenario_json_schema=None):
    if scenario_json_schema is None:
        scenario_json_schema = default_scenario_json_schema
    array_keys = []
    int_keys = ['@id']

    def object_mapper(obj):
        if not isinstance(obj, dict):
            return obj
        for key, value in obj.items():
            if not isinstance(value, dict):
                continue
            if value.get('type', '') == 'array':
                array_keys.append(key)
            elif value.get('type', '') == 'integer':
                int_keys.append(key)
        return obj

    json.loads(scenario_json_schema, object_hook=object_mapper)
    array_keys = list(set(array_keys))
    int_keys = list(set(int_keys))
    return array_keys, int_keys


def load_scenario_xml(scenario, scenario_json_schema=None):
    """
    convert scenario xml to json data followed scenario json schema

    :type scenario: str
    :param scenario: scenario xml text
    :param scenario_json_schema: schema supplied by the engine caller
    :return:
    """
    if scenario_json_schema is None:
        scenario_json_schema = default_scenario_json_schema
    arr_keys, int_keys = get_array_and_int_keys(scenario_json_schema)
    scenario_xml = xmltodict.parse(scenario)
    scenario_xml = json.dumps(scenario_xml)

    def correct_struct(obj):
        if not isinstance(obj, dict):
            return obj
        keys_need_remove = []
        for key, value in obj.items():
            if value is None:
                keys_need_remove.append(key)
                continue
            if key in int_keys:
                if isinstance(obj[key], list):
                    obj[key] = [int(x) for x in obj[key]]
                elif not isinstance(value, int):
                    obj[key] = int(value)
            if key in arr_keys and not isinstance(value, list):
                obj[key] = [value]
        for key in keys_need_remove:
            del obj[key]
        return obj

    scenario_dict = json.loads(scenario_xml, object_hook=correct_struct)
    return scenario_dict


def main(scenario, scenario_json_schema=None):
    result = load_scenario_xml(scenario, scenario_json_schema)
    return convert_result(result)


def test():
    main(nuc11_scenario)


if __name__ == '__main__':
    test()
