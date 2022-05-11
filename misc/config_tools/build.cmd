@echo off
python scenario_config/schema_slicer.py || exit /b
python scenario_config/jsonschema/converter.py || exit /b
xmllint --xinclude schema/datachecks.xsd > schema/allchecks.xsd || exit /b
python -m build || exit /b
rem pip install .\dist\acrn_config_tools-3.0-py3-none-any.whl --force-reinstall
del .\configurator\packages\configurator\thirdLib\acrn_config_tools-3.0-py3-none-any.whl
cd configurator
python .\packages\configurator\thirdLib\manager.py install || exit /b
yarn
echo build and install success
