#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

import os
from tempfile import TemporaryDirectory
from pathlib import Path

from scenario_config.config_summary import main as config_summary_gen_main

from .pyodide import nuc11_board, nuc11_scenario, write_temp_file


def generate_config_summary(board, scenario):
    """

    :param board: board xml text
    :param scenario: scenario xml text
    """
    with TemporaryDirectory() as tmpdir:
        # Write file to dir
        write_temp_file(tmpdir, {
            'board.xml': board,
            'scenario.xml': scenario
        })

        # define path
        board_file_path = Path(tmpdir) / 'board.xml'
        scenario_file_path = Path(tmpdir) / 'scenario.xml'
        config_summary_path = Path(tmpdir) / 'config_summary.rst'

        # generate launch script
        config_summary_gen_main(board_file_path, scenario_file_path, config_summary_path)

        # get output and convert it to {filename: content}
        config_summary_content = open(config_summary_path, encoding='utf-8').read()
    return config_summary_content


main = generate_config_summary


def test():
    main(nuc11_board, nuc11_scenario)


if __name__ == '__main__':
    test()
