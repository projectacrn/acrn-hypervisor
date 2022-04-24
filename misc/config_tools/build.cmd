@echo off
python scenario_config/schema_slicer.py
python scenario_config/jsonschema/converter.py
xmllint --xinclude schema/datachecks.xsd > schema/allchecks.xsd
python -m build
rem pip install .\dist\acrn_config_tools-3.0-py3-none-any.whl --force-reinstall
del .\configurator\thirdLib\acrn_config_tools-3.0-py3-none-any.whl
python .\configurator\thirdLib\manager.py install
echo build and install success
