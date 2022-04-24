#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

import os
from tempfile import TemporaryDirectory
from pathlib import Path

from launch_config.launch_cfg_gen import main as launch_cfg_gen_main

from .pyodide import convert_result, nuc11_board, nuc11_scenario, write_temp_file


def generate_launch_script(board, scenario, user_vm_id=0):
    """

    :param board: board xml text
    :param scenario: scenario xml text
    :param user_vm_id: the vm which you want to generate launch script, will generate all launch script if it set to zero
    """
    launch_scripts = {}
    with TemporaryDirectory() as tmpdir:
        # Write file to dir
        write_temp_file(tmpdir, {
            'board.xml': board,
            'scenario.xml': scenario
        })

        # define path
        board_file_path = Path(tmpdir) / 'board.xml'
        scenario_file_path = Path(tmpdir) / 'scenario.xml'
        launch_script_output_dir = Path(tmpdir) / 'output'

        # generate launch script
        launch_cfg_gen_main(board_file_path, scenario_file_path, user_vm_id, launch_script_output_dir)

        # get output and convert it to {filename: content}
        for filename in os.listdir(launch_script_output_dir):
            abs_name = launch_script_output_dir / str(filename)
            launch_scripts[filename] = open(abs_name, encoding='utf-8').read()
    return convert_result(launch_scripts)


main = generate_launch_script


def test():
    main(nuc11_board, nuc11_scenario)


if __name__ == '__main__':
    test()
