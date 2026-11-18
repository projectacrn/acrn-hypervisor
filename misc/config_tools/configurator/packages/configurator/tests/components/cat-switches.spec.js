import {afterEach, describe, expect, it, vi} from 'vitest'
import {flushPromises, mount} from '@vue/test-utils'
import {createPinia, setActivePinia} from 'pinia'
import CAT from '../../src/pages/Config/ConfigForm/CustomWidget/CAT.vue'
import {useScenarioStore} from '../../src/stores/scenario'
import {BFormCheckbox} from 'bootstrap-vue-3'
import {nextTick} from 'vue'
import {confirm as confirmDialog} from '@tauri-apps/plugin-dialog'

vi.mock('@tauri-apps/plugin-dialog', () => ({confirm: vi.fn()}))

function mountCat(switches = {RDT_ENABLED: 'y', CDP_ENABLED: 'y', VCAT_ENABLED: 'n'}) {
    const pinia = createPinia()
    setActivePinia(pinia)
    const formData = {
        FEATURES: {RDT: {...switches}},
        CACHE_REGION: {CACHE_ALLOCATION: []},
    }
    const store = useScenarioStore()
    store.doc = {hv: formData, vm: []}
    store.board = {CAT_INFO: []}
    store.schemas = {}
    const wrapper = mount(CAT, {
        props: {
            rootFormData: formData,
            rootSchema: {
                definitions: {
                    RDTType: {
                        properties: {
                            RDT_ENABLED: {description: 'CAT'},
                            CDP_ENABLED: {description: 'CDP'},
                            VCAT_ENABLED: {description: 'VCAT'},
                        },
                    },
                },
            },
        },
        global: {
            plugins: [pinia],
            stubs: {
                'b-form-checkbox': BFormCheckbox,
                'b-button': true,
                'n-popover': {template: '<div><slot name="trigger"/><slot/></div>'},
                HexBlockRangeSelector: true,
                IconInfo: true,
            },
        },
    })
    return {formData, store, wrapper}
}

afterEach(() => {
    vi.mocked(confirmDialog).mockReset()
    vi.useRealTimers()
})

describe('CAT switch view synchronization', () => {
    it('unchecks CDP and CAT in the view after confirmed changes', async () => {
        vi.mocked(confirmDialog).mockResolvedValue(true)
        const {formData, store, wrapper} = mountCat()
        const checkboxes = () => wrapper.findAll('input[type="checkbox"]')
        expect(checkboxes()).toHaveLength(3)

        await checkboxes()[1].trigger('click')
        await flushPromises()

        expect(formData.FEATURES.RDT.CDP_ENABLED).toBe('n')
        expect(store.doc.hv.FEATURES.RDT.CDP_ENABLED).toBe('n')
        expect(checkboxes()[1].element.checked).toBe(false)
        expect(checkboxes()[0].element.checked).toBe(true)

        await checkboxes()[0].trigger('click')
        await flushPromises()

        expect(formData.FEATURES.RDT).toEqual({
            RDT_ENABLED: 'n', CDP_ENABLED: 'n', VCAT_ENABLED: 'n',
        })
        expect(checkboxes().map(input => input.element.checked)).toEqual([false, false, false])
    })

    it('refreshes the derived view after a Scenario mask edit', async () => {
        const {store, wrapper} = mountCat()
        const previousView = wrapper.vm.CAT_INFO

        store.setClosMask(
            {id: 0, level: 3},
            {VM: 'VM0', VCPU: 0, TYPE: 'Unified'},
            '0x1'
        )
        await nextTick()

        expect(store.revision).toBe(1)
        expect(wrapper.vm.CAT_INFO).not.toBe(previousView)
        expect(store.doc.hv.CACHE_REGION.CACHE_ALLOCATION[0].POLICY[0].CLOS_MASK).toBe('0x1')
    })

    it('does not write a delayed enable after its widget is unmounted', async () => {
        vi.useFakeTimers()
        const {formData, wrapper} = mountCat({RDT_ENABLED: 'n', CDP_ENABLED: 'n', VCAT_ENABLED: 'n'})
        const cat = wrapper.findAll('input[type="checkbox"]')[0]

        await cat.trigger('click')
        wrapper.unmount()
        await vi.advanceTimersByTimeAsync(50)

        expect(formData.FEATURES.RDT.RDT_ENABLED).toBe('n')
    })

    it('does not write a stale confirmation after its widget is unmounted', async () => {
        let resolveConfirmation
        vi.mocked(confirmDialog).mockImplementation(() => new Promise(resolve => {
            resolveConfirmation = resolve
        }))
        const {formData, wrapper} = mountCat()
        const cdp = wrapper.findAll('input[type="checkbox"]')[1]

        await cdp.trigger('click')
        wrapper.unmount()
        resolveConfirmation(true)
        await flushPromises()

        expect(formData.FEATURES.RDT.CDP_ENABLED).toBe('y')
    })
})
