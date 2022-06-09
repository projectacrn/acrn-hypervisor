#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

from pathlib import Path
from tempfile import TemporaryDirectory

from scenario_config.default_populator import DefaultValuePopulatingStage
from scenario_config.pipeline import PipelineObject, PipelineEngine
from scenario_config.validator import ValidatorConstructionByFileStage, SemanticValidationStage, \
    SyntacticValidationStage
from scenario_config.xml_loader import XMLLoadStage

from .pyodide import (
    convert_result, write_temp_file,
    nuc11_board, nuc11_scenario, scenario_xml_schema_path, datachecks_xml_schema_path
)


def main(board, scenario, completed_verify=False):
    pipeline = PipelineEngine(["board_path", "scenario_path", "schema_path", "datachecks_path"])
    stages = [
        ValidatorConstructionByFileStage(),
        XMLLoadStage("schema"),

        XMLLoadStage("board"),
        XMLLoadStage("scenario"),
        DefaultValuePopulatingStage(),
        SyntacticValidationStage()
    ]
    if completed_verify:
        stages.append(SemanticValidationStage())

    pipeline.add_stages(stages)
    with TemporaryDirectory() as tmpdir:
        write_temp_file(tmpdir, {
            'board.xml': board,
            'scenario.xml': scenario
        })
        board_file_path = Path(tmpdir) / 'board.xml'
        scenario_file_path = Path(tmpdir) / 'scenario.xml'

        obj = PipelineObject(
            board_path=board_file_path,
            scenario_path=scenario_file_path,
            schema_path=scenario_xml_schema_path,
            datachecks_path=datachecks_xml_schema_path
        )
        pipeline.run(obj)

        validate_result: list = obj.get("syntactic_errors")
        if completed_verify:
            validate_result.extend(obj.get("semantic_errors"))
        return convert_result(validate_result)


def test():
    main(nuc11_board, nuc11_scenario)


if __name__ == '__main__':
    test()
