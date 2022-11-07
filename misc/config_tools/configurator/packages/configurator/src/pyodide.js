import {loadPyodide} from "/thirdLib/pyodide/pyodide";
import scenarioJSONSchema from '../build/assets/scenario.json';

window.__dynamic__load__scenario__from__pyodide__ = () => {
    return JSON.stringify(scenarioJSONSchema)
}


export default async function () {
    let pyodide = await loadPyodide({
        indexURL: '/thirdLib/pyodide/'
    });
    await pyodide.loadPackage(['micropip', 'lxml', 'beautifulsoup4'])
    await pyodide.runPythonAsync(`
        import micropip
        await micropip.install([
            './thirdLib/xmltodict-0.12.0-py2.py3-none-any.whl',
            './thirdLib/elementpath-2.5.0-py3-none-any.whl',
            './thirdLib/defusedxml-0.7.1-py2.py3-none-any.whl',
            './thirdLib/xmlschema-1.9.2-py3-none-any.whl',
            './thirdLib/acrn_config_tools-3.0-py3-none-any.whl',
            './thirdLib/rstcloth-0.5.2-py3-none-any.whl'
        ])
    `)

    function test() {
        let result = pyodide.runPython(`
            import sys
            sys.version
        `)
        console.log(result);
    }

    test()

    // pyodide load success
    window.pyodide = pyodide;
}
