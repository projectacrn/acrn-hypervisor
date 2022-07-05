python3 scenario_config/schema_slicer.py
python3 scenario_config/jsonschema/converter.py
xmllint --xinclude schema/datachecks.xsd > schema/allchecks.xsd 
python3 -m build 
rm ./configurator/packages/configurator/thirdLib/acrn_config_tools-3.0-py3-none-any.whl
cd configurator
python3 ./packages/configurator/thirdLib/manager.py install 
yarn 
echo build and install success
