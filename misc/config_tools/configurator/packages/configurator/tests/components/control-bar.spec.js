import {beforeEach, describe, expect, it, vi} from 'vitest'
import {flushPromises, mount} from '@vue/test-utils'

const appWindow = vi.hoisted(() => ({
    minimize: vi.fn(),
    maximize: vi.fn(),
    unmaximize: vi.fn(),
    close: vi.fn(),
    isMaximized: vi.fn(),
}))

vi.mock('@tauri-apps/api/webviewWindow', () => ({
    getCurrentWebviewWindow: () => appWindow,
}))
vi.mock('@tauri-apps/api/core', () => ({
    invoke: vi.fn(),
}))

import {invoke} from '@tauri-apps/api/core'
import ControlBar from '../../src/components/common/ControlBar.vue'

describe('window control bar', () => {
    beforeEach(() => {
        vi.clearAllMocks()
    })

    it('minimizes and closes the native window', () => {
        const wrapper = mount(ControlBar)

        wrapper.vm.minus()
        wrapper.vm.close()

        expect(appWindow.minimize).toHaveBeenCalledTimes(1)
        expect(appWindow.close).toHaveBeenCalledTimes(1)
    })

    it('maximizes when the window is not maximized yet', async () => {
        appWindow.isMaximized.mockResolvedValue(false)
        const wrapper = mount(ControlBar)

        wrapper.vm.maximize()
        await flushPromises()

        expect(appWindow.maximize).toHaveBeenCalledTimes(1)
        expect(appWindow.unmaximize).not.toHaveBeenCalled()
    })

    it('restores the window when it is already maximized', async () => {
        appWindow.isMaximized.mockResolvedValue(true)
        const wrapper = mount(ControlBar)

        wrapper.vm.maximize()
        await flushPromises()

        expect(appWindow.unmaximize).toHaveBeenCalledTimes(1)
        expect(appWindow.maximize).not.toHaveBeenCalled()
    })

    it('asks the backend to open developer tools', () => {
        const wrapper = mount(ControlBar)

        wrapper.vm.openDevtools()

        expect(invoke).toHaveBeenCalledWith('open_devtools', {})
    })
})
