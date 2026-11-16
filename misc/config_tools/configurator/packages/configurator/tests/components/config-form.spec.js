import {flushPromises, mount} from '@vue/test-utils'
import {describe, expect, it} from 'vitest'
import ConfigForm from '../../src/pages/Config/ConfigForm.vue'

const VueFormStub = {
    props: {schema: {type: Object, required: true}},
    template: `
        <div class="stub-form">
            <label v-for="(field, key) in schema.properties" :key="key">{{ field.title }}</label>
        </div>
    `
}

describe('ConfigForm parameter modes', () => {
    it('recreates the keyed form when changing parameter modes', async () => {
        let unmountCount = 0
        let wrapper
        const TrackingVueForm = {
            ...VueFormStub,
            beforeUnmount() {
                unmountCount += 1
            }
        }
        wrapper = mount(ConfigForm, {
            props: {
                currentActiveVMID: -1,
                currentFormSchema: {
                    BasicConfigType: {type: 'object', properties: {}},
                    AdvancedConfigType: {type: 'object', properties: {}}
                },
                currentFormData: {}
            },
            global: {
                stubs: {VueForm: TrackingVueForm, 'b-button': true}
            }
        })
        await flushPromises()

        await wrapper.find('.aTab').trigger('click')
        await flushPromises()
        expect(unmountCount).toBe(1)

        await wrapper.find('.bTab').trigger('click')
        await flushPromises()
        expect(unmountCount).toBe(2)
    })

    it('renders Advanced fields and restores Basic fields after switching back', async () => {
        const currentFormSchema = {
            BasicConfigType: {
                type: 'object',
                properties: {
                    basic_setting: {type: 'string', title: 'Basic-only setting'}
                }
            },
            AdvancedConfigType: {
                type: 'object',
                properties: {
                    advanced_setting: {type: 'string', title: 'Advanced-only setting'}
                }
            }
        }
        const wrapper = mount(ConfigForm, {
            props: {
                currentActiveVMID: -1,
                currentFormSchema,
                currentFormData: {basic_setting: '', advanced_setting: ''}
            },
            global: {
                stubs: {VueForm: VueFormStub, 'b-button': true}
            }
        })
        await flushPromises()
        expect(wrapper.text()).toContain('Basic-only setting')
        expect(wrapper.text()).not.toContain('Advanced-only setting')

        await wrapper.find('.aTab').trigger('click')
        await flushPromises()
        expect(wrapper.text()).toContain('Advanced-only setting')
        expect(wrapper.text()).not.toContain('Basic-only setting')

        await wrapper.find('.bTab').trigger('click')
        await flushPromises()
        expect(wrapper.text()).toContain('Basic-only setting')
        expect(wrapper.text()).not.toContain('Advanced-only setting')
    })
})
