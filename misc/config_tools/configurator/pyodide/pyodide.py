#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

import json
import sys
from pathlib import Path


class LazyPath:
    def __init__(self, my):
        self.my = my

    def __truediv__(self, other):
        return str(self.my / other)


def file_text(path):
    return open(path, encoding='utf-8').read()


# path define
config_tools_dir = Path(__file__).absolute().parent.parent.parent
configurator_dir = config_tools_dir / 'configurator' / 'packages' / 'configurator'
schema_dir = config_tools_dir / 'schema'
board_xml_schema_path = schema_dir / 'board.xsd'
scenario_xml_schema_path = schema_dir / 'sliced.xsd'
datachecks_xml_schema_path = schema_dir / 'allchecks.xsd'

nuc11_folder = LazyPath(config_tools_dir / 'data' / 'nuc11tnbi5')
nuc11_board_path = nuc11_folder / 'nuc11tnbi5.xml'

# file define
nuc11_board = file_text(nuc11_folder / 'nuc11tnbi5.xml')
nuc11_scenario = file_text(nuc11_folder / 'shared_launch_6user_vm.xml')
scenario_json_schema = file_text(configurator_dir / 'build' / 'assets' / 'scenario.json')

IS_WEB = sys.platform == 'emscripten'


def convert_result(result):
    if not IS_WEB:
        print(json.dumps(result, indent='  '))
    return json.dumps(result)


def write_temp_file(tmpdir, file_dict: dict):
    temp_path = Path(tmpdir)
    for filename, content in file_dict.items():
        with open(temp_path / filename, 'w', encoding='utf-8') as f:
            f.write(content)


def main():
    pass


def test():
    pass


if __name__ == '__main__':
    test()
