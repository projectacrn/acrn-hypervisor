import {describe, expect, it} from 'vitest'
import {mapErrors} from '../../src/domain/validation/mapErrors'

describe('domain validation error mapping', () => {
    it('maps HV and one-based VM paths and deduplicates repeated errors per bucket', () => {
        const hv = {source: 'syntactic', message: 'HV', paths: ['/acrn-config/hv/RELEASE']}
        const first = {source: 'syntactic', message: 'first', paths: ['/acrn-config/vm/name']}
        const shared = {
            source: 'semantic',
            message: 'shared',
            paths: ['/acrn-config/vm[2]/cpu_affinity', '/acrn-config/vm[1]/name']
        }
        const repeated = {
            source: 'semantic',
            message: 'repeated',
            paths: ['/acrn-config/vm[3]/name', '/acrn-config/vm[3]/name']
        }

        expect(mapErrors({
            syntactic_errors: [hv, first],
            semantic_errors: [shared, repeated]
        }, {vm: [{'@id': 10}, {'@id': 20}, {'@id': 30}]})).toEqual({
            '-1': [hv],
            10: [first, shared],
            20: [shared],
            30: [repeated]
        })
    })

    it('keeps unrelated paths in the HV bucket', () => {
        const issue = {message: 'other', paths: ['/acrn-config/other/value']}
        expect(mapErrors({syntactic_errors: [issue], semantic_errors: []}, {vm: []}))
            .toEqual({'-1': [issue]})
    })
})
