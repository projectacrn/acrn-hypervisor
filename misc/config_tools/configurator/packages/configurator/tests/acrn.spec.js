import {beforeEach, describe, expect, it} from 'vitest'
import configurator from '../src/lib/acrn'

function loadCAT({rdt = 'y', cdp = 'n', vcat = 'n'} = {}) {
    const scenario = {
        hv: {
            FEATURES: {
                RDT: {
                    RDT_ENABLED: rdt,
                    CDP_ENABLED: cdp,
                    VCAT_ENABLED: vcat
                }
            }
        },
        vm: [
            {
                '@id': 0,
                name: 'zeta',
                load_order: 'PRE_LAUNCHED_VM',
                vm_type: 'RTVM',
                cpu_affinity: {pcpu: [{pcpu_id: 0, real_time_vcpu: 'y'}]}
            },
            {
                '@id': 1,
                name: 'service',
                load_order: 'SERVICE_VM',
                vm_type: 'STANDARD_VM',
                cpu_affinity: null
            },
            {
                '@id': 2,
                name: 'alpha',
                load_order: 'POST_LAUNCHED_VM',
                vm_type: 'STANDARD_VM',
                cpu_affinity: {pcpu: [{pcpu_id: 2, real_time_vcpu: 'n'}]}
            },
            {
                '@id': 3,
                name: 'beta',
                load_order: 'PRE_LAUNCHED_VM',
                vm_type: 'RTVM',
                cpu_affinity: {pcpu: [{pcpu_id: 1, real_time_vcpu: 'y'}]}
            }
        ]
    }
    const schemas = {
        HV: {
            BasicConfigType: {
                definitions: {
                    CPUAffinityConfiguration: {
                        properties: {pcpu_id: {enum: [0, 1, 2]}}
                    }
                }
            }
        }
    }
    const board = {
        CAT_INFO: [{
            id: '0',
            level: 3,
            type: 'Unified',
            cache_size: 12 * 1024 * 1024,
            capacity_mask_length: 8,
            processors: [0, 1, 2]
        }]
    }

    window.getSchemaData = () => schemas
    window.getCurrentScenarioData = () => scenario
    window.getBoardData = () => board
    configurator.cat.scenarioLoaded()

    return {scenario, catData: configurator.cat.getCATUIData()}
}

describe('CAT configuration', () => {
    beforeEach(() => {
        configurator.cat.CATDB = []
    })

    it('excludes pre-launched VM processors from Service VM vCPUs', () => {
        const {catData} = loadCAT()
        const servicePolicies = catData.regions[0].data.Standard
            .filter(policy => policy.VM === 'service')

        expect(servicePolicies).toHaveLength(1)
        expect(servicePolicies[0].VCPU).toBe(0)
    })

    it('sorts policies by VM, vCPU, and type', () => {
        const {catData} = loadCAT()

        expect(catData.regions[0].data.RTCore.map(policy => policy.VM))
            .toEqual(['beta', 'zeta'])
        expect(catData.regions[0].data.Standard.map(policy => policy.VM))
            .toEqual(['alpha', 'service'])
    })

    it('enables RDT when an imported scenario enables CDP', () => {
        const {scenario, catData} = loadCAT({rdt: 'n', cdp: 'y'})

        expect(scenario.hv.FEATURES.RDT.RDT_ENABLED).toBe('y')
        expect(catData.regions).toHaveLength(1)
    })

    it('disables VCAT without a Proxy setter error when CDP is enabled', () => {
        const {scenario, catData} = loadCAT({rdt: 'n', cdp: 'y', vcat: 'y'})

        expect(scenario.hv.FEATURES.RDT.RDT_ENABLED).toBe('y')
        expect(scenario.hv.FEATURES.RDT.VCAT_ENABLED).toBe('n')
        expect(catData.regions).toHaveLength(1)
    })

    it('accepts hexadecimal masks without a 0x prefix', () => {
        expect(configurator.cat.hexToRange('ff', 8)).toEqual([0, 8])
        expect(configurator.cat.hexToRange('0f', 8)).toEqual([4, 8])
    })
})
