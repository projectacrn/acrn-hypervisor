export type PolicyType = 'Unified' | 'Code' | 'Data'
export type SwitchValue = 'y' | 'n'
export type CatSwitchName = 'RDT_ENABLED' | 'CDP_ENABLED' | 'VCAT_ENABLED'
export type CpuId = number | string

export interface Policy {
    VM: string
    VCPU: number
    TYPE: PolicyType
    CLOS_MASK: string
}

export interface CacheAllocation {
    CACHE_ID: string
    CACHE_LEVEL: number
    POLICY: Policy[]
}

export interface CacheRegion {
    CACHE_ALLOCATION: CacheAllocation[]
}

export interface CatSwitches {
    RDT_ENABLED: SwitchValue
    CDP_ENABLED: SwitchValue
    VCAT_ENABLED: SwitchValue
}

export interface Hypervisor {
    FEATURES?: {
        RDT?: Partial<CatSwitches>
        [key: string]: unknown
    }
    CACHE_REGION?: CacheRegion | null
    [key: string]: unknown
}

export interface CpuAffinityEntry {
    pcpu_id: CpuId
    real_time_vcpu?: string
    [key: string]: unknown
}

export interface VMConfig {
    '@id': number | string
    name: string
    load_order: string
    vm_type?: string
    cpu_affinity?: {
        pcpu: CpuAffinityEntry[]
    } | null
    virtual_cat_support?: string
    virtual_cat_number?: number
    [key: string]: unknown
}

export interface ScenarioDocument {
    hv: Hypervisor | null
    vm: VMConfig[]
}

export interface CatRegion {
    id: string
    level: number
    capacity_mask_length: number
    type: string
    cache_size: number
    processors: CpuId[]
    [key: string]: unknown
}

export interface CatRegionView extends CatRegion {
    data: {
        RTCore: Policy[]
        Standard: Policy[]
        VCAT: Policy[]
    }
}

export interface CatSummary {
    [level: string]: {
        count: number
        [regionId: string]: number
    }
}

export interface CatView {
    errorMsg: string
    regions: CatRegionView[]
    summary: CatSummary
}

export interface CatSchema {
    HV: {
        BasicConfigType: {
            definitions: {
                CPUAffinityConfiguration: {
                    properties: {
                        pcpu_id: {
                            enum: CpuId[]
                        }
                    }
                }
            }
        }
    }
}

export interface CatInput {
    doc: ScenarioDocument
    board: {
        CAT_INFO: CatRegion[]
    }
    schema: CatSchema
}

export interface SwitchCorrectionResult {
    switches: CatSwitches
    corrections: Partial<CatSwitches>
}

export interface ClosMaskEdit {
    cacheId: string
    cacheLevel: number
    vm: string
    vcpu: number
    type: PolicyType
    closMask: string
}
