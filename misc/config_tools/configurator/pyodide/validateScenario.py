#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

from pathlib import Path
from tempfile import TemporaryDirectory

from scenario_config.default_populator import DefaultValuePopulatingStage
from scenario_config.pipeline import PipelineObject, PipelineEngine
from scenario_config.validator import ValidatorConstructionByFileStage, SemanticValidationStage, \
    SyntacticValidationStage
from scenario_config.lxml_loader import LXMLLoadStage

from .pyodide import (
    convert_result, write_temp_file,
    # Todo: add debug switch
    # is_debug,
    nuc11_board, nuc11_scenario, scenario_xml_schema_path, datachecks_xml_schema_path
)


def main(board, scenario):
    pipeline = PipelineEngine(["board_path", "scenario_path", "schema_path", "datachecks_path"])
    stages = [
        ValidatorConstructionByFileStage(),
        LXMLLoadStage("schema"),

        LXMLLoadStage("board"),
        LXMLLoadStage("scenario"),
        DefaultValuePopulatingStage(),
        SyntacticValidationStage(),
        SemanticValidationStage(),
    ]

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

        syntactic_errors = obj.get("syntactic_errors")
        semantic_errors = obj.get("semantic_errors")
        return convert_result({"syntactic_errors": syntactic_errors, "semantic_errors": semantic_errors})


def test():
    main(nuc11_board, nuc11_scenario)


if __name__ == '__main__':
    test()
