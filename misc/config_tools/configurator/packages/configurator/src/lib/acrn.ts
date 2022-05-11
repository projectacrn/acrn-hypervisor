import {dialog, invoke} from "@tauri-apps/api";
import JSON2XML from "./json2xml"
import {OpenDialogOptions} from "@tauri-apps/api/dialog";

enum HistoryType {
    WorkingFolder,
    Board,
    Scenario
}

export type HistoryTypeString = keyof typeof HistoryType;

class PythonObject {
    api(scriptName, ...params) {
        // @ts-ignore
        let pythonFunction = window.pyodide.pyimport(`configurator.pyodide.${scriptName}`);
        let result = pythonFunction.main(...params);
        return JSON.parse(result);
    }

    loadBoard(boardXMLText) {
        return this.api('loadBoard', boardXMLText)
    }

    loadScenario(scenarioXMLText) {
        return this.api('loadScenario', scenarioXMLText)
    }

    validateScenario(boardXMLText, scenarioXMLText) {
        return this.api('validateScenario', boardXMLText, scenarioXMLText)
    }

    generateLaunchScript(boardXMLText, scenarioXMLText) {
        return this.api('generateLaunchScript', boardXMLText, scenarioXMLText)
    }

    populateDefaultValues(scenarioXMLText) {
        return this.api('populateDefaultValues', scenarioXMLText)
    }
}

class Configurator {
    public pythonObject: PythonObject;

    constructor() {
        this.pythonObject = new PythonObject()
    }

    getHistory(historyType: HistoryTypeString): Promise<String[] | []> {
        return invoke("get_history", {historyType})
            .then((historyJsonText) => {
                if (typeof historyJsonText === "string") {
                    return JSON.parse(historyJsonText);
                }
                return [];
            })
    }

    addHistory(historyType: HistoryTypeString, historyPath: String) {
        return invoke("add_history", {historyType, historyPath})
    }

    openDialog(options: OpenDialogOptions) {
        return dialog.open(options)
    }

    readFile(filePath: String): Promise<String> {
        return invoke("acrn_read", {filePath})
    }

    writeFile(filePath: String, contents: String) {
        return invoke("acrn_write", {filePath, contents})
    }

    isFile(filePath: String): Promise<Boolean> {
        return invoke("acrn_is_file", {path: filePath})
    }

    readDir(path: String, recursive: Boolean) {
        return invoke('acrn_read_dir', {path, recursive})
    }

    creatDir(path: String, recursive = true) {
        return invoke('acrn_create_dir', {path, recursive})
    }

    removeDir(path: String) {
        return invoke('acrn_remove_dir', {path})
    }

    removeFile(path: String) {
        return invoke('acrn_remove_file', {path})
    }

    runPython(code: String, isJSON = false): String | Object {
        // @ts-ignore
        let result = window.pydoide.runPython(code);
        if (isJSON) {
            result = JSON.parse(result)
        }
        return result
    }

    loadBoard(path: String) {
        return this.readFile(path)
            .then((fileContent) => {
                return this.pythonObject.loadBoard(fileContent)
            })
    }

    loadScenario(path: String): Object {
        return this.readFile(path).then((fileContent) => {
            return this.pythonObject.loadScenario(fileContent)
        })
    }

    newVM(vmid, load_order) {
        return {
            '@id': vmid,
            load_order: load_order,
            name: `VM${vmid}`,
            cpu_affinity: {
                pcpu: [
                    {
                        pcpu_id: null,
                        real_time_vcpu: 'n'
                    }
                ]
            }
        }
    }

    createNewScenario(pre, service, post) {
        let newScenario = {
            hv: {},
            vm: []
        }
        let vmid = 0
        let vmNums = {'PRE_LAUNCHED_VM': pre, 'SERVICE_VM': service, 'POST_LAUNCHED_VM': post}
        for (let key in vmNums) {
            for (let i = 0; i < vmNums[key]; i++) {
                newScenario.vm.push(this.newVM(vmid, key))
                vmid++;
            }
        }
        return newScenario;
    }

    convertScenarioToXML(scenarioData: Object) {
        let json2xml = new JSON2XML();
        let xml_data = json2xml.convert(scenarioData);
        return xml_data
    }

}

let configurator = new Configurator()
export default configurator