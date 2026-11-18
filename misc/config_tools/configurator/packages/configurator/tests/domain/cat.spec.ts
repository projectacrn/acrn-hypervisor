import {describe, expect, it} from 'vitest'
import {correctSwitches, deriveCatView} from '../../src/domain/cat/derive'
import {hexToRange, rangeToHex} from '../../src/domain/cat/masks'
import {buildCacheRegion, setClosMask} from '../../src/domain/cat/toScenario'
import type {CacheRegion, CatInput} from '../../src/domain/cat/model'
import {catScenario} from '../golden/fixtures'

function makeInput(): CatInput {
    return {
        doc: catScenario(),
        board: {
            CAT_INFO: [{
                id: '0',
                level: 3,
                type: 'Unified',
                cache_size: 8 * 1024 * 1024,
                capacity_mask_length: 8,
                processors: [0, 1, 2, 3]
            }]
        },
        schema: {
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
    }
}

describe('domain CAT masks', () => {
    it.each([
        ['0x1ff', 8, [0, 8]],
        ['0x101', 8, [6, 8]],
        ['0x1a5', 8, [3, 8]],
        ['0x0f', 8, [4, 8]],
        ['0x81', 8, [0, 2]],
        ['0x00', 8, [0, 0]],
        ['0x0abc', 12, [0, 7]]
    ])('preserves hexToRange behavior for %s', (mask, max, range) => {
        expect(hexToRange(mask, max)).toEqual(range)
    })

    it.each([
        [[0, 0], 8, '0x00'],
        [[0, 4], 8, '0xf0'],
        [[4, 8], 8, '0x0f'],
        [[2, 10], 12, '0x3fc']
    ])('preserves rangeToHex behavior for %s', (range, max, mask) => {
        expect(rangeToHex(range as [number, number], max)).toBe(mask)
    })
})

describe('domain CAT derivation', () => {
    it('derives sorted policies and normalized masks without mutating the document', () => {
        const input = makeInput()
        const before = JSON.stringify(input.doc)

        const view = deriveCatView(input)!

        expect(JSON.stringify(input.doc)).toBe(before)
        expect(view.errorMsg).toBe('')
        expect(view.summary).toEqual({3: {count: 1, 0: 1}})
        expect(view.regions[0].data.RTCore.map(policy => policy.VM)).toEqual(['pre'])
        expect(view.regions[0].data.RTCore[0].CLOS_MASK).toBe('0x78')
        expect(view.regions[0].data.Standard.map(policy => policy.VM))
            .toEqual(['post', 'post', 'service', 'service', 'service'])

        const cacheRegion = buildCacheRegion(view)!
        expect(cacheRegion.CACHE_ALLOCATION[0].POLICY.map(policy => [policy.VM, policy.VCPU]))
            .toEqual([
                ['pre', 0],
                ['post', 0], ['post', 1],
                ['service', 0], ['service', 1], ['service', 2]
            ])
    })

    it('returns RDT, CDP, and VCAT corrections without applying them', () => {
        const input = makeInput()
        input.doc.hv!.FEATURES!.RDT = {RDT_ENABLED: 'n', CDP_ENABLED: 'y', VCAT_ENABLED: 'y'}

        const result = correctSwitches(input.doc)

        expect(result.switches).toEqual({RDT_ENABLED: 'y', CDP_ENABLED: 'y', VCAT_ENABLED: 'n'})
        expect(result.corrections).toEqual({RDT_ENABLED: 'y', VCAT_ENABLED: 'n'})
        expect(input.doc.hv!.FEATURES!.RDT).toEqual({RDT_ENABLED: 'n', CDP_ENABLED: 'y', VCAT_ENABLED: 'y'})
        expect(deriveCatView(input)!.regions[0].data.RTCore.map(policy => policy.TYPE))
            .toEqual(['Code', 'Data'])
    })

    it('reports every VM without CPU affinity', () => {
        const input = makeInput()
        input.doc.vm = [
            {'@id': 0, name: 'first', load_order: 'PRE_LAUNCHED_VM'},
            {'@id': 1, name: 'second', load_order: 'POST_LAUNCHED_VM'},
            {'@id': 2, name: 'service', load_order: 'SERVICE_VM', cpu_affinity: null}
        ]

        const view = deriveCatView(input)!

        expect(view.errorMsg).toBe(
            'CPU affinity is not set for the following VMs:\n' +
            'VM first has no CPU affinity setting\n' +
            'VM second has no CPU affinity setting\n' +
            'Please set CPU affinity for all VMs'
        )
    })

    it('constructs region counts and per-level module ordering', () => {
        const input = makeInput()
        input.board.CAT_INFO = [
            {...input.board.CAT_INFO[0], id: 'b'},
            {...input.board.CAT_INFO[0], id: 'a'},
            {...input.board.CAT_INFO[0], id: 'l2', level: 2, processors: []}
        ]

        const view = deriveCatView(input)!

        expect(view.summary).toEqual({
            3: {count: 2, b: 1, a: 2},
            2: {count: 1, l2: 1}
        })
    })

    it('returns no cache region when CAT is disabled or unsupported', () => {
        const input = makeInput()
        input.doc.hv!.FEATURES!.RDT = {RDT_ENABLED: 'n', CDP_ENABLED: 'n', VCAT_ENABLED: 'n'}
        expect(buildCacheRegion(deriveCatView(input))).toBeNull()

        input.board.CAT_INFO = []
        expect(buildCacheRegion(deriveCatView(input))).toBeNull()
    })
})

describe('domain CAT scenario writes', () => {
    it('updates a policy without mutating the previous CACHE_REGION', () => {
        const original: CacheRegion = {
            CACHE_ALLOCATION: [{
                CACHE_ID: '0',
                CACHE_LEVEL: 3,
                POLICY: [{VM: 'pre', VCPU: 0, TYPE: 'Unified', CLOS_MASK: '0x0f'}]
            }]
        }

        const updated = setClosMask(original, {
            cacheId: '0', cacheLevel: 3, vm: 'pre', vcpu: 0, type: 'Unified', closMask: '0x3c'
        })

        expect(updated).not.toBe(original)
        expect(updated.CACHE_ALLOCATION[0].POLICY[0].CLOS_MASK).toBe('0x3c')
        expect(original.CACHE_ALLOCATION[0].POLICY[0].CLOS_MASK).toBe('0x0f')
    })

    it('appends a missing policy and creates a missing allocation', () => {
        const updated = setClosMask(null, {
            cacheId: '1', cacheLevel: 2, vm: 'post', vcpu: 1, type: 'Data', closMask: '0x0f'
        })

        expect(updated).toEqual({
            CACHE_ALLOCATION: [{
                CACHE_ID: '1',
                CACHE_LEVEL: 2,
                POLICY: [{VM: 'post', VCPU: 1, TYPE: 'Data', CLOS_MASK: '0x0f'}]
            }]
        })
    })
})
