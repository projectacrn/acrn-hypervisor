import {afterEach, describe, expect, it, vi} from 'vitest'
import Config from '../src/pages/Config.vue'
import configurator from '../src/lib/acrn'
import {assignVmIds, pruneStaleLaunchScripts} from '../src/domain/artifacts/pipeline'
import {createPinia, setActivePinia} from 'pinia'
import {useScenarioStore} from '../src/stores/scenario'
import {useValidationStore} from '../src/stores/validation'

function mockDefaultsFor(context) {
    setActivePinia(createPinia())
    context.scenarioStore = useScenarioStore()
    context.scenarioStore.doc = context.scenario
    context.scenarioStore.board = context.board
    vi.spyOn(configurator, 'convertScenarioToXML').mockReturnValue('<scenario/>')
    vi.spyOn(configurator.pythonObject, 'populateDefaultValues').mockImplementation(() => ({
        json: {
            'acrn-config': {
                ...context.scenario,
                hv: {
                    ...context.scenario.hv,
                    FEATURES: {RDT: {RDT_ENABLED: 'n'}}
                }
            }
        }
    }))
    vi.spyOn(configurator, 'readDir').mockResolvedValue([])
}

const cleanupDependencies = () => ({
    listFiles: (path, recursive) => configurator.readDir(path, recursive),
    removeFile: path => configurator.removeFile(path)
})

afterEach(() => {
    vi.restoreAllMocks()
    vi.unstubAllGlobals()
})

describe('scenario editing', () => {
    it('clears the form while Scenario data is not loaded yet', () => {
        setActivePinia(createPinia())
        const store = useScenarioStore()

        expect(store.activeFormData).toEqual({})
    })

    it('clears previous workspace data when a configuration mounts', () => {
        setActivePinia(createPinia())
        const scenarioStore = useScenarioStore()
        const validationStore = useValidationStore()
        scenarioStore.board = {content: '<board/>'}
        scenarioStore.schemas = {HV: {}}
        scenarioStore.doc = {hv: {}, vm: []}
        scenarioStore.activeVmId = 4
        scenarioStore.dirty = true
        validationStore.errors = {4: [{message: 'stale'}]}
        const context = {scenarioStore, validationStore, isNewConfig: 'true'}

        Config.mounted.call(context)

        expect(scenarioStore.board).toBeNull()
        expect(scenarioStore.schemas).toBeNull()
        expect(scenarioStore.doc).toBeNull()
        expect(scenarioStore.activeVmId).toBe(-1)
        expect(scenarioStore.dirty).toBe(false)
        expect(validationStore.errors).toEqual({})
        expect(context.showFlag).toBe(true)
    })

    it('does not enable CAT switches when the board has no CAT regions', () => {
        setActivePinia(createPinia())
        const store = useScenarioStore()
        const rdt = {RDT_ENABLED: 'n', CDP_ENABLED: 'y', VCAT_ENABLED: 'n'}
        store.doc = {hv: {FEATURES: {RDT: rdt}}, vm: []}
        store.board = {CAT_INFO: []}
        store.schemas = {}

        expect(store.getCatView()).toEqual({
            errorMsg: "This board(or CPU) doesn't support CAT which means there is no any CAT capability.</br>" +
                'There is no Cache Region was shared which means "all support CAT region only have one CPU core".',
            regions: [],
            summary: {}
        })
        expect(rdt).toEqual({RDT_ENABLED: 'n', CDP_ENABLED: 'y', VCAT_ENABLED: 'n'})
    })

    it('removes references when a VM name is cleared', () => {
        setActivePinia(createPinia())
        const store = useScenarioStore()
        store.doc = {
            hv: {
                FEATURES: {
                    IVSHMEM: {
                        IVSHMEM_REGION: [{
                            IVSHMEM_VMS: {
                                IVSHMEM_VM: [{VM_NAME: 'VM0'}, {VM_NAME: 'VM1'}]
                            }
                        }]
                    }
                },
                vuart_connections: {
                    vuart_connection: [{endpoint: [{vm_name: 'VM0'}, {vm_name: 'VM1'}]}]
                },
                CACHE_REGION: {
                    CACHE_ALLOCATION: [{POLICY: [{VM: 'VM0'}, {VM: 'VM1'}]}]
                }
            },
            vm: [{
                '@id': 0,
                name: 'VM0',
                load_order: 'PRE_LAUNCHED_VM',
                vm_type: 'STANDARD_VM',
                cpu_affinity: {pcpu: []}
            }, {
                '@id': 1,
                name: 'VM1',
                load_order: 'PRE_LAUNCHED_VM',
                vm_type: 'STANDARD_VM',
                cpu_affinity: {pcpu: []}
            }]
        }
        const cleared = {...store.doc.vm[0], name: ''}

        store.updateActiveForm(0, cleared)

        expect(store.doc.hv.FEATURES.IVSHMEM.IVSHMEM_REGION).toEqual([])
        expect(store.doc.hv.vuart_connections.vuart_connection).toEqual([])
        expect(store.doc.hv.CACHE_REGION.CACHE_ALLOCATION[0].POLICY).toEqual([{VM: 'VM1'}])
    })

    it('waits for old launch scripts to be removed and preserves unrelated scripts', async () => {
        vi.spyOn(configurator, 'readDir').mockResolvedValue([
            {path: '/work/launch_user_vm_id1.sh'},
            {path: '/work/custom.sh'}
        ])
        let finishRemoval
        const removal = new Promise(resolve => { finishRemoval = resolve })
        const removeFile = vi.spyOn(configurator, 'removeFile').mockReturnValue(removal)

        let settled = false
        const cleaning = pruneStaleLaunchScripts('/work/', new Set(), cleanupDependencies())
            .then(() => { settled = true })
        await vi.waitFor(() => expect(removeFile).toHaveBeenCalledWith('/work/launch_user_vm_id1.sh'))

        expect(removeFile).toHaveBeenCalledTimes(1)
        expect(settled).toBe(false)
        finishRemoval()
        await cleaning
        expect(settled).toBe(true)
    })

    it('propagates launch script removal failures', async () => {
        vi.spyOn(configurator, 'readDir').mockResolvedValue([{path: '/work/launch_vm.sh'}])
        vi.spyOn(configurator, 'removeFile').mockRejectedValue(new Error('read-only filesystem'))

        await expect(pruneStaleLaunchScripts('/work/', new Set(), cleanupDependencies()))
            .rejects.toThrow('read-only filesystem')
    })

    it('keeps newly generated launch scripts while removing stale ones', async () => {
        vi.spyOn(configurator, 'readDir').mockResolvedValue([
            {path: '/work/launch_user_vm_id1.sh'},
            {path: '/work/launch_user_vm_id2.sh'}
        ])
        const removeFile = vi.spyOn(configurator, 'removeFile').mockResolvedValue()

        await pruneStaleLaunchScripts(
            '/work/', new Set(['launch_user_vm_id1.sh']), cleanupDependencies())

        expect(removeFile).toHaveBeenCalledTimes(1)
        expect(removeFile).toHaveBeenCalledWith('/work/launch_user_vm_id2.sh')
    })

    it('keeps the active VM attached to the same object when IDs are reassigned', () => {
        const service = {'@id': 7, load_order: 'SERVICE_VM'}
        const context = {
            activeVMID: 7,
            scenario: {
                vm: [
                    service,
                    {'@id': 3, load_order: 'POST_LAUNCHED_VM'},
                    {'@id': 9, load_order: 'PRE_LAUNCHED_VM'}
                ]
            }
        }

        context.activeVMID = assignVmIds(context.scenario, context.activeVMID)

        expect(context.activeVMID).toBe(1)
        expect(context.scenario.vm[context.activeVMID]).toBe(service)
    })

    it('keeps the active VM PCI devices available but excludes devices owned by other VMs', () => {
        const items = {enum: ['00:02.0', '00:04.0'], enumNames: ['00:02.0', '00:04.0']}
        const context = {
            scenario: {vm: [{'@id': 0, load_order: 'PRE_LAUNCHED_VM', pci_devs: {pci_dev: ['00:03.0']}}]},
            schemas: {
                PreLaunchedVM: {
                    BasicConfigType: {
                        definitions: {
                            PCIDevsConfiguration: {properties: {pci_dev: {items}}}
                        }
                    }
                }
            }
        }

        setActivePinia(createPinia())
        const store = useScenarioStore()
        store.doc = context.scenario
        store.schemas = context.schemas
        store.activeVmId = 0
        store.updateActivePciSchema()

        const updatedItems = store.schemas.PreLaunchedVM.BasicConfigType.definitions
            .PCIDevsConfiguration.properties.pci_dev.items
        expect(updatedItems.enum).toEqual(['00:02.0', '00:03.0', '00:04.0'])
        expect(updatedItems.enumNames).toEqual(updatedItems.enum)
        expect(items.enum).toEqual(['00:02.0', '00:04.0'])
    })

    it('removes dangling references for every VM cascade-deleted with the Service VM', async () => {
        vi.stubGlobal('confirm', vi.fn().mockResolvedValue(true))
        const context = {
            activeVMID: 1,
            scenario: {
                hv: {
                    FEATURES: {
                        IVSHMEM: {
                            IVSHMEM_REGION: [{
                                IVSHMEM_VMS: {IVSHMEM_VM: [{VM_NAME: 'post'}, {VM_NAME: 'service'}]}
                            }]
                        }
                    },
                    vuart_connections: {
                        vuart_connection: [{endpoint: [{vm_name: 'post'}, {vm_name: 'service'}]}]
                    },
                    CACHE_REGION: {
                        CACHE_ALLOCATION: [{
                            POLICY: [{VM: 'post'}, {VM: 'service'}, {VM: 'pre'}]
                        }]
                    }
                },
                vm: [
                    {'@id': 0, name: 'post', load_order: 'POST_LAUNCHED_VM'},
                    {'@id': 1, name: 'service', load_order: 'SERVICE_VM'},
                    {'@id': 2, name: 'pre', load_order: 'PRE_LAUNCHED_VM'}
                ]
            },
            updateCurrentFormSchema: vi.fn(),
            updateCurrentFormData: vi.fn()
        }
        context.vmNameChange = (newName, oldName) =>
            Config.methods.vmNameChange.call(context, newName, oldName)

        setActivePinia(createPinia())
        context.scenarioStore = useScenarioStore()
        context.scenarioStore.doc = context.scenario
        context.scenarioStore.activeVmId = 1

        await Config.methods.deleteVM.call(context)

        expect(context.scenario.vm.map(vm => vm.name)).toEqual(['pre'])
        expect(context.scenario.hv.FEATURES.IVSHMEM.IVSHMEM_REGION).toEqual([])
        expect(context.scenario.hv.vuart_connections.vuart_connection).toEqual([])
        expect(context.scenario.hv.CACHE_REGION.CACHE_ALLOCATION[0].POLICY).toEqual([{VM: 'pre'}])
    })

    it('includes syntactic errors in the per-tab error list', () => {
        const error = {message: 'invalid value', paths: ['/acrn-config/hv/setting']}
        const errors = Config.methods.translateErrors({
            syntactic_errors: [error],
            semantic_errors: []
        }, {vm: []})

        expect(errors[-1]).toEqual([error])
    })
})

describe('scenario saving', () => {
    it('saves the Scenario but skips generated artifacts for duplicate VM names', async () => {
        const writeFile = vi.spyOn(configurator, 'writeFile').mockResolvedValue()
        vi.spyOn(configurator.pythonObject, 'validateScenario').mockReturnValue({
            syntactic_errors: [], semantic_errors: []
        })
        const generateConfigSummary = vi.spyOn(configurator.pythonObject, 'generateConfigSummary')
        const context = {
            saving: false,
            showTotalMessageFlag: false,
            totalMsg: '',
            scenario: {
                hv: {},
                vm: [
                    {'@id': 0, name: 'POST_STD_VM1', load_order: 'SERVICE_VM'},
                    {'@id': 1, name: 'POST_STD_VM1', load_order: 'SERVICE_VM'}
                ]
            },
            board: {content: '<board/>'},
            WorkingFolder: '/work/',
            formatSaveReport: Config.methods.formatSaveReport,
            confirmVmName: Config.methods.confirmVmName,
            scenarioToXML: () => '<scenario/>',
            updateCurrentFormSchema: vi.fn(),
            updateCurrentFormData: vi.fn(),
            translateErrors: vi.fn().mockReturnValue({})
        }
        mockDefaultsFor(context)

        const result = await Config.methods.saveScenario.call(context)

        expect(writeFile).toHaveBeenCalledTimes(1)
        expect(writeFile).toHaveBeenCalledWith('/work/scenario.xml', '<scenario/>')
        expect(generateConfigSummary).not.toHaveBeenCalled()
        expect(configurator.readDir).not.toHaveBeenCalled()
        expect(result).toContain('Multiple VMs have the same name: POST_STD_VM1')
        expect(result).toContain('Scenario xml saved')
        expect(result).toContain('Settings validate failed')
        expect(result).not.toContain('Scenario xml save failed')
        expect(context.showTotalMessageFlag).toBe(true)
        expect(context.saving).toBe(false)
    })

    it('saves the Scenario but does not remove launch scripts when validation fails', async () => {
        const writeFile = vi.spyOn(configurator, 'writeFile').mockResolvedValue()
        vi.spyOn(configurator.pythonObject, 'validateScenario').mockReturnValue({
            syntactic_errors: [{message: 'invalid value', paths: ['/acrn-config/hv/setting']}],
            semantic_errors: []
        })
        const context = {
            saving: false,
            showTotalMessageFlag: false,
            totalMsg: '',
            scenario: {hv: {}, vm: [{'@id': 0, load_order: 'SERVICE_VM'}]},
            board: {content: '<board/>'},
            WorkingFolder: '/work/',
            formatSaveReport: Config.methods.formatSaveReport,
            confirmVmName: () => false,
            scenarioToXML: () => '<scenario/>',
            updateCurrentFormSchema: vi.fn(),
            updateCurrentFormData: vi.fn(),
            translateErrors: vi.fn().mockReturnValue({})
        }
        mockDefaultsFor(context)

        writeFile.mockImplementation(async () => {
            // Populating defaults can refresh the active form before the write resolves.
            context.scenarioStore.updateActiveForm(-1, {...context.scenarioStore.doc.hv})
        })
        const result = await Config.methods.saveScenario.call(context)

        expect(configurator.readDir).not.toHaveBeenCalled()
        expect(writeFile).toHaveBeenCalledTimes(1)
        expect(writeFile).toHaveBeenCalledWith('/work/scenario.xml', '<scenario/>')
        expect(useValidationStore().errors[-1]).toEqual([
            {message: 'invalid value', paths: ['/acrn-config/hv/setting']}
        ])
        expect(result).toContain('Settings validate failed')
        expect(result).toContain('Scenario xml saved')
        expect(result).not.toContain('Scenario xml save failed')
        expect(context.saving).toBe(false)
        expect(context.showTotalMessageFlag).toBe(true)
    })

    it('does not apply save validation after the workspace is replaced', async () => {
        const context = {
            scenario: {hv: {}, vm: [{'@id': 0, load_order: 'SERVICE_VM'}]},
            board: {content: '<board/>'}
        }
        mockDefaultsFor(context)
        vi.spyOn(configurator.pythonObject, 'validateScenario').mockReturnValue({
            syntactic_errors: [{message: 'stale error', paths: ['/acrn-config/hv/setting']}],
            semantic_errors: []
        })
        vi.spyOn(configurator, 'writeFile').mockImplementation(async () => {
            context.scenarioStore.reset()
            useValidationStore().clear()
        })

        const report = await context.scenarioStore.save('/work/')

        expect(report.validationErrors.syntactic_errors).toHaveLength(1)
        expect(useValidationStore().errors).toEqual({})
    })

    it('saves the Scenario when the validator itself throws', async () => {
        const writeFile = vi.spyOn(configurator, 'writeFile').mockResolvedValue()
        vi.spyOn(configurator.pythonObject, 'validateScenario').mockImplementation(() => {
            throw new Error('validator crashed')
        })
        const generateConfigSummary = vi.spyOn(configurator.pythonObject, 'generateConfigSummary')
        const context = {
            saving: false,
            showTotalMessageFlag: false,
            totalMsg: '',
            scenario: {hv: {}, vm: [{'@id': 0, load_order: 'SERVICE_VM'}]},
            board: {content: '<board/>'},
            WorkingFolder: '/work/',
            formatSaveReport: Config.methods.formatSaveReport,
            confirmVmName: () => false,
            scenarioToXML: () => '<scenario/>',
            updateCurrentFormSchema: vi.fn(),
            updateCurrentFormData: vi.fn(),
            translateErrors: vi.fn().mockReturnValue({})
        }
        mockDefaultsFor(context)

        const result = await Config.methods.saveScenario.call(context)

        expect(writeFile).toHaveBeenCalledTimes(1)
        expect(writeFile).toHaveBeenCalledWith('/work/scenario.xml', '<scenario/>')
        expect(generateConfigSummary).not.toHaveBeenCalled()
        expect(result).toContain('Scenario xml saved')
        expect(result).toContain('Settings validate failed')
        expect(result).toContain('validator crashed')
        expect(result).not.toContain('Scenario xml save failed')
        expect(context.showTotalMessageFlag).toBe(true)
        expect(context.saving).toBe(false)
    })

    it('reports a Scenario write failure separately from validation failure', async () => {
        const writeFile = vi.spyOn(configurator, 'writeFile').mockRejectedValue(new Error('disk full'))
        vi.spyOn(configurator.pythonObject, 'validateScenario').mockReturnValue({
            syntactic_errors: [], semantic_errors: []
        })
        const generateConfigSummary = vi.spyOn(configurator.pythonObject, 'generateConfigSummary')
        const context = {
            saving: false,
            showTotalMessageFlag: false,
            totalMsg: '',
            scenario: {hv: {}, vm: [{'@id': 0, load_order: 'SERVICE_VM'}]},
            board: {content: '<board/>'},
            WorkingFolder: '/work/',
            formatSaveReport: Config.methods.formatSaveReport,
            confirmVmName: () => false,
            scenarioToXML: () => '<scenario/>',
            updateCurrentFormSchema: vi.fn(),
            updateCurrentFormData: vi.fn(),
            translateErrors: vi.fn().mockReturnValue({})
        }
        mockDefaultsFor(context)

        const result = await Config.methods.saveScenario.call(context)

        expect(writeFile).toHaveBeenCalledTimes(1)
        expect(generateConfigSummary).not.toHaveBeenCalled()
        expect(result).toContain('Scenario xml save failed')
        expect(result).not.toContain('Scenario xml saved')
        expect(result).toContain('disk full')
        expect(context.showTotalMessageFlag).toBe(true)
        expect(context.saving).toBe(false)
    })

    it('preserves old launch scripts when replacement generation fails', async () => {
        const writeFile = vi.spyOn(configurator, 'writeFile').mockResolvedValue()
        vi.spyOn(configurator.pythonObject, 'validateScenario').mockReturnValue({
            syntactic_errors: [], semantic_errors: []
        })
        vi.spyOn(configurator.pythonObject, 'generateConfigSummary').mockReturnValue('summary')
        vi.spyOn(configurator.pythonObject, 'generateLaunchScript').mockImplementation(() => {
            throw new Error('generator failed')
        })
        const context = {
            saving: false,
            showTotalMessageFlag: false,
            totalMsg: '',
            scenario: {hv: {}, vm: [{'@id': 0, load_order: 'POST_LAUNCHED_VM'}]},
            board: {content: '<board/>'},
            WorkingFolder: '/work/',
            formatSaveReport: Config.methods.formatSaveReport,
            confirmVmName: () => false,
            scenarioToXML: () => '<scenario/>',
            updateCurrentFormSchema: vi.fn(),
            updateCurrentFormData: vi.fn(),
            translateErrors: vi.fn().mockReturnValue({})
        }
        mockDefaultsFor(context)

        const result = await Config.methods.saveScenario.call(context)

        expect(configurator.readDir).not.toHaveBeenCalled()
        expect(writeFile).toHaveBeenCalledTimes(1)
        expect(writeFile).toHaveBeenCalledWith('/work/scenario.xml', '<scenario/>')
        expect(result).toContain('Launch scripts generation failed')
    })

    it('opens the result only after all writes finish', async () => {
        let finishScenarioWrite
        const scenarioWrite = new Promise(resolve => { finishScenarioWrite = resolve })
        const writeFile = vi.spyOn(configurator, 'writeFile').mockImplementation(path => {
            return path.endsWith('scenario.xml') ? scenarioWrite : Promise.resolve()
        })
        vi.spyOn(configurator.pythonObject, 'validateScenario').mockReturnValue({
            syntactic_errors: [], semantic_errors: []
        })
        vi.spyOn(configurator.pythonObject, 'generateConfigSummary').mockReturnValue('summary')

        const context = {
            saving: false,
            showTotalMessageFlag: false,
            totalMsg: '',
            scenario: {hv: {}, vm: [{'@id': 0, load_order: 'SERVICE_VM'}]},
            board: {content: '<board/>'},
            WorkingFolder: '/work/',
            formatSaveReport: Config.methods.formatSaveReport,
            confirmVmName: () => false,
            scenarioToXML: () => '<scenario/>',
            updateCurrentFormSchema: vi.fn(),
            updateCurrentFormData: vi.fn(),
            translateErrors: vi.fn().mockReturnValue({})
        }
        mockDefaultsFor(context)

        let settled = false
        const saving = Config.methods.saveScenario.call(context).then(result => {
            settled = true
            return result
        })
        await vi.waitFor(() => expect(writeFile).toHaveBeenCalledWith('/work/scenario.xml', '<scenario/>'))

        expect(settled).toBe(false)
        expect(context.showTotalMessageFlag).toBe(false)
        finishScenarioWrite()
        const result = await saving

        expect(settled).toBe(true)
        expect(context.showTotalMessageFlag).toBe(true)
        expect(context.saving).toBe(false)
        expect(result).toContain('Document config_summary.rst generated')
        expect(result).toContain('All files successfully saved')
        expect(result).not.toContain('Launch scripts generated')
    })
})
