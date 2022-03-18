import dynamicScenario from "../assets/schema/dynamicScenario.py"
import scenario_json from "../assets/schema/scenario.json"
import {Base64} from 'js-base64';


function toJSONString(obj) {
    if (_.isString(obj)) {
        return JSON.stringify(obj)
    }
    return JSON.stringify(JSON.stringify(obj))
}


async function initPyodide() {
    await pyodide.runPythonAsync(
        `import micropip
await micropip.install(['elementpath', 'xmlschema'])
`)
    return pyodide
}

initPyodide()

function runPyCode(pythonCode) {
    pyodide.loadPackagesFromImports(pythonCode)
    return pyodide.runPython(pythonCode)
}

function writeFile(filename, content) {
    let file_content = JSON.stringify(content)
    return runPyCode(
        `import json; open(${toJSONString(filename)},'w',encoding='utf-8').write(json.loads(${toJSONString(file_content)}))`
    )
}

function readFile(filename) {
    return runPyCode(
        `open(${toJSONString(filename)},'r',encoding='utf-8').read()`
    )
}

function loadLibrary(libraryName, content) {
    return writeFile(`/lib/python3.9/${libraryName}.py`, content)
}

function getNewSchema(boardXMLText) {
    let params = JSON.stringify({board_xml: boardXMLText, scenario_json: JSON.stringify(scenario_json)})
    params = Base64.encode(params);
    let scenario_text = runPyCode(`
params="${params}"
${dynamicScenario}
`)
    let new_scenario_json = JSON.parse(scenario_text)
    console.log(new_scenario_json)
    return new_scenario_json
}

export {runPyCode, readFile, writeFile, getNewSchema}