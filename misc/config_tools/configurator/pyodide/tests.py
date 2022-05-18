#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

from .loadBoard import test as load_board_test
from .loadScenario import test as load_scenario_test
from .generateLaunchScript import test as generate_launch_script_test
from .validateBoardStructure import test as validate_board_structure_test
from .validateScenarioStructure import test as validate_scenario_structure_test
from .validateScenario import test as validate_scenario_test
from .populateDefaultValues import test as populate_default_values


def main():
    load_board_test()
    load_scenario_test()
    generate_launch_script_test()
    validate_board_structure_test()
    validate_scenario_structure_test()
    validate_scenario_test()
    populate_default_values()


def test():
    main()


if __name__ == '__main__':
    test()
