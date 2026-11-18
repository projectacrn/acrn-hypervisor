import _ from 'lodash'
import {hexToRange, rangeToHex} from './masks'
import type {
    CatInput,
    CatRegionView,
    CatSummary,
    CatSwitchName,
    CatSwitches,
    CatView,
    CpuId,
    Policy,
    PolicyType,
    ScenarioDocument,
    SwitchCorrectionResult,
    VMConfig
} from './model'

const switchNames: CatSwitchName[] = ['RDT_ENABLED', 'CDP_ENABLED', 'VCAT_ENABLED']

export function correctSwitches(doc: ScenarioDocument): SwitchCorrectionResult {
    const stored = doc.hv?.FEATURES?.RDT || {}
    const values: Partial<CatSwitches> = {
        RDT_ENABLED: stored.RDT_ENABLED === undefined ? 'n' : stored.RDT_ENABLED,
        CDP_ENABLED: stored.CDP_ENABLED === undefined ? 'n' : stored.CDP_ENABLED,
        VCAT_ENABLED: stored.VCAT_ENABLED === undefined ? 'n' : stored.VCAT_ENABLED
    }
    const switches: CatSwitches = {
        RDT_ENABLED: values.RDT_ENABLED === 'y' ? 'y' : 'n',
        CDP_ENABLED: values.CDP_ENABLED === 'y' ? 'y' : 'n',
        VCAT_ENABLED: values.VCAT_ENABLED === 'y' ? 'y' : 'n'
    }

    if (switches.CDP_ENABLED === 'y' || switches.VCAT_ENABLED === 'y') {
        values.RDT_ENABLED = 'y'
        switches.RDT_ENABLED = 'y'
    }
    if (switches.CDP_ENABLED === 'y' && switches.VCAT_ENABLED === 'y') {
        values.VCAT_ENABLED = 'n'
        switches.VCAT_ENABLED = 'n'
    }

    const corrections: SwitchCorrectionResult['corrections'] = {}
    switchNames.forEach(name => {
        if (stored[name] !== values[name]) {
            corrections[name] = values[name]
        }
    })
    return {switches, corrections}
}

export function hasCPUAffinity(vmConfig: VMConfig): boolean {
    if (vmConfig.load_order === 'SERVICE_VM') {
        return false
    }
    return Object.prototype.hasOwnProperty.call(vmConfig, 'cpu_affinity') &&
        vmConfig.cpu_affinity !== null && vmConfig.cpu_affinity !== undefined &&
        Object.prototype.hasOwnProperty.call(vmConfig.cpu_affinity, 'pcpu') &&
        _.isArray(vmConfig.cpu_affinity.pcpu)
}

function checkCPUAffinity(doc: ScenarioDocument): string {
    const errMsg = ['CPU affinity is not set for the following VMs:']
    const result = doc.vm.map(vmConfig => {
        if (vmConfig.load_order === 'SERVICE_VM') {
            return true
        }
        const haveCPUAffinitySetting = hasCPUAffinity(vmConfig)
        if (!haveCPUAffinitySetting) {
            errMsg.push(`VM ${vmConfig.name} has no CPU affinity setting`)
        }
        return haveCPUAffinitySetting
    }).every(value => value)
    if (result) {
        return ''
    }
    errMsg.push('Please set CPU affinity for all VMs')
    return errMsg.join('\n')
}

function findPolicy(doc: ScenarioDocument, cacheId: string, cacheLevel: number,
                   vmName: string, vcpu: number, type: PolicyType): Policy | undefined {
    const cacheRegion = doc.hv?.CACHE_REGION
    if (!cacheRegion || !_.isArray(cacheRegion.CACHE_ALLOCATION)) {
        return undefined
    }
    const allocation = cacheRegion.CACHE_ALLOCATION.find(candidate =>
        candidate.CACHE_ID === cacheId && candidate.CACHE_LEVEL === cacheLevel)
    if (!allocation || !Array.isArray(allocation.POLICY)) {
        return undefined
    }
    return allocation.POLICY.find(candidate =>
        candidate.VM === vmName && candidate.VCPU === vcpu && candidate.TYPE === type)
}

function getClosMask(doc: ScenarioDocument, cacheId: string, cacheLevel: number,
                     vmName: string, vcpu: number, type: PolicyType, maxLength: number): string {
    const policy = findPolicy(doc, cacheId, cacheLevel, vmName, vcpu, type)
    if (policy) {
        return rangeToHex(hexToRange(policy.CLOS_MASK, maxLength), maxLength)
    }
    return '0x' + Number.parseInt('1'.repeat(maxLength), 2).toString(16)
}

function newPolicy(doc: ScenarioDocument, cacheId: string, cacheLevel: number,
                   vmConfig: VMConfig, vcpu: number, type: PolicyType, maxLength: number): Policy {
    return {
        VM: vmConfig.name,
        VCPU: vcpu,
        TYPE: type,
        CLOS_MASK: getClosMask(doc, cacheId, cacheLevel, vmConfig.name, vcpu, type, maxLength)
    }
}

function regionIncludes(region: CatRegionView, pcpuId: CpuId): boolean {
    return region.processors.indexOf(pcpuId) !== -1
}

function getRTCoreData(input: CatInput, region: CatRegionView, switches: CatSwitches): Policy[] {
    const policies: Policy[] = []
    input.doc.vm.forEach(vmConfig => {
        if (vmConfig.vm_type === 'RTVM' && hasCPUAffinity(vmConfig)) {
            vmConfig.cpu_affinity!.pcpu.forEach((pcpu, index) => {
                if (regionIncludes(region, pcpu.pcpu_id) && pcpu.real_time_vcpu === 'y') {
                    const args = [region.id, region.level, vmConfig, index] as const
                    if (switches.CDP_ENABLED === 'n') {
                        policies.push(newPolicy(input.doc, ...args, 'Unified', region.capacity_mask_length))
                    } else {
                        policies.push(newPolicy(input.doc, ...args, 'Code', region.capacity_mask_length))
                        policies.push(newPolicy(input.doc, ...args, 'Data', region.capacity_mask_length))
                    }
                }
            })
        }
    })
    return _.sortBy(policies, ['VM', 'VCPU', 'TYPE'])
}

function getServiceVM(input: CatInput): VMConfig | null {
    let serviceVM: VMConfig | null = null
    input.doc.vm.forEach(vmConfig => {
        if (vmConfig.load_order === 'SERVICE_VM') {
            serviceVM = vmConfig
        }
    })
    return serviceVM
}

function getPreLaunchedVMCPUs(input: CatInput): CpuId[] {
    const preLaunchedVMCPUs: CpuId[] = []
    input.doc.vm.forEach(vmConfig => {
        if (vmConfig.load_order === 'PRE_LAUNCHED_VM' && hasCPUAffinity(vmConfig)) {
            preLaunchedVMCPUs.push(...vmConfig.cpu_affinity!.pcpu.map(pcpu => pcpu.pcpu_id))
        }
    })
    return preLaunchedVMCPUs
}

function getServiceVMVCPUs(input: CatInput, serviceVM: VMConfig | null): CpuId[] {
    if (serviceVM === null) {
        return []
    }
    const preLaunchedVMCPUs = getPreLaunchedVMCPUs(input)
    const pcpuIds = input.schema.HV.BasicConfigType.definitions.CPUAffinityConfiguration.properties.pcpu_id.enum
    return pcpuIds.filter(pcpuId => preLaunchedVMCPUs.indexOf(pcpuId) === -1)
}

function getServiceData(input: CatInput, region: CatRegionView, switches: CatSwitches,
                        serviceVM: VMConfig | null, serviceVMCPUs: CpuId[]): Policy[] {
    if (serviceVM === null) {
        return []
    }
    const policies: Policy[] = []
    serviceVMCPUs.forEach((pcpuId, index) => {
        if (regionIncludes(region, pcpuId)) {
            const args = [region.id, region.level, serviceVM, index] as const
            if (switches.CDP_ENABLED === 'n') {
                policies.push(newPolicy(input.doc, ...args, 'Unified', region.capacity_mask_length))
            } else {
                policies.push(newPolicy(input.doc, ...args, 'Code', region.capacity_mask_length))
                policies.push(newPolicy(input.doc, ...args, 'Data', region.capacity_mask_length))
            }
        }
    })
    return policies
}

function getStandardData(input: CatInput, region: CatRegionView, switches: CatSwitches,
                         serviceVM: VMConfig | null, serviceVMCPUs: CpuId[]): Policy[] {
    let policies: Policy[] = []
    input.doc.vm.forEach(vmConfig => {
        if (hasCPUAffinity(vmConfig)) {
            vmConfig.cpu_affinity!.pcpu.forEach((pcpu, index) => {
                if (regionIncludes(region, pcpu.pcpu_id) &&
                    (!Object.prototype.hasOwnProperty.call(pcpu, 'real_time_vcpu') ||
                        pcpu.real_time_vcpu === 'n' ||
                        (pcpu.real_time_vcpu === 'y' && vmConfig.vm_type !== 'RTVM'))) {
                    const args = [region.id, region.level, vmConfig, index] as const
                    if (switches.CDP_ENABLED === 'n') {
                        policies.push(newPolicy(input.doc, ...args, 'Unified', region.capacity_mask_length))
                    } else {
                        policies.push(newPolicy(input.doc, ...args, 'Code', region.capacity_mask_length))
                        policies.push(newPolicy(input.doc, ...args, 'Data', region.capacity_mask_length))
                    }
                }
            })
        }
    })
    policies = policies.concat(getServiceData(input, region, switches, serviceVM, serviceVMCPUs))
    return _.sortBy(policies, ['VM', 'VCPU', 'TYPE'])
}

function getVCATData(input: CatInput, region: CatRegionView, switches: CatSwitches): Policy[] {
    const policies: Policy[] = []
    if (switches.VCAT_ENABLED === 'y' && region.processors.indexOf(0) !== -1) {
        input.doc.vm.forEach(vmConfig => {
            if (hasCPUAffinity(vmConfig) && vmConfig.virtual_cat_support === 'y') {
                policies.push(newPolicy(input.doc, region.id, region.level, vmConfig, 0, 'Unified',
                    vmConfig.virtual_cat_number!))
            }
        })
    }
    return _.sortBy(policies, ['VM'])
}

export function deriveCatView(input: CatInput): CatView | null {
    if (!input.doc.hv) {
        return null
    }
    if (input.board.CAT_INFO.length === 0) {
        return {
            errorMsg: 'This board(or CPU) doesn\'t support CAT which means there is no any CAT capability.</br>' +
                'There is no Cache Region was shared which means "all support CAT region only have one CPU core".',
            regions: [],
            summary: {}
        }
    }

    const {switches} = correctSwitches(input.doc)
    if (switches.RDT_ENABLED === 'n') {
        return {errorMsg: '', regions: [], summary: {}}
    }

    const errorMsg = checkCPUAffinity(input.doc)
    const serviceVM = getServiceVM(input)
    const serviceVMCPUs = getServiceVMVCPUs(input, serviceVM)
    const summary: CatSummary = {}
    const regions = input.board.CAT_INFO.map(region => {
        const regionData = _.cloneDeep(region) as CatRegionView
        if (!Object.prototype.hasOwnProperty.call(summary, regionData.level)) {
            summary[regionData.level] = {count: 0}
        }
        summary[regionData.level].count++
        summary[regionData.level][regionData.id] = summary[regionData.level].count
        regionData.data = {
            RTCore: getRTCoreData(input, regionData, switches),
            Standard: getStandardData(input, regionData, switches, serviceVM, serviceVMCPUs),
            VCAT: getVCATData(input, regionData, switches)
        }
        return regionData
    })

    return {errorMsg, regions, summary}
}
