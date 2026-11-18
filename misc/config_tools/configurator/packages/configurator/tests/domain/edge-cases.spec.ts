import {describe, expect, it, vi} from 'vitest'
import {correctSwitches, deriveCatView} from '../../src/domain/cat/derive'
import {setClosMask} from '../../src/domain/cat/toScenario'
import {deriveActivePciSchema, deriveLoadedPciSchema, selectFormSchema} from '../../src/domain/schema/derive'
import {removeVm, renameVm} from '../../src/domain/references/rename'
import {pruneStaleLaunchScripts, runSavePipeline} from '../../src/domain/artifacts/pipeline'

describe('CAT derivation defaults', () => {
    it('treats missing RDT settings as disabled switches', () => {
        const result = correctSwitches({hv: {}})

        expect(result.switches).toEqual({RDT_ENABLED: 'n', CDP_ENABLED: 'n', VCAT_ENABLED: 'n'})
    })

    it('returns no view when the scenario has no hypervisor section', () => {
        expect(deriveCatView({doc: {hv: null, vm: []}, board: {CAT_INFO: []}, schema: {}})).toBeNull()
    })

    it('derives regions without a Service VM present', () => {
        const input = {
            doc: {
                hv: {FEATURES: {RDT: {RDT_ENABLED: 'y', CDP_ENABLED: 'n', VCAT_ENABLED: 'n'}}},
                vm: [],
            },
            board: {
                CAT_INFO: [{
                    id: '0x0', level: 3, type: 'L3', cache_size: 1024,
                    capacity_mask_length: 4, processors: [0, 1],
                }],
            },
            schema: {
                HV: {
                    BasicConfigType: {
                        definitions: {
                            CPUAffinityConfiguration: {properties: {pcpu_id: {enum: [0, 1]}}},
                        },
                    },
                },
            },
        }

        const view = deriveCatView(input as any)

        expect(view).not.toBeNull()
        expect(view!.regions).toHaveLength(1)
        expect(view!.regions[0].data.Standard).toEqual([])
        expect(view!.regions[0].data.RTCore).toEqual([])
        expect(view!.summary[3].count).toBe(1)
    })

    it('creates a cache region from scratch when none exists yet', () => {
        const region = setClosMask(null, {
            cacheId: '0x0', cacheLevel: 3, vm: 'VM0', vcpu: 0, type: 'Unified', closMask: '0x3',
        })

        expect(region.CACHE_ALLOCATION).toHaveLength(1)
        expect(region.CACHE_ALLOCATION[0].POLICY).toEqual([
            {VM: 'VM0', VCPU: 0, TYPE: 'Unified', CLOS_MASK: '0x3'},
        ])
    })
})

describe('schema derivation fallbacks', () => {
    it('leaves schemas untouched when there is no pre-launched schema', () => {
        const schemas = {HV: {BasicConfigType: {}}}

        expect(deriveLoadedPciSchema(schemas, ['00:01.0'])).toBe(schemas)
        expect(deriveActivePciSchema(schemas, ['00:01.0'])).toBe(schemas)
    })

    it('leaves a pre-launched schema without PCI configuration untouched', () => {
        const schemas = {PreLaunchedVM: {BasicConfigType: {definitions: {}}}}

        const derived = deriveLoadedPciSchema(schemas, ['00:01.0'])

        expect(derived.PreLaunchedVM).toBe(schemas.PreLaunchedVM)
        expect(derived.PreLaunchedVM.BasicConfigType.definitions).toEqual({})
    })

    it('returns no form schema for an unknown VM id', () => {
        const schemas = {HV: {BasicConfigType: {}}, PreLaunchedVM: {BasicConfigType: {}}}
        const vms = [{'@id': 0, load_order: 'PRE_LAUNCHED_VM'}]

        expect(selectFormSchema(schemas, -1, vms)).toBe(schemas.HV)
        expect(selectFormSchema(schemas, 0, vms)).toBe(schemas.PreLaunchedVM)
        expect(selectFormSchema(schemas, 42, vms)).toBeUndefined()
    })
})

describe('VM reference rewriting tolerates malformed containers', () => {
    it('ignores null region entries and non-array collections', () => {
        const withNullEntry = {
            hv: {FEATURES: {IVSHMEM: {IVSHMEM_REGION: [null]}}},
            vm: [],
        }
        expect(() => renameVm(withNullEntry, 'VM0', 'VM1')).not.toThrow()
        expect(withNullEntry.hv.FEATURES.IVSHMEM.IVSHMEM_REGION).toEqual([null])

        const withObjectCollection = {
            hv: {FEATURES: {IVSHMEM: {IVSHMEM_REGION: {IVSHMEM_VMS: {}}}}},
            vm: [],
        }
        expect(() => renameVm(withObjectCollection, 'VM0', 'VM1')).not.toThrow()
        expect(withObjectCollection.hv.FEATURES.IVSHMEM.IVSHMEM_REGION).toEqual({IVSHMEM_VMS: {}})
    })

    it('skips pruning when the IVSHMEM container is missing or not an array', () => {
        const missingContainer = {hv: {}, vm: []}
        expect(() => removeVm(missingContainer, 'VM0')).not.toThrow()
        expect(missingContainer.hv).toEqual({})

        const notAnArray = {hv: {FEATURES: {IVSHMEM: {IVSHMEM_REGION: {}}}} as any, vm: []}
        expect(() => removeVm(notAnArray, 'VM0')).not.toThrow()
        expect(notAnArray.hv.FEATURES.IVSHMEM.IVSHMEM_REGION).toEqual({})
    })

    it('still prunes regions left with fewer than two VMs', () => {
        const doc = {
            hv: {
                FEATURES: {
                    IVSHMEM: {
                        IVSHMEM_REGION: [
                            {IVSHMEM_VMS: {IVSHMEM_VM: [{VM_NAME: 'VM0'}, {VM_NAME: 'VM1'}]}},
                            {IVSHMEM_VMS: {IVSHMEM_VM: [{VM_NAME: 'VM2'}, {VM_NAME: 'VM3'}]}},
                        ],
                    },
                },
            },
            vm: [],
        }

        removeVm(doc, 'VM0')

        expect(doc.hv.FEATURES.IVSHMEM.IVSHMEM_REGION).toHaveLength(1)
        expect(doc.hv.FEATURES.IVSHMEM.IVSHMEM_REGION[0].IVSHMEM_VMS.IVSHMEM_VM).toEqual([
            {VM_NAME: 'VM2'}, {VM_NAME: 'VM3'},
        ])
    })
})

describe('launch script cleanup', () => {
    it('ignores entries without a basename and keeps current scripts', async () => {
        const removed: string[] = []

        await pruneStaleLaunchScripts('/work', new Set(['launch_current.sh']), {
            listFiles: async () => [
                {path: '/work/'},
                {path: '/work/launch_old.sh'},
                {path: '/work/launch_current.sh'},
                {path: '/work/scenario.xml'},
            ],
            removeFile: async (path: string) => {
                removed.push(path)
            },
        })

        expect(removed).toEqual(['/work/launch_old.sh'])
    })
})

describe('save pipeline path and error handling', () => {
    function scenario() {
        return {
            hv: {FEATURES: {RDT: {RDT_ENABLED: 'y'}}},
            vm: [{'@id': 0, load_order: 'SERVICE_VM', name: 'VM0'}],
        }
    }

    function dependencies(overrides = {}) {
        return {
            scenarioToXml: () => '<xml/>',
            populateDefaults: () => ({json: {'acrn-config': scenario()}}),
            writeFile: vi.fn().mockResolvedValue(undefined),
            validateScenario: () => ({syntactic_errors: [], semantic_errors: []}),
            generateConfigSummary: () => 'summary',
            generateLaunchScript: () => ({}),
            listFiles: async () => [],
            removeFile: async () => {},
            ...overrides,
        }
    }

    function input(workingFolder: string) {
        return {scenario: scenario(), activeVmId: -1, boardXml: '<board/>', workingFolder}
    }

    it('joins artifacts with the platform separator when the folder has none', async () => {
        const deps = dependencies()
        const report = await runSavePipeline(input('/work'), deps as any)

        expect(report.success).toBe(true)
        expect(deps.writeFile.mock.calls.map(call => call[0])).toEqual([
            '/work/scenario.xml', '/work/config_summary.rst',
        ])
    })

    it('joins artifacts with backslashes for Windows working folders', async () => {
        const deps = dependencies()
        const report = await runSavePipeline(input('C:\\work'), deps as any)

        expect(report.success).toBe(true)
        expect(deps.writeFile.mock.calls.map(call => call[0])).toEqual([
            'C:\\work\\scenario.xml', 'C:\\work\\config_summary.rst',
        ])
    })

    it('does not duplicate the separator when the folder already ends with one', async () => {
        const deps = dependencies()
        await runSavePipeline(input('/work/'), deps as any)

        expect(deps.writeFile.mock.calls[0][0]).toBe('/work/scenario.xml')
    })

    it('reports non-Error failures at the step that stopped and skips the rest', async () => {
        const deps = dependencies({writeFile: vi.fn().mockRejectedValue('disk full')})
        const report = await runSavePipeline(input('/work'), deps as any)

        expect(report.success).toBe(false)
        expect(report.error).toBe('disk full')
        expect(report.steps.find(step => step.id === 'writeScenario')).toEqual({
            id: 'writeScenario', status: 'failed', detail: 'disk full',
        })
        expect(report.steps.find(step => step.id === 'configSummary')!.status).toBe('skipped')
        expect(report.steps.find(step => step.id === 'pruneStaleScripts')!.status).toBe('skipped')
    })
})
