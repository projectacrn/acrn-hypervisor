#!/usr/bin/env python3
"""Single request dispatcher for the configurator's Pyodide engine."""
__package__ = 'configurator.pyodide'

import json

from .generateConfigSummary import main as generate_config_summary
from .generateLaunchScript import main as generate_launch_script
from .loadBoard import main as load_board
from .loadScenario import main as load_scenario
from .populateDefaultValues import main as populate_default_values
from .updateSchema import main as update_schema
from .validateBoardStructure import main as validate_board_structure
from .validateScenario import main as validate_scenario
from .validateScenarioStructure import main as validate_scenario_structure


def _load_board(payload):
    return load_board(payload['board'], payload['path'], payload['scenarioJsonSchema'])


def _load_scenario(payload):
    return load_scenario(payload['scenario'], payload['scenarioJsonSchema'])


def _validate_board_structure(payload):
    return validate_board_structure(payload['board'])


def _validate_scenario_structure(payload):
    return validate_scenario_structure(payload['scenario'])


def _validate_scenario(payload):
    return validate_scenario(payload['board'], payload['scenario'])


def _generate_launch_script(payload):
    return generate_launch_script(payload['board'], payload['scenario'])


def _populate_default_values(payload):
    return populate_default_values(payload['scenario'], payload['scenarioJsonSchema'])


def _generate_config_summary(payload):
    return generate_config_summary(payload['board'], payload['scenario'])


def _update_schema(payload):
    return update_schema(payload['board'], payload['scenario'])


METHOD_REGISTRY = {
    'loadBoard': _load_board,
    'loadScenario': _load_scenario,
    'validateBoardStructure': _validate_board_structure,
    'validateScenarioStructure': _validate_scenario_structure,
    'validateScenario': _validate_scenario,
    'generateLaunchScript': _generate_launch_script,
    'populateDefaultValues': _populate_default_values,
    'generateConfigSummary': _generate_config_summary,
    'updateSchema': _update_schema,
}


def dispatch(method, payload):
    """Dispatch one engine request without changing its existing result format."""
    if isinstance(payload, str):
        payload = json.loads(payload)
    if not isinstance(payload, dict):
        raise TypeError('Engine payload must be a JSON object.')
    try:
        handler = METHOD_REGISTRY[method]
    except KeyError as error:
        raise ValueError(f'Unknown engine method: {method}') from error
    return handler(payload)
