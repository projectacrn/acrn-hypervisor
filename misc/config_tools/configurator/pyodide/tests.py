#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

from .loadBoard import test as load_board_test
from .loadScenario import test as load_scenario_test
from .generateLaunchScript import test as generate_launch_script_test
from .validateScenario import test as validate_scenario_test


def main():
    load_board_test()
    load_scenario_test()
    generate_launch_script_test()
    validate_scenario_test()


def test():
    main()


if __name__ == '__main__':
    test()
