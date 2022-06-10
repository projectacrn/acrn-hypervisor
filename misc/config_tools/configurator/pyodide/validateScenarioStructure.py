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
    nuc11_scenario, schema_dir
)


def main(scenario):
    pipeline = PipelineEngine(["scenario_path", "schema_path", "datachecks_path"])
    pipeline.add_stages([
        ValidatorConstructionByFileStage(),
        XMLLoadStage("schema"),
        XMLLoadStage("scenario"),
        DefaultValuePopulatingStage(),
        SyntacticValidationStage(),
    ])

    try:
        with TemporaryDirectory() as tmpdir:
            write_temp_file(tmpdir, {
                'scenario.xml': scenario
            })
            scenario_file_path = Path(tmpdir) / 'scenario.xml'

            obj = PipelineObject(
                scenario_path=scenario_file_path,
                schema_path=schema_dir / 'scenario_structure.xsd',
                datachecks_path=None
            )
            pipeline.run(obj)

            validate_result = obj.get("syntactic_errors")
            return "\n\n".join(map(lambda x: x["message"], validate_result))
    except Exception as e:
        return str(e)


def test():
    print(main(nuc11_scenario))


if __name__ == '__main__':
    test()
