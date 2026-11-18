import {describe, expect, it} from 'vitest'
import configurator from '../../src/lib/acrn'
import {complexScenario} from './fixtures'

describe('golden scenario XML', () => {
    it('serializes HV, CAT, IVSHMEM, VUART and every VM load order', () => {
        const scenario = {'acrn-config': complexScenario()}

        expect(configurator.convertScenarioToXML(scenario)).toMatchSnapshot()
    })
})
