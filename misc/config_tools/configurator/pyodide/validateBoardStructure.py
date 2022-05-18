#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

from pathlib import Path
from tempfile import TemporaryDirectory

from scenario_config.default_populator import DefaultValuePopulatingStage
from scenario_config.pipeline import PipelineObject, PipelineEngine
from scenario_config.validator import ValidatorConstructionByFileStage, SyntacticValidationStage
from scenario_config.xml_loader import XMLLoadStage

from .pyodide import (
    convert_result, write_temp_file,
    nuc11_board, board_xml_schema_path
)


def main(board):
    pipeline = PipelineEngine(["board_path", "schema_path", "datachecks_path"])
    pipeline.add_stages([
        ValidatorConstructionByFileStage(),
        XMLLoadStage("board"),
        SyntacticValidationStage(etree_tag = "board"),
    ])

    try:
        with TemporaryDirectory() as tmpdir:
            write_temp_file(tmpdir, {
                'board.xml': board
            })
            board_file_path = Path(tmpdir) / 'board.xml'

            obj = PipelineObject(
                board_path=board_file_path,
                schema_path=board_xml_schema_path,
                datachecks_path=None
            )
            pipeline.run(obj)

            validate_result = obj.get("syntactic_errors")
            return "\n\n".join(map(lambda x: x["message"], validate_result))
    except Exception as e:
        return str(e)


def test():
    print(main(nuc11_board))


if __name__ == '__main__':
    test()
