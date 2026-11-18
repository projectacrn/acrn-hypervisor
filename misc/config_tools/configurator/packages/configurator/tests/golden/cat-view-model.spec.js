import {describe, expect, it} from 'vitest'
import {createPinia, setActivePinia} from 'pinia'
import {useScenarioStore} from '../../src/stores/scenario'
import {buildCacheRegion} from '../../src/domain/cat/toScenario'
import {catScenario} from './fixtures'

function plain(value) {
    return JSON.parse(JSON.stringify(value))
}

function loadCAT() {
    const scenario = catScenario()
    const board = {
        CAT_INFO: [{
            id: '0',
            level: 3,
            type: 'Unified',
            cache_size: 8 * 1024 * 1024,
            capacity_mask_length: 8,
            processors: [0, 1, 2, 3]
        }]
    }
    const schemas = {
        HV: {
            BasicConfigType: {
                definitions: {
                    CPUAffinityConfiguration: {
                        properties: {pcpu_id: {enum: [0, 1, 2, 3]}}
                    }
                }
            }
        }
    }

    setActivePinia(createPinia())
    const store = useScenarioStore()
    store.board = board
    store.schemas = schemas
    store.load(scenario)

    return {scenario, board, schemas, store}
}

describe('golden CAT view model', () => {
    it('pins derived regions, summary, policies and scenario policies together', () => {
        const {store} = loadCAT()

        // Pin the current 0x55 -> 0x78 normalization for non-contiguous masks.
        const view = store.getCatView()
        const scenarioData = buildCacheRegion(view)

        expect(plain({
            errorMsg: view.errorMsg,
            regions: view.regions,
            summary: view.summary,
            scenarioData
        })).toMatchSnapshot()
    })
})
