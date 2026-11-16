import {afterEach, describe, expect, it, vi} from 'vitest'
import Config from '../src/pages/Config.vue'
import configurator from '../src/lib/acrn'

afterEach(() => {
    vi.restoreAllMocks()
    vi.unstubAllGlobals()
})

describe('scenario editing', () => {
    it('clears the form while Scenario data is not loaded yet', () => {
        const context = {activeVMID: -1, scenario: {}, currentFormData: {stale: true}}

        expect(() => Config.methods.updateCurrentFormData.call(context)).not.toThrow()
        expect(context.currentFormData).toEqual({})
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
        const cleaning = Config.methods.cleanLaunchScript.call({WorkingFolder: '/work/'})
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

        await expect(Config.methods.cleanLaunchScript.call({WorkingFolder: '/work/'}))
            .rejects.toThrow('read-only filesystem')
    })

    it('keeps newly generated launch scripts while removing stale ones', async () => {
        vi.spyOn(configurator, 'readDir').mockResolvedValue([
            {path: '/work/launch_user_vm_id1.sh'},
            {path: '/work/launch_user_vm_id2.sh'}
        ])
        const removeFile = vi.spyOn(configurator, 'removeFile').mockResolvedValue()

        await Config.methods.cleanLaunchScript.call(
            {WorkingFolder: '/work/'}, new Set(['launch_user_vm_id1.sh']))

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

        Config.methods.assignVMID.call(context)

        expect(context.activeVMID).toBe(1)
        expect(context.scenario.vm[context.activeVMID]).toBe(service)
    })

    it('keeps the active VM PCI devices available but excludes devices owned by other VMs', () => {
        const items = {enum: ['00:02.0', '00:04.0'], enumNames: ['00:02.0', '00:04.0']}
        const context = {
            scenario: {vm: [{pci_devs: {pci_dev: ['00:03.0']}}]},
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

        Config.methods.updateSchema.call(context, 0)

        expect(items.enum).toEqual(['00:02.0', '00:03.0', '00:04.0'])
        expect(items.enumNames).toEqual(items.enum)
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

        await Config.methods.deleteVM.call(context)

        expect(context.scenario.vm.map(vm => vm.name)).toEqual(['pre'])
        expect(context.scenario.hv.FEATURES.IVSHMEM.IVSHMEM_REGION).toEqual([])
        expect(context.scenario.hv.vuart_connections.vuart_connection).toEqual([])
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
    it('shows duplicate VM names in the result modal without saving', async () => {
        const alert = vi.fn()
        vi.stubGlobal('alert', alert)
        const context = {
            saving: false,
            showTotalMessageFlag: false,
            totalMsg: '',
            scenario: {
                vm: [
                    {'@id': 0, name: 'POST_STD_VM1'},
                    {'@id': 1, name: 'POST_STD_VM1'}
                ]
            },
            confirmVmName: Config.methods.confirmVmName,
            showSaveError: Config.methods.showSaveError
        }

        const result = await Config.methods.saveScenario.call(context)

        expect(result).toContain('Multiple VMs have the same name: POST_STD_VM1')
        expect(result).toContain('Scenario xml save failed')
        expect(context.showTotalMessageFlag).toBe(true)
        expect(context.saving).toBe(false)
        expect(alert).not.toHaveBeenCalled()
    })

    it('does not remove launch scripts when scenario validation fails', async () => {
        const writeFile = vi.spyOn(configurator, 'writeFile').mockResolvedValue()
        vi.spyOn(configurator.cat, 'getScenarioDataFromCAT').mockReturnValue(null)
        vi.spyOn(configurator.pythonObject, 'validateScenario').mockReturnValue({
            syntactic_errors: [{message: 'invalid value', paths: ['/acrn-config/hv/setting']}],
            semantic_errors: []
        })
        const cleanLaunchScript = vi.fn()
        const context = {
            saving: false,
            showTotalMessageFlag: false,
            totalMsg: '',
            scenario: {hv: {}, vm: [{'@id': 0, load_order: 'SERVICE_VM'}]},
            board: {content: '<board/>'},
            WorkingFolder: '/work/',
            confirmVmName: () => false,
            assignVMID: vi.fn(),
            applyScenarioDefaults: async scenario => scenario,
            scenarioToXML: () => '<scenario/>',
            updateCurrentFormSchema: vi.fn(),
            updateCurrentFormData: vi.fn(),
            cleanLaunchScript,
            translateErrors: vi.fn().mockReturnValue({})
        }

        const result = await Config.methods.saveScenario.call(context)

        expect(cleanLaunchScript).not.toHaveBeenCalled()
        expect(writeFile).not.toHaveBeenCalled()
        expect(result).toContain('Settings validate failed')
        expect(result).toContain('Scenario xml save failed')
        expect(result).not.toContain('Scenario xml saved')
        expect(context.saving).toBe(false)
        expect(context.showTotalMessageFlag).toBe(true)
    })

    it('preserves old launch scripts when replacement generation fails', async () => {
        const writeFile = vi.spyOn(configurator, 'writeFile').mockResolvedValue()
        vi.spyOn(configurator.cat, 'getScenarioDataFromCAT').mockReturnValue(null)
        vi.spyOn(configurator.pythonObject, 'validateScenario').mockReturnValue({
            syntactic_errors: [], semantic_errors: []
        })
        vi.spyOn(configurator.pythonObject, 'generateConfigSummary').mockReturnValue('summary')
        vi.spyOn(configurator.pythonObject, 'generateLaunchScript').mockImplementation(() => {
            throw new Error('generator failed')
        })
        const cleanLaunchScript = vi.fn()
        const context = {
            saving: false,
            showTotalMessageFlag: false,
            totalMsg: '',
            scenario: {hv: {}, vm: [{'@id': 0, load_order: 'POST_LAUNCHED_VM'}]},
            board: {content: '<board/>'},
            WorkingFolder: '/work/',
            confirmVmName: () => false,
            assignVMID: vi.fn(),
            applyScenarioDefaults: async scenario => scenario,
            scenarioToXML: () => '<scenario/>',
            updateCurrentFormSchema: vi.fn(),
            updateCurrentFormData: vi.fn(),
            cleanLaunchScript,
            translateErrors: vi.fn().mockReturnValue({})
        }

        const result = await Config.methods.saveScenario.call(context)

        expect(cleanLaunchScript).not.toHaveBeenCalled()
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
        vi.spyOn(configurator.cat, 'getScenarioDataFromCAT').mockReturnValue(null)
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
            confirmVmName: () => false,
            assignVMID: vi.fn(),
            applyScenarioDefaults: async scenario => scenario,
            scenarioToXML: () => '<scenario/>',
            updateCurrentFormSchema: vi.fn(),
            updateCurrentFormData: vi.fn(),
            cleanLaunchScript: vi.fn().mockResolvedValue(),
            translateErrors: vi.fn().mockReturnValue({})
        }

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
