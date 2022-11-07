import {dialog, invoke} from "@tauri-apps/api";
import JSON2XML from "./json2xml"
import {OpenDialogOptions} from "@tauri-apps/api/dialog";
import _ from "lodash";
import {vueUtils} from "@lljj/vue3-form-naive";

function all(arr: boolean[]): boolean {
    return arr.every(element => element === true);
}

function count(source, target) {
    return (source.match(new RegExp(target, 'g')) || []).length;
}


declare global {
    interface Window {
        configurator: Configurator;
        getSchemaData: () => any;
        getCurrentScenarioData: () => any;
        getBoardData: () => any;
        pyodide: {
            pyimport: (name: string) => { main: (...any) => any },
            runPython: (code: string) => string
        };
    }
}

enum HistoryTypeEnum {
    WorkingFolder,
    Board,
    Scenario
}

export type HistoryTypes = keyof typeof HistoryTypeEnum;


enum PolicyTypeEnum {
    Unified,
    Code,
    Data
}

type PolicyType = keyof typeof PolicyTypeEnum

type Policy = {
    VM: string,
    VCPU: number,
    TYPE: PolicyType,
    CLOS_MASK: string
}

type CATDBRecord = {
    CACHE_LEVEL: number,
    CACHE_ID: string,
    META: { vmid: number },
    VM: string,
    VCPU: number,
    TYPE: PolicyType,
    CLOS_MASK: string
}

type CATUIDataObject = {
    errorMsg: string,
    regions: {
        level: number,
        id: string,
        capacity_mask_length: number,
        type: string,
        cache_size: number,
        processors: number[],
        data: {
            RTCore: Policy[],
            Standard: Policy[],
            VCAT: Policy[],
        },
    }[],
    summary: {
        [CATRegionLevel: string]: {
            count: number,
            [CATRegionID: string]: number
        }
    }
}


type vmID = number;

class CAT {
    private scenario: any;
    private schemaData: any;
    private CAT_REGION_INFO: any;

    private switches: {
        RDT_ENABLED: boolean,
        CDP_ENABLED: boolean,
        VCAT_ENABLED: boolean
    };

    private preLaunchedVMCPUs: string[];
    private serviceVM: any;
    private serviceVMCPUs: string[];

    public CATDB: CATDBRecord[];
    private vmIDs: { [vmName: string]: vmID };

    hexToRange(hexValue, maxValue) {
        let str_bin = Number.parseInt(hexValue).toString(2);
        let block_length = str_bin.length;
        let block_enabled_length = count(str_bin, "1");

        let start: number
        let end: number

        if (block_length > maxValue) {
            if (block_enabled_length >= maxValue) {
                str_bin = "1".repeat(maxValue);
            } else {
                str_bin = "0".repeat(maxValue - block_enabled_length) + "1".repeat(block_enabled_length);
            }
        } else {
            if (block_length < maxValue) {
                str_bin = "0".repeat(maxValue - block_length) + str_bin;
            }
        }

        start = str_bin.indexOf("1") !== -1 ? str_bin.indexOf("1") : 0;
        end = start + count(str_bin, "1");

        return [start, end]
    }

    rangeToHex(value, max) {
        let newHexValue = '0'.repeat(value[0]) + '1'.repeat(value[1] - value[0]) + '0'.repeat(max - value[1])
        newHexValue = (parseInt(newHexValue, 2).toString(16))
        let zeroPadding = '0'.repeat(Number.parseInt('1'.repeat(max), 2).toString(16).length - newHexValue.length)
        newHexValue = '0x' + zeroPadding + newHexValue;
        return newHexValue;
    }

    formDataProxy(name, data = null, update = false) {
        let path = {
            'RDT_ENABLED': 'FEATURES.RDT.RDT_ENABLED',
            'CDP_ENABLED': 'FEATURES.RDT.CDP_ENABLED',
            'VCAT_ENABLED': 'FEATURES.RDT.VCAT_ENABLED',
        }[name]

        // check parent node exists
        let oldValue = vueUtils.getPathVal(this.scenario.hv, path);
        if (oldValue === undefined) {
            let t = path.split('.');
            let parentPath = t.splice(0, t.length - 1).join('.');
            if (!vueUtils.getPathVal(this.scenario.hv, parentPath)) {
                vueUtils.setPathVal(this.scenario.hv, parentPath, {});
            }
            // set to checkbox default value
            vueUtils.setPathVal(this.scenario.hv, path, 'n');
        }
        // if data is not empty, set value
        if (data !== null) {
            vueUtils.setPathVal(this.scenario.hv, path, data)

            // if data is not empty, set value as expected and update CAT_INFO
            if (update) {
                switch (name) {
                    case 'RDT_ENABLED':
                        if (data === 'n') {
                            this.formDataProxy('CDP_ENABLED', 'n');
                            this.formDataProxy('VCAT_ENABLED', 'n');
                        }
                        break;
                    case 'CDP_ENABLED':
                        if (data === 'y') {
                            this.formDataProxy('RDT_ENABLED', 'y');
                            this.formDataProxy('VCAT_ENABLED', 'n');
                        }
                        break;
                    case 'VCAT_ENABLED':
                        if (data === 'y') {
                            this.formDataProxy('RDT_ENABLED', 'y');
                            this.formDataProxy('CDP_ENABLED', 'n');
                        }
                        break;
                }
            }
        }
        let result: string;
        // @ts-ignore
        result = vueUtils.getPathVal(this.scenario.hv, path);
        if (typeof result !== 'string') {
            console.log(`Unexpected result of ${name}: `, result)
        }
        return result
    }


    scenarioLoaded() {
        // get CAT schema && scenario data
        this.schemaData = window.getSchemaData();
        this.scenario = window.getCurrentScenarioData();
        this.vmIDs = this.getVMIDs()
        // get cat scenario data
        this.CATDB = this.getCATDataFromScenario();
    }

    getScenarioDataFromCAT() {
        let CATUIData = this.getCATUIData();
        let ScenarioCATData: {
            CACHE_ALLOCATION: {
                CACHE_ID: string,
                CACHE_LEVEL: number,
                POLICY: Policy[]
            }[]
        }
        if (CATUIData.regions.length === 0) {
            return null;
        }
        ScenarioCATData = {CACHE_ALLOCATION: []}
        for (const region of CATUIData.regions) {
            let policies: Policy[] = region.data.RTCore.concat(region.data.Standard, region.data.VCAT);
            ScenarioCATData.CACHE_ALLOCATION.push({
                CACHE_ID: region.id,
                CACHE_LEVEL: region.level,
                POLICY: policies
            })
        }
        return ScenarioCATData;
    }


    getCATUIData(): CATUIDataObject {
        // get CAT schema && scenario && board basic data
        this.schemaData = window.getSchemaData();
        this.scenario = window.getCurrentScenarioData();
        this.CAT_REGION_INFO = window.getBoardData().CAT_INFO;

        // check scenario data is empty
        // usually, this happens when user has no scenario loaded, then import a board
        if (!this.scenario.hv) {
            return null;
        }


        // get switches status from scenario
        // @ts-ignore
        this.switches = new Proxy({}, {
            get: (target: {}, switchName: string | symbol): any => {
                return this.formDataProxy(switchName) === 'y'
            },
            set: (target: {}, switchName: string | symbol, value: boolean): boolean => {
                return this.formDataProxy(switchName, value ? 'y' : 'n', true) === 'y';
            }
        })


        // if no CAT REGION INFO from board xml,
        // means this board(or CPU) not support CAT, or all support CAT region only have one CPU core
        if (this.CAT_REGION_INFO.length === 0) {
            let errorMsg = `This board(or CPU) doesn't support CAT which means there is no any CAT capability.</br>There is no Cache Region was shared which means \"all support CAT region only have one CPU core\".`;
            console.log(errorMsg);
            return {
                errorMsg,
                regions: [],
                summary: {}
            };
        }

        // correct switches and return rdt_enabled result
        if (!this.correctSwitches()) {
            return {
                errorMsg: '',
                regions: [],
                summary: {}
            }
        }


        // CPU affinity data checks
        // If error, only show error message
        let errorMsg = this.checkCPUAffinity()


        // get CPU data
        this.preLaunchedVMCPUs = this.getPreLaunchedVMCPUs();
        this.serviceVM = this.getServiceVM();
        this.serviceVMCPUs = this.getServiceVMVCPUs()
        this.vmIDs = this.getVMIDs()


        let CATUIData: CATUIDataObject = {
            errorMsg, regions: [], summary: {}
        };
        // mapping CAT region info
        this.CAT_REGION_INFO.map(region => {
            let regionData = _.cloneDeep(region);
            if (!CATUIData.summary.hasOwnProperty(regionData.level)) {
                CATUIData.summary[regionData.level] = {count: 0}
            }
            CATUIData.summary[regionData.level].count++;
            CATUIData.summary[regionData.level][regionData.id] = CATUIData.summary[regionData.level].count;

            regionData['data'] = {
                RTCore: this.getRTCoreData(regionData),
                Standard: this.getStandardData(regionData),
                VCAT: this.getVCATData(regionData)
            }
            CATUIData.regions.push(regionData);
        })


        return CATUIData
    }

    haveCPUAffinity(vmConfig) {
        if (vmConfig.load_order === 'SERVICE_VM') {
            return false
        }
        return (
            vmConfig.hasOwnProperty('cpu_affinity') &&
            vmConfig.cpu_affinity.hasOwnProperty('pcpu') &&
            _.isArray(vmConfig.cpu_affinity.pcpu)
        )
    }

    checkCPUAffinity() {
        // check cpu affinity
        let errMsg = ['CPU affinity is not set for the following VMs:'];
        let result = all(this.scenario.vm.map(vmConfig => {
            if (vmConfig.load_order === 'SERVICE_VM') {
                return true
            }
            let haveCPUAffinitySetting = this.haveCPUAffinity(vmConfig);
            if (!haveCPUAffinitySetting) {
                errMsg.push(`VM ${vmConfig.name} has no CPU affinity setting`);
            }
            return haveCPUAffinitySetting;
        }))
        if (result) {
            return '';
        }
        errMsg.push('Please set CPU affinity for all VMs');
        return errMsg.join('\n')
    }

    correctSwitches() {
        if (this.switches.RDT_ENABLED) {
            if (this.switches.CDP_ENABLED) {
                if (this.switches.VCAT_ENABLED) {
                    this.switches.VCAT_ENABLED = false
                }
            }
        } else if (this.switches.VCAT_ENABLED) {
            if (!this.switches.RDT_ENABLED) {
                this.switches.RDT_ENABLED = true
            }
        }
        return this.switches.RDT_ENABLED
    }

    getPreLaunchedVMCPUs() {
        let preLaunchedVMCPUs = [];

        this.scenario.vm.map(vmConfig => {
            if (vmConfig.load_order === 'PRE_LAUNCHED_VM' && this.haveCPUAffinity(vmConfig)) {
                let vmCPUIDs = vmConfig.cpu_affinity.pcpu.map(pcpu => {
                    return pcpu.pcpu_id;
                })
                preLaunchedVMCPUs.concat(vmCPUIDs)
            }
        })

        return preLaunchedVMCPUs;
    }

    newPolicy(CACHE_ID, CACHE_LEVEL, vmConfig, VCPU, TYPE: PolicyType, maxLength): Policy {
        let originPolicy = {
            VM: vmConfig.name,
            VCPU, TYPE,
            CLOS_MASK: this.getCLOSMask(CACHE_ID, CACHE_LEVEL, vmConfig['@id'], vmConfig.name, VCPU, TYPE, maxLength)
        }
        return new Proxy(originPolicy, {
            set: (target, key, value) => {
                target[key] = value;
                if (key === 'CLOS_MASK') {
                    console.log(`${CACHE_ID} ${CACHE_LEVEL} ${vmConfig.name} ${VCPU} ${TYPE} CLOS_MASK: ${value}`);
                    this.setCLOSMask(CACHE_ID, CACHE_LEVEL, vmConfig['@id'], vmConfig.name, VCPU, TYPE, value);
                }
                return true;
            }
        })
    }

    selectCATData(CACHE_ID, CACHE_LEVEL, vmID, vmName, VCPU, TYPE: PolicyType) {
        for (let i = 0; i < this.CATDB.length; i++) {
            let CATData = this.CATDB[i];
            if (
                CATData.CACHE_ID === CACHE_ID && CATData.CACHE_LEVEL === CACHE_LEVEL &&
                CATData.META.vmid === vmID && CATData.VCPU === VCPU && CATData.TYPE === TYPE
            ) {
                return CATData
            }
        }
        return false;
    }

    setCLOSMask(CACHE_ID, CACHE_LEVEL, vmID, vmName, VCPU, TYPE: PolicyType, CLOS_MASK: string) {
        let CATData = this.selectCATData(CACHE_ID, CACHE_LEVEL, vmID, vmName, VCPU, TYPE);
        if (CATData !== false) {
            CATData.CLOS_MASK = CLOS_MASK;
            return true;
        }

        this.CATDB.push({
            META: {vmid: vmID},
            CACHE_ID, CACHE_LEVEL,
            CLOS_MASK,
            VM: vmName, VCPU, TYPE,
        })
        return true;
    }

    getCLOSMask(CACHE_ID, CACHE_LEVEL, vmID, vmName, VCPU, TYPE: PolicyType, maxLength: number) {
        let CATData = this.selectCATData(CACHE_ID, CACHE_LEVEL, vmID, vmName, VCPU, TYPE);
        let CLOS_MASK
        if (CATData !== false) {
            CLOS_MASK = CATData.CLOS_MASK;
            // ensure CLOS_MASK length is shorter or equal to maxLength
            CLOS_MASK = this.rangeToHex(this.hexToRange(CLOS_MASK, maxLength), maxLength);
            return CLOS_MASK;
        } else CLOS_MASK = "0x" + parseInt('1'.repeat(maxLength), 2).toString(16)
        this.CATDB.push({
            META: {vmid: vmID},
            CACHE_ID, CACHE_LEVEL,
            CLOS_MASK,
            VM: vmName, VCPU, TYPE,
        })
        return CLOS_MASK;
    }

    getRTCoreData(regionData): Policy[] {
        let RTCoreData: Policy[] = [];
        this.scenario.vm.map(vmConfig => {
            if (vmConfig.vm_type === 'RTVM' && this.haveCPUAffinity(vmConfig)) {
                vmConfig.cpu_affinity.pcpu.map(
                    (pcpu, index) => {
                        if (
                            regionData.processors.indexOf(pcpu.pcpu_id) !== -1 &&
                            pcpu.hasOwnProperty('real_time_vcpu') &&
                            pcpu.real_time_vcpu === 'y'
                        ) {
                            if (!this.switches.CDP_ENABLED) {
                                RTCoreData.push(this.newPolicy(regionData.id, regionData.level, vmConfig, index, 'Unified', regionData.capacity_mask_length))
                            } else {
                                RTCoreData.push(this.newPolicy(regionData.id, regionData.level, vmConfig, index, 'Code', regionData.capacity_mask_length))
                                RTCoreData.push(this.newPolicy(regionData.id, regionData.level, vmConfig, index, 'Data', regionData.capacity_mask_length))
                            }
                        }
                    }
                )
            }
        })

        _.sortBy(RTCoreData, ['VM', 'VCPU', 'TYPE']);

        return RTCoreData;
    }


    getStandardData(regionData): Policy[] {
        let StandardData: Policy[] = [];
        this.scenario.vm.map(vmConfig => {
            if (this.haveCPUAffinity(vmConfig)) {
                vmConfig.cpu_affinity.pcpu.map(
                    (pcpu, index) => {
                        if (regionData.processors.indexOf(pcpu.pcpu_id) !== -1) {
                            if (!pcpu.hasOwnProperty('real_time_vcpu') ||
                                (pcpu.real_time_vcpu === 'n') ||
                                (pcpu.real_time_vcpu === 'y' && vmConfig.vm_type !== 'RTVM')
                            ) {
                                if (!this.switches.CDP_ENABLED) {
                                    StandardData.push(this.newPolicy( regionData.id, regionData.level,vmConfig, index, 'Unified', regionData.capacity_mask_length))
                                } else {
                                    StandardData.push(this.newPolicy(regionData.id, regionData.level, vmConfig, index, "Code", regionData.capacity_mask_length))
                                    StandardData.push(this.newPolicy( regionData.id, regionData.level,vmConfig, index, "Data", regionData.capacity_mask_length))
                                }
                            }
                        }
                    })
            }
        })

        // add service vm policy
        StandardData = StandardData.concat(
            this.getServiceData(regionData),
        )

        _.sortBy(StandardData, ['VM', 'VCPU', 'TYPE']);
        return StandardData;
    }

    getServiceData(regionData): Policy[] {
        let ServiceData: Policy[] = [];

        this.serviceVMCPUs.map((pcpuID, index) => {
            if (regionData.processors.indexOf(pcpuID) !== -1) {
                if (!this.switches.CDP_ENABLED) {
                    ServiceData.push(this.newPolicy(regionData.id, regionData.level, this.serviceVM, index, "Unified", regionData.capacity_mask_length))
                } else {
                    ServiceData.push(this.newPolicy(regionData.id, regionData.level, this.serviceVM, index, "Code", regionData.capacity_mask_length))
                    ServiceData.push(this.newPolicy(regionData.id, regionData.level, this.serviceVM, index, "Data", regionData.capacity_mask_length))
                }
            }
        })
        return ServiceData;
    }

    getVCATData(regionData): Policy[] {
        let VCATData: Policy[] = [];
        // VCAT is only available for CPU 0
        if (this.switches.VCAT_ENABLED && regionData.processors.indexOf(0) !== -1) {
            this.scenario.vm.map(vmConfig => {
                if (
                    this.haveCPUAffinity(vmConfig) &&
                    vmConfig.hasOwnProperty('virtual_cat_support') &&
                    vmConfig.virtual_cat_support === "y"
                ) {
                    VCATData.push(
                        this.newPolicy(regionData.id, regionData.level, vmConfig, 0, "Unified", vmConfig.virtual_cat_number)
                    )
                }
            })
        }
        _.sortBy(VCATData, ['VM']);
        return VCATData;
    }

    private getServiceVM() {
        let serviceVM = null;
        this.scenario.vm.map(vmConfig => {
            if (vmConfig.load_order === 'SERVICE_VM') {
                serviceVM = vmConfig;
            }
        })
        return serviceVM;
    }

    private getServiceVMVCPUs() {
        let serviceVMCPUs = [];
        if (this.serviceVM !== null) {
            // noinspection JSUnresolvedVariable
            this.schemaData.HV.BasicConfigType.definitions.CPUAffinityConfiguration.properties.pcpu_id.enum.map((pcpu_id) => {
                // if pcpu_id in preLaunchedVMCPUIDs, it's used by pre launched vm, we need skip it
                if (this.preLaunchedVMCPUs.indexOf(pcpu_id) !== -1) {
                    return;
                }
                serviceVMCPUs.push(pcpu_id);
            })
        }
        return serviceVMCPUs;
    }


    private getCATDataFromScenario() {
        let hv = this.scenario.hv;

        let scenarioCATData: CATDBRecord[] = []
         // noinspection JSUnresolvedVariable
        if (
            hv !== null &&
            hv.hasOwnProperty('CACHE_REGION')
        ) {
            let cacheRegion = hv.CACHE_REGION
            if (cacheRegion !== null && cacheRegion.hasOwnProperty('CACHE_ALLOCATION') &&
                _.isArray(cacheRegion.CACHE_ALLOCATION)) {
                // noinspection JSUnresolvedVariable

                cacheRegion.CACHE_ALLOCATION.map((cache_allocation) => {
                    if (
                        cache_allocation.hasOwnProperty('POLICY') &&
                        cache_allocation.POLICY.length > 0
                    ) {
                        cache_allocation.POLICY.map(policy => {
                            scenarioCATData.push({
                                CACHE_ID: cache_allocation.CACHE_ID,
                                CACHE_LEVEL: cache_allocation.CACHE_LEVEL,
                                CLOS_MASK: policy.CLOS_MASK,
                                META: {vmid: this.vmIDs[policy.VM]},
                                TYPE: policy.TYPE,
                                VCPU: policy.VCPU,
                                VM: policy.VM
                            })
                        })
                    }

                })

            }
        }
        return scenarioCATData
    }

    private getVMIDs(): { [vmName: string]: vmID } {
        let vmIDs = {}
        this.scenario.vm.map(vmConfig => {
            vmIDs[vmConfig.name] = vmConfig['@id']
        })
        return vmIDs
    }
}

class PythonObject {
    api(scriptName, output_format, ...params) {
        let pythonFunction = window.pyodide.pyimport(`configurator.pyodide.${scriptName}`);
        let result = pythonFunction.main(...params);
        if (output_format === 'json') {
            return JSON.parse(result);
        } else {
            return result;
        }
    }

    loadBoard(boardXMLText, path) {
        return this.api('loadBoard', 'json', boardXMLText, path)

    }

    loadScenario(scenarioXMLText) {
        return this.api('loadScenario', 'json', scenarioXMLText)
    }

    validateBoardStructure(boardXMLText) {
        return this.api('validateBoardStructure', 'plaintext', boardXMLText)
    }

    validateScenarioStructure(scenarioXMLText) {
        return this.api('validateScenarioStructure', 'plaintext', scenarioXMLText)
    }

    validateScenario(boardXMLText, scenarioXMLText) {
        return this.api('validateScenario', 'json', boardXMLText, scenarioXMLText)
    }

    generateLaunchScript(boardXMLText, scenarioXMLText) {
        return this.api('generateLaunchScript', 'json', boardXMLText, scenarioXMLText)
    }

    populateDefaultValues(scenarioXMLText) {
        return this.api('populateDefaultValues', 'json', scenarioXMLText)
    }

    generateConfigSummary(boardXMLText, scenarioXMLText) {
        return this.api('generateConfigSummary', 'plaintext', boardXMLText, scenarioXMLText)
    }
}

class Configurator {
    public pythonObject: PythonObject;
    public cat: CAT;

    constructor() {
        this.pythonObject = new PythonObject()
        this.cat = new CAT()
    }

    getHistory(historyType: HistoryTypes): Promise<String[] | []> {
        return invoke("get_history", {historyType})
            .then((historyJsonText) => {
                if (typeof historyJsonText === "string") {
                    return JSON.parse(historyJsonText);
                }
                return [];
            })
    }

    addHistory(historyType: HistoryTypes, historyPath: string) {
        return invoke("add_history", {historyType, historyPath})
    }

    openDialog(options: OpenDialogOptions) {
        return dialog.open(options)
    }

    readFile(filePath: string): Promise<String> {
        return invoke("acrn_read", {filePath})
    }

    writeFile(filePath: string, contents: string) {
        return invoke("acrn_write", {filePath, contents})
    }

    isFile(filePath: string): Promise<Boolean> {
        return invoke("acrn_is_file", {path: filePath})
    }

    readDir(path: string, recursive: Boolean) {
        return invoke('acrn_read_dir', {path, recursive})
    }

    creatDir(path: string, recursive = true) {
        return invoke('acrn_create_dir', {path, recursive})
    }

    removeDir(path: string) {
        return invoke('acrn_remove_dir', {path})
    }

    removeFile(path: string) {
        return invoke('acrn_remove_file', {path})
    }

    runPython(code: string, isJSON = false): string | Object {
        let result = window.pyodide.runPython(code);
        if (isJSON) {
            result = JSON.parse(result)
        }
        return result
    }

    loadBoard(path: string) {
        return this.readFile(path)
            .then((fileContent) => {
                let syntactical_errors = this.pythonObject.validateBoardStructure(fileContent);
                if (syntactical_errors !== "") {
                    throw Error("The file has broken structure.");
                }
                return this.pythonObject.loadBoard(fileContent, path);
            })
    }

    loadScenario(path: string): Object {
        return this.readFile(path).then((fileContent) => {
            let syntactical_errors = this.pythonObject.validateScenarioStructure(fileContent);
            if (syntactical_errors !== "") {
                throw Error("The loaded file does not look like a valid ACRN scenario XML.\n\n" +
                    "If that file is used with ACRN 2.x, try upgrading it following the instructions at https://projectacrn.github.io/latest/tutorials/upgrading_configuration.html.\n");
            }
            return this.pythonObject.loadScenario(fileContent)
        })
    }

    newVM(vmid, load_order) {
        let newVMData = {
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
        return json2xml.convert(scenarioData)
    }

}

let configurator = new Configurator()

window.configurator = configurator
export default configurator
