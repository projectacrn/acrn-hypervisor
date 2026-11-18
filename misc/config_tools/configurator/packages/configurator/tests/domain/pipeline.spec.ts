import {describe, expect, it, vi} from 'vitest'
import {
    assignVmIds,
    populateScenarioDefaults,
    runSavePipeline
} from '../../src/domain/artifacts/pipeline'
import type {SavePipelineDependencies, SaveReport, SaveStepId} from '../../src/domain/artifacts/pipeline'

const valid = () => ({syntactic_errors: [], semantic_errors: []})

function setup(postLaunched = true) {
    const events: string[] = []
    const scenario = {
        hv: {},
        vm: [
            {'@id': 8, name: 'post', load_order: postLaunched ? 'POST_LAUNCHED_VM' : 'SERVICE_VM'},
            {'@id': 4, name: 'pre', load_order: 'PRE_LAUNCHED_VM'}
        ]
    }
    const input = {
        scenario,
        activeVmId: 8,
        boardXml: '<board/>',
        workingFolder: '/work/',
        vmNameError: false as string | false
    }
    const dependencies: SavePipelineDependencies = {
        scenarioToXml: vi.fn(() => {
            events.push('xml')
            return '<scenario/>'
        }),
        populateDefaults: vi.fn(() => {
            events.push('defaults')
            return {
                json: {
                    'acrn-config': {
                        hv: {
                            CACHE_REGION: {CACHE_ALLOCATION: []},
                            FEATURES: {RDT: {RDT_ENABLED: 'n'}}
                        },
                        vm: scenario.vm
                    }
                }
            }
        }),
        getCacheRegion: () => {
            events.push('cat')
            return {CACHE_ALLOCATION: [{CACHE_ID: '0'}]}
        },
        writeFile: vi.fn(path => {
            events.push('write:' + path)
            return Promise.resolve()
        }),
        validateScenario: vi.fn(() => {
            events.push('validate')
            return valid()
        }),
        generateConfigSummary: vi.fn(() => {
            events.push('summary')
            return 'summary'
        }),
        generateLaunchScript: vi.fn(() => {
            events.push('launch')
            return {'launch_post.sh': 'launch'}
        }),
        listFiles: vi.fn(() => {
            events.push('list')
            return Promise.resolve([
                {path: '/work/launch_post.sh'},
                {path: '/work/launch_stale.sh'},
                {path: '/work/custom.sh'}
            ])
        }),
        removeFile: vi.fn(path => {
            events.push('remove:' + path)
            return Promise.resolve()
        }),
        onVmIdsAssigned: () => { events.push('assign') },
        onScenarioPopulated: () => { events.push('state') },
        onScenarioWritten: () => { events.push('written') }
    }
    return {input, dependencies, events, scenario}
}

function step(report: SaveReport, id: SaveStepId) {
    return report.steps.find(candidate => candidate.id === id)
}

describe('save pipeline', () => {
    it('runs every stage in legacy order and prunes only stale launch scripts', async () => {
        const {input, dependencies, events} = setup()

        const report = await runSavePipeline(input, dependencies)

        expect(events).toEqual([
            'assign', 'cat', 'xml', 'defaults', 'xml', 'state',
            'write:/work/scenario.xml', 'written', 'validate', 'summary', 'launch',
            'write:/work/config_summary.rst', 'write:/work/launch_post.sh',
            'list', 'remove:/work/launch_stale.sh'
        ])
        expect(report.success).toBe(true)
        expect(report.activeVmId).toBe(1)
        expect(report.scenario.hv.CACHE_REGION).toBeUndefined()
        expect(report.steps.filter(candidate => candidate.status === 'ok').map(candidate => candidate.id))
            .toEqual([
                'assignVmIds', 'populateDefaults', 'writeScenario', 'validate',
                'configSummary', 'launchScripts', 'pruneStaleScripts'
            ])
    })

    it('stops before validation and generators when the Scenario write fails', async () => {
        const {input, dependencies} = setup()
        dependencies.writeFile = vi.fn().mockRejectedValue(new Error('disk full'))

        const report = await runSavePipeline(input, dependencies)

        expect(report.success).toBe(false)
        expect(step(report, 'writeScenario')).toMatchObject({status: 'failed', detail: 'disk full'})
        expect(dependencies.validateScenario).not.toHaveBeenCalled()
        expect(dependencies.generateConfigSummary).not.toHaveBeenCalled()
        expect(dependencies.generateLaunchScript).not.toHaveBeenCalled()
        expect(dependencies.listFiles).not.toHaveBeenCalled()
    })

    it('keeps the saved Scenario but generates nothing after validation fails', async () => {
        const {input, dependencies} = setup()
        dependencies.validateScenario = vi.fn().mockReturnValue({
            syntactic_errors: [{message: 'invalid'}], semantic_errors: []
        })
        const onValidated = vi.fn()
        dependencies.onValidated = onValidated

        const report = await runSavePipeline(input, dependencies)

        expect(dependencies.writeFile).toHaveBeenCalledTimes(1)
        expect(dependencies.writeFile).toHaveBeenCalledWith('/work/scenario.xml', '<scenario/>')
        expect(onValidated).toHaveBeenCalledOnce()
        expect(step(report, 'validate')?.status).toBe('failed')
        expect(dependencies.generateConfigSummary).not.toHaveBeenCalled()
        expect(dependencies.generateLaunchScript).not.toHaveBeenCalled()
        expect(dependencies.listFiles).not.toHaveBeenCalled()
    })

    it('does not write the summary or prune old scripts when launch generation fails', async () => {
        const {input, dependencies} = setup()
        dependencies.generateLaunchScript = vi.fn(() => {
            throw new Error('generator failed')
        })

        const report = await runSavePipeline(input, dependencies)

        expect(dependencies.generateConfigSummary).toHaveBeenCalledOnce()
        expect(dependencies.writeFile).toHaveBeenCalledTimes(1)
        expect(dependencies.writeFile).toHaveBeenCalledWith('/work/scenario.xml', '<scenario/>')
        expect(step(report, 'configSummary')?.status).toBe('skipped')
        expect(step(report, 'launchScripts')).toMatchObject({
            status: 'failed', detail: 'generator failed'
        })
        expect(dependencies.listFiles).not.toHaveBeenCalled()
    })

    it('skips launch generation for no post-launched VM and still prunes stale scripts', async () => {
        const {input, dependencies} = setup(false)

        const report = await runSavePipeline(input, dependencies)

        expect(report.success).toBe(true)
        expect(report.needSaveLaunchScript).toBe(false)
        expect(dependencies.generateLaunchScript).not.toHaveBeenCalled()
        expect(dependencies.writeFile).toHaveBeenCalledTimes(2)
        expect(step(report, 'launchScripts')?.status).toBe('skipped')
        expect(step(report, 'pruneStaleScripts')?.status).toBe('ok')
        expect(dependencies.removeFile).toHaveBeenCalledWith('/work/launch_stale.sh')
        expect(dependencies.removeFile).toHaveBeenCalledWith('/work/launch_post.sh')
    })

    it('generates launch content before a summary write failure and never prunes', async () => {
        const {input, dependencies} = setup()
        dependencies.writeFile = vi.fn(path => path.endsWith('config_summary.rst')
            ? Promise.reject(new Error('summary read-only'))
            : Promise.resolve())

        const report = await runSavePipeline(input, dependencies)

        expect(dependencies.generateLaunchScript).toHaveBeenCalledOnce()
        expect(step(report, 'configSummary')).toMatchObject({
            status: 'failed', detail: 'summary read-only'
        })
        expect(dependencies.listFiles).not.toHaveBeenCalled()
    })
})

describe('save normalization', () => {
    it('keeps the active VM object stable while sorting and reassigning IDs', () => {
        const service = {'@id': 7, load_order: 'SERVICE_VM'}
        const scenario = {
            vm: [
                service,
                {'@id': 3, load_order: 'POST_LAUNCHED_VM'},
                {'@id': 9, load_order: 'PRE_LAUNCHED_VM'}
            ]
        }

        expect(assignVmIds(scenario, 7)).toBe(1)
        expect(scenario.vm.map(vm => vm['@id'])).toEqual([0, 1, 2])
        expect(scenario.vm[1]).toBe(service)
    })

    it('drops CACHE_REGION only when defaults disable RDT', async () => {
        const scenario = {hv: {}, vm: []}
        const withRdt = enabled => ({
            json: {
                'acrn-config': {
                    hv: {
                        CACHE_REGION: {CACHE_ALLOCATION: []},
                        FEATURES: {RDT: {RDT_ENABLED: enabled}}
                    },
                    vm: []
                }
            }
        })

        const disabled = await populateScenarioDefaults(scenario, () => '<scenario/>', () => withRdt('n'))
        const enabled = await populateScenarioDefaults(scenario, () => '<scenario/>', () => withRdt('y'))

        expect(disabled.hv.CACHE_REGION).toBeUndefined()
        expect(enabled.hv.CACHE_REGION).toEqual({CACHE_ALLOCATION: []})
    })
})
