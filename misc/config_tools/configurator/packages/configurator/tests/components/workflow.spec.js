import {afterEach, describe, expect, it, vi} from 'vitest'
import {flushPromises, mount, shallowMount} from '@vue/test-utils'
import Board from '../../src/pages/Config/Board.vue'
import NewConfiguration from '../../src/pages/Welcome/NewConfiguration.vue'
import UseExisting from '../../src/pages/Welcome/UseExisting.vue'
import configurator from '../../src/lib/acrn'

afterEach(() => {
    vi.restoreAllMocks()
    vi.unstubAllGlobals()
})

describe('configuration workflow components', () => {
    it('imports the selected board from the rendered component', async () => {
        vi.stubGlobal('alert', vi.fn())
        vi.spyOn(configurator, 'readDir').mockResolvedValue([])
        vi.spyOn(configurator, 'getHistory').mockResolvedValue(['/fixtures/generic_board.xml'])
        vi.spyOn(configurator, 'loadBoard').mockResolvedValue({
            scenarioJSONSchema: {HV: {}},
            boardInfo: {
                name: 'generic_board.board.xml',
                content: '<board/>',
                BIOS_INFO: 'BIOS Information',
                BASE_BOARD_INFO: 'Base Board Information'
            }
        })
        const writeFile = vi.spyOn(configurator, 'writeFile').mockResolvedValue()
        vi.spyOn(configurator, 'addHistory').mockResolvedValue()

        const wrapper = shallowMount(Board, {
            props: {WorkingFolder: '/work/', board: {}, schemas: {}},
            global: {stubs: {NewBoard: true}}
        })
        await flushPromises()

        expect(wrapper.get('select').element.value).toBe('/fixtures/generic_board.xml')
        await wrapper.get('button').trigger('click')
        await flushPromises()

        expect(writeFile).toHaveBeenCalledWith('/work/generic_board.board.xml', '<board/>')
        expect(wrapper.emitted('boardUpdate')).toHaveLength(1)
        expect(wrapper.emitted('boardUpdate')[0][0].name).toBe('generic_board.board.xml')
    })

    it('opens a new folder with the expected route state', async () => {
        vi.stubGlobal('systemInfo', {homeDir: '/home/test', pathSplit: '/'})
        vi.spyOn(configurator, 'readDir').mockRejectedValue(new Error('missing directory'))
        const createDir = vi.spyOn(configurator, 'creatDir').mockResolvedValue()
        const push = vi.fn()
        const wrapper = mount(NewConfiguration, {
            global: {mocks: {$router: {push}}}
        })

        await wrapper.get('input').setValue('/tmp/acrn component')
        await wrapper.get('button').trigger('click')
        await flushPromises()

        expect(createDir).toHaveBeenCalledWith('/tmp/acrn component/')
        expect(push).toHaveBeenCalledWith({
            name: 'Config',
            params: {WorkingFolder: '/tmp/acrn component/'},
            query: {isNewConfig: 'true'}
        })
    })

    it('opens an existing folder selected from history', async () => {
        vi.spyOn(configurator, 'getHistory').mockResolvedValue(['/work/existing/'])
        const push = vi.fn()
        const wrapper = mount(UseExisting, {
            global: {mocks: {$router: {push}}}
        })
        await flushPromises()

        expect(wrapper.get('select').element.value).toBe('/work/existing/')
        await wrapper.get('button').trigger('click')

        expect(push).toHaveBeenCalledWith({
            name: 'Config',
            params: {WorkingFolder: '/work/existing/'},
            query: {isNewConfig: 'false'}
        })
    })
})
