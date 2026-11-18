import {afterEach, beforeEach, describe, expect, it, vi} from 'vitest'
import {createPinia, setActivePinia} from 'pinia'
import configurator from '../../src/lib/acrn'
import {useScenarioStore} from '../../src/stores/scenario'
import type {ScenarioDoc} from '../../src/engine/types'
import type {CatRegionView, Policy} from '../../src/domain/cat/model'

function deferred<T>() {
    let resolve!: (value: T) => void
    let reject!: (reason?: unknown) => void
    const promise = new Promise<T>((resolvePromise, rejectPromise) => {
        resolve = resolvePromise
        reject = rejectPromise
    })
    return {promise, resolve, reject}
}

beforeEach(() => {
    setActivePinia(createPinia())
})

afterEach(() => {
    vi.restoreAllMocks()
})

describe('scenario getters and CAT guards', () => {
    it('returns empty values for missing schemas, documents, VMs, and names', () => {
        const store = useScenarioStore()

        expect(store.activeSchema).toBeNull()
        expect(store.activeFormData).toEqual({})
        expect(store.vmNames).toEqual([])

        store.schemas = {HV: {title: 'HV'}}
        store.activeVmId = 4
        expect(store.activeSchema).toBeNull()

        store.doc = {hv: {RELEASE: 'y'}, vm: []}
        expect(store.activeSchema).toBeNull()
        expect(store.activeFormData).toEqual({})
        expect(store.vmNames).toEqual([])
    })

    it('leaves an absent document or hypervisor untouched when correcting CAT switches', () => {
        const store = useScenarioStore()

        store.correctCatSwitches()
        expect(store.doc).toBeNull()

        store.doc = {hv: null, vm: []}
        store.correctCatSwitches()
        expect(store.doc.hv).toBeNull()
        expect(store.revision).toBe(0)
    })

    it('creates missing CAT feature containers and writes all default corrections', () => {
        const store = useScenarioStore()
        store.doc = {hv: {}, vm: []}

        store.correctCatSwitches()

        expect(store.doc.hv).toEqual({
            FEATURES: {
                RDT: {RDT_ENABLED: 'n', CDP_ENABLED: 'n', VCAT_ENABLED: 'n'}
            }
        })
    })

    it('applies conflicting CAT switch corrections inside an existing RDT object', () => {
        const store = useScenarioStore()
        store.doc = {
            hv: {FEATURES: {RDT: {RDT_ENABLED: 'n', CDP_ENABLED: 'y', VCAT_ENABLED: 'y'}}},
            vm: []
        }

        store.correctCatSwitches()

        expect(store.doc.hv.FEATURES.RDT).toEqual({
            RDT_ENABLED: 'y', CDP_ENABLED: 'y', VCAT_ENABLED: 'n'
        })
    })

    it('does not dirty the scenario when a CLOS mask has no hypervisor to update', () => {
        const store = useScenarioStore()
        const region = {id: '0', level: 3} as CatRegionView
        const policy: Policy = {VM: 'VM0', VCPU: 0, TYPE: 'Unified', CLOS_MASK: '0xff'}

        store.setClosMask(region, policy, '0x0f')
        expect(store.doc).toBeNull()

        store.doc = {hv: null, vm: []}
        store.setClosMask(region, policy, '0x0f')
        expect(store.doc.hv).toBeNull()
        expect(store.dirty).toBe(false)
        expect(store.revision).toBe(0)
    })
})

describe('scenario file loading', () => {
    it('loads a selected file with the default selection callback and resets editing state', async () => {
        const store = useScenarioStore()
        const doc = {hv: {RELEASE: 'y'}, vm: [{'@id': 0, name: 'VM0'}]}
        const loadScenario = vi.spyOn(configurator, 'loadScenario')
            .mockResolvedValue({'acrn-config': doc})
        store.activeVmId = 0
        store.dirty = true

        const result = await store.loadFromFile('/work/scenario.xml')

        expect(loadScenario).toHaveBeenCalledWith('/work/scenario.xml')
        expect(result).toEqual(doc)
        expect(store.doc).toEqual(doc)
        expect(store.activeVmId).toBe(-1)
        expect(store.dirty).toBe(false)
        expect(store.revision).toBe(2)
    })

    it('discards a successful result after the scenario revision changes', async () => {
        const store = useScenarioStore()
        const pending = deferred<Record<string, ScenarioDoc>>()
        vi.spyOn(configurator, 'loadScenario').mockReturnValue(pending.promise)
        const current = {hv: {RELEASE: 'n'}, vm: []}
        store.doc = current
        store.dirty = true

        const loading = store.loadFromFile('/work/stale.xml')
        expect(store.invalidate()).toBe(2)
        pending.resolve({'acrn-config': {hv: {RELEASE: 'y'}, vm: []}})

        await expect(loading).resolves.toBeNull()
        expect(store.doc).toEqual(current)
        expect(store.dirty).toBe(true)
        expect(store.revision).toBe(2)
    })

    it('suppresses a load failure after the scenario revision changes', async () => {
        const store = useScenarioStore()
        const pending = deferred<Record<string, ScenarioDoc>>()
        vi.spyOn(configurator, 'loadScenario').mockReturnValue(pending.promise)
        const current = {hv: {RELEASE: 'n'}, vm: []}
        store.doc = current

        const loading = store.loadFromFile('/work/stale.xml')
        store.invalidate()
        pending.reject(new Error('stale read failed'))

        await expect(loading).resolves.toBeNull()
        expect(store.doc).toEqual(current)
        expect(store.revision).toBe(2)
    })

    it('discards a loaded document when its workspace is no longer selected', async () => {
        const store = useScenarioStore()
        vi.spyOn(configurator, 'loadScenario')
            .mockResolvedValue({'acrn-config': {hv: {}, vm: []}})
        const isSelected = vi.fn(() => false)

        await expect(store.loadFromFile('/work/other.xml', isSelected)).resolves.toBeNull()

        expect(isSelected).toHaveBeenCalledOnce()
        expect(store.doc).toBeNull()
        expect(store.dirty).toBe(false)
    })

    it('propagates a current load failure without replacing the document', async () => {
        const store = useScenarioStore()
        const failure = new Error('malformed XML')
        vi.spyOn(configurator, 'loadScenario').mockRejectedValue(failure)
        const current = {hv: {RELEASE: 'n'}, vm: []}
        store.doc = current

        await expect(store.loadFromFile('/work/broken.xml')).rejects.toBe(failure)

        expect(store.doc).toEqual(current)
        expect(store.revision).toBe(1)
    })
})

describe('scenario editing guards', () => {
    it('ignores form updates, update aliases, and renames without a document', () => {
        const store = useScenarioStore()

        store.updateActiveForm(-1, {RELEASE: 'y'})
        store.update(0, {name: 'replacement'})
        store.renameVm('VM0', 'VM1')

        expect(store.doc).toBeNull()
        expect(store.dirty).toBe(false)
        expect(store.revision).toBe(0)
    })

    it('ignores a form update for a VM ID that is not in the document', () => {
        const store = useScenarioStore()
        const existing = {'@id': 0, name: 'VM0', load_order: 'PRE_LAUNCHED_VM'}
        store.doc = {hv: {}, vm: [existing]}

        store.updateActiveForm(99, {'@id': 99, name: 'VM99', load_order: 'PRE_LAUNCHED_VM'})

        expect(store.doc.vm).toEqual([existing])
        expect(store.dirty).toBe(false)
        expect(store.revision).toBe(0)
    })

    it('renames references and normalizes non-RT vCPUs when a VM form changes', () => {
        const store = useScenarioStore()
        store.doc = {
            hv: {
                CACHE_REGION: {
                    CACHE_ALLOCATION: [{POLICY: [{VM: 'VM0'}, {VM: 'VM1'}]}]
                }
            },
            vm: [{'@id': 0, name: 'VM0', load_order: 'PRE_LAUNCHED_VM'}]
        }

        store.updateActiveForm(0, {
            '@id': 0,
            name: 'renamed',
            load_order: 'PRE_LAUNCHED_VM',
            vm_type: 'STANDARD_VM',
            cpu_affinity: {pcpu: [{pcpu_id: 0, real_time_vcpu: 'y'}]}
        })

        expect(store.doc.hv.CACHE_REGION.CACHE_ALLOCATION[0].POLICY)
            .toEqual([{VM: 'renamed'}, {VM: 'VM1'}])
        expect(store.doc.vm[0].cpu_affinity.pcpu[0].real_time_vcpu).toBe('n')
        expect(store.dirty).toBe(true)
        expect(store.revision).toBe(1)
    })

    it('returns -1 rather than adding a VM without a document', () => {
        const store = useScenarioStore()

        expect(store.addVm('POST_LAUNCHED_VM')).toBe(-1)
        expect(store.doc).toBeNull()
        expect(store.dirty).toBe(false)
        expect(store.revision).toBe(0)
    })

    it('handles missing documents and IDs and removes the active VM by default', () => {
        const store = useScenarioStore()
        expect(store.removeVm()).toEqual([])

        store.doc = {
            hv: {},
            vm: [{'@id': 1, name: 'VM1', load_order: 'PRE_LAUNCHED_VM'}]
        }
        store.activeVmId = 9
        expect(store.removeVm()).toEqual([])
        expect(store.doc.vm).toHaveLength(1)
        expect(store.dirty).toBe(false)
        expect(store.revision).toBe(0)

        store.activeVmId = 1
        expect(store.removeVm()).toEqual(['VM1'])
        expect(store.doc.vm).toEqual([])
        expect(store.activeVmId).toBe(-1)
        expect(store.dirty).toBe(true)
        expect(store.revision).toBe(1)
    })

    it('rejects saving when either the document or board is absent', async () => {
        const store = useScenarioStore()

        await expect(store.save('/work')).rejects.toThrow('No board or scenario has been loaded')

        store.doc = {hv: {}, vm: []}
        await expect(store.save('/work')).rejects.toThrow('No board or scenario has been loaded')
        expect(store.revision).toBe(0)
    })
})
