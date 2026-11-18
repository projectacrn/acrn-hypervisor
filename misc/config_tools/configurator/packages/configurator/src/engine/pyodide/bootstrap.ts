// These assets are generated/copied before the frontend build.
// @ts-ignore
import {loadPyodide} from "/thirdLib/pyodide/pyodide";
// @ts-ignore
import scenarioJSONSchema from '../../../build/assets/scenario.json';
import {configureScenarioJsonSchema} from "./client";

// Retain the existing global for compatibility during P1. Python no longer
// imports it; the client passes this schema explicitly with every request that
// needs it.
(window as any).__dynamic__load__scenario__from__pyodide__ = () => JSON.stringify(scenarioJSONSchema);
configureScenarioJsonSchema(scenarioJSONSchema);

export default async function bootstrapPyodide(): Promise<void> {
    const pyodide = await loadPyodide({
        indexURL: '/thirdLib/pyodide/'
    });
    await pyodide.loadPackage(['micropip', 'lxml', 'beautifulsoup4']);
    await pyodide.runPythonAsync(`
        import micropip
        await micropip.install([
            './thirdLib/xmltodict-0.12.0-py2.py3-none-any.whl',
            './thirdLib/elementpath-2.5.0-py3-none-any.whl',
            './thirdLib/defusedxml-0.7.1-py2.py3-none-any.whl',
            './thirdLib/xmlschema-1.9.2-py3-none-any.whl',
            './thirdLib/acrn_config_tools-3.0-py3-none-any.whl'
        ])
    `);

    const version = pyodide.runPython(`
        import sys
        sys.version
    `);
    console.log(version);

    (window as any).pyodide = pyodide;
}
