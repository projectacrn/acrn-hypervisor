import JSON2XML from "./json2xml"
import type {OpenDialogOptions} from "@tauri-apps/plugin-dialog";
import {PythonObject} from "../engine/pyodide/client";
import {getPlatform} from "../platform";

export enum HistoryTypeEnum {
    WorkingFolder,
    Board,
    Scenario
}

export type HistoryTypes = keyof typeof HistoryTypeEnum;


class Configurator {
    public pythonObject: PythonObject;

    constructor() {
        this.pythonObject = new PythonObject()
    }

    getHistory(historyType: HistoryTypes): Promise<string[]> {
        return getPlatform().history.get(historyType)
    }

    addHistory(historyType: HistoryTypes, historyPath: string) {
        return getPlatform().history.add(historyType, historyPath)
    }

    openDialog(options: OpenDialogOptions) {
        return getPlatform().dialog.open(options)
    }

    readFile(filePath: string): Promise<string> {
        return getPlatform().fs.readText(filePath)
    }

    writeFile(filePath: string, contents: string) {
        return getPlatform().fs.writeTextAtomic(filePath, contents)
    }

    readDir(path: string, recursive: boolean) {
        return getPlatform().fs.list(path, recursive)
    }

    creatDir(path: string, recursive = true) {
        return getPlatform().fs.createDir(path, recursive)
    }

    removeFile(path: string) {
        return getPlatform().fs.removeFile(path)
    }

    loadBoard(path: string) {
        return this.readFile(path)
            .then((fileContent) => {
                const syntacticalErrors = this.pythonObject.validateBoardStructure(fileContent);
                if (syntacticalErrors !== "") {
                    throw Error("The file has broken structure.");
                }
                return this.pythonObject.loadBoard(fileContent, path);
            })
    }

    loadScenario(path: string): Promise<any> {
        return this.readFile(path).then((fileContent) => {
            const syntacticalErrors = this.pythonObject.validateScenarioStructure(fileContent);
            if (syntacticalErrors !== "") {
                throw Error("The loaded file does not look like a valid ACRN scenario XML.\n\n" +
                    "If that file is used with ACRN 2.x, try upgrading it following the instructions at https://projectacrn.github.io/latest/tutorials/upgrading_configuration.html.\n");
            }
            return this.pythonObject.loadScenario(fileContent)
        })
    }

    newVM(vmid: number, load_order: string) {
        const newVMData: Record<string, any> = {
            '@id': vmid,
            load_order: load_order,
            name: `VM${vmid}`,
            cpu_affinity: null
        }
        if (load_order !== 'SERVICE_VM') {
            newVMData['cpu_affinity'] = {
                pcpu: [
                    {
                        pcpu_id: null,
                        real_time_vcpu: 'n'
                    }
                ]
            }
        }
        return newVMData
    }

    createNewScenario(pre: number, service: number, post: number) {
        const newScenario: Record<string, any> = {
            hv: {},
            vm: []
        }
        let vmid = 0
        const vmNums: Record<string, number> = {
            PRE_LAUNCHED_VM: pre,
            SERVICE_VM: service,
            POST_LAUNCHED_VM: post
        }
        for (const key in vmNums) {
            for (let i = 0; i < vmNums[key]; i++) {
                newScenario.vm.push(this.newVM(vmid, key))
                vmid++;
            }
        }
        return newScenario;
    }

    convertScenarioToXML(scenarioData: object) {
        const json2xml = new JSON2XML();
        return json2xml.convert(scenarioData)
    }

}

const configurator = new Configurator()

export default configurator
