"""

public var board_xml
public function get_enum(source, selector)

enum = {
    'type': 'dynamicEnum',
    'function': 'get_enum',
    'source': 'board_xml',
    'selector': element['xs:annotation']['@acrn:options'],
    'sorted': element['xs:annotation'].get('@acrn:options-sorted-by', None)
}

params defined before this document string in js side like
```js
let params = JSON.stringify({board_xml: boardXMLText, scenario_json: JSON.stringify(scenario_json)})
params = Base64.encode(params);
return runPyCode(`
params="${params}"
${dynamicScenario}
`)
```
"""

import json
from base64 import b64decode

import elementpath
import lxml.etree as etree

# local test var set
if 'params' not in globals():
    # params = b64encode(open(r"board.xml",'rb').read())
    raise NotImplementedError

# Main flow
# noinspection PyUnresolvedReferences,PyUnboundLocalVariable
params: str = b64decode(params).decode('utf-8')
params: dict = json.loads(params)
board_xml = etree.XML(params["board_xml"])
scenario_json = params['scenario_json']


def get_enum(source, options):
    elements = [str(x) for x in elementpath.select(source, options) if x]
    elements = list(set(elements))
    if not elements:
        elements = ['']
    return elements


def dynamic_enum(**kwargs):
    # value from env
    function, source = [globals()[kwargs[key]] for key in ['function', 'source']]
    # value from given
    selector, sorted_func = [kwargs[key] for key in ['selector', 'sorted']]

    # get enum data
    enum = function(source, selector)
    if sorted_func:
        enum = sorted(enum, key=eval(sorted_func))
    return enum


def dynamic_enum_apply(obj):
    # get json schema enum obj
    if 'enum' in obj and isinstance(obj['enum'], dict):
        enum_setting = obj['enum']
        # check enum obj type
        if enum_setting['type'] == 'dynamicEnum':
            del enum_setting['type']
            # replace json schema obj enum field data
            obj['enum'] = dynamic_enum(**enum_setting)
    return obj


data = json.loads(scenario_json, object_hook=dynamic_enum_apply)
json.dumps(data)
