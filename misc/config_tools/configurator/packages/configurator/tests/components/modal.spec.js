import {mount} from '@vue/test-utils'
import {describe, expect, it} from 'vitest'
import SaveScenario from '../../src/pages/Config/Scenario/SaveScenario.vue'

const BModalStub = {
    props: {modelValue: {type: Boolean, required: true}},
    emits: ['update:modelValue'],
    template: `
        <div v-if="modelValue">
            <button type="button" @click="$emit('update:modelValue', false)">OK</button>
            <slot/>
        </div>
    `
}

describe('Scenario save result', () => {
    it('forwards modal visibility changes without mutating the prop', async () => {
        const wrapper = mount(SaveScenario, {
            props: {showModal: true, totalMsg: 'All files successfully saved'},
            global: {stubs: {'b-modal': BModalStub}}
        })

        await wrapper.find('button').trigger('click')

        expect(wrapper.emitted('update:showModal')).toEqual([[false]])
        expect(wrapper.props('showModal')).toBe(true)
    })

    it('renders validation messages as text instead of injected HTML', () => {
        const message = 'Multiple VMs have the same name: <img src=x onerror=alert(1)>'
        const wrapper = mount(SaveScenario, {
            props: {showModal: true, totalMsg: message},
            global: {stubs: {'b-modal': BModalStub}}
        })

        expect(wrapper.find('p').text()).toBe(message)
        expect(wrapper.find('p img').exists()).toBe(false)
    })
})
