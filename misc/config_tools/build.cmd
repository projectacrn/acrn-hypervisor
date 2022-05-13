@echo off
python scenario_config/schema_slicer.py || exit 1
python scenario_config/jsonschema/converter.py || exit 1
xmllint --xinclude schema/datachecks.xsd > schema/allchecks.xsd || exit 1
python -m build || exit 1
rem pip install .\dist\acrn_config_tools-3.0-py3-none-any.whl --force-reinstall
del .\configurator\packages\configurator\thirdLib\acrn_config_tools-3.0-py3-none-any.whl
cd configurator
python .\packages\configurator\thirdLib\manager.py install || exit 1
yarn || exit 1
echo build and install success
