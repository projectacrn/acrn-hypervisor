import {describe, expect, it} from 'vitest'
import {createPinia, setActivePinia} from 'pinia'
import {useScenarioStore} from '../../src/stores/scenario'
import {referenceScenario} from './fixtures'

describe('golden VM name reference updates', () => {
    it('renames IVSHMEM and VUART references without pruning regions', () => {
        const scenario = referenceScenario()

        setActivePinia(createPinia())
        const store = useScenarioStore()
        store.load(scenario)
        store.renameVm('old', 'renamed')

        expect(scenario.hv).toMatchSnapshot()
    })

    it('deletes references, drops VUART connections and prunes IVSHMEM below two VMs', () => {
        const scenario = referenceScenario()

        setActivePinia(createPinia())
        const store = useScenarioStore()
        store.load(scenario)
        store.renameVm('old', '')

        expect(scenario.hv).toMatchSnapshot()
    })
})
