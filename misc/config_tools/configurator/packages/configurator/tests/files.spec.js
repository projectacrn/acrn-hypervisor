import {afterEach, describe, expect, it, vi} from 'vitest'
import Scenario from '../src/pages/Config/Scenario.vue'
import Board from '../src/pages/Config/Board.vue'
import NewConfiguration from '../src/pages/Welcome/NewConfiguration.vue'
import configurator from '../src/lib/acrn'
import {createPinia, setActivePinia} from 'pinia'
import {useScenarioStore} from '../src/stores/scenario'
import {useWorkspaceStore} from '../src/stores/workspace'
import {getPlatform} from '../src/platform'

afterEach(() => {
    vi.restoreAllMocks()
    vi.unstubAllGlobals()
})

describe('file loading and cleanup', () => {
    it('ignores a stale scenario response after a newer selection loads', async () => {
        const pending = {}
        vi.spyOn(configurator, 'loadScenario').mockImplementation(path => {
            return new Promise(resolve => { pending[path] = resolve })
        })
        setActivePinia(createPinia())
        const context = {
            currentSelectedScenario: 'a.xml',
            scenarioStore: useScenarioStore(),
            $emit: vi.fn()
        }

        const first = Scenario.methods.loadScenario.call(context, true)
        context.currentSelectedScenario = 'b.xml'
        const second = Scenario.methods.loadScenario.call(context, true)
        pending['b.xml']({'acrn-config': {vm: [{name: 'new'}]}})
        await second
        pending['a.xml']({'acrn-config': {vm: [{name: 'old'}]}})
        await first

        expect(context.$emit).toHaveBeenCalledTimes(1)
        expect(context.$emit).toHaveBeenCalledWith('scenarioUpdate', {vm: [{name: 'new'}]})
    })

    it('keeps the latest Scenario history selection when an older request finishes last', async () => {
        const pending = []
        vi.spyOn(getPlatform().history, 'get').mockImplementation(() =>
            new Promise(resolve => pending.push(resolve)))
        setActivePinia(createPinia())
        const context = {workspaceStore: useWorkspaceStore(), currentSelectedScenario: ''}

        const first = Scenario.methods.getScenarioHistory.call(context)
        const second = Scenario.methods.getScenarioHistory.call(context)
        pending[1](['new.xml'])
        await second
        pending[0](['old.xml'])
        await first

        expect(context.workspaceStore.scenarioHistory).toEqual(['new.xml'])
        expect(context.currentSelectedScenario).toBe('new.xml')
    })

    it('keeps the latest Board history selection when an older request finishes last', async () => {
        const pending = []
        vi.spyOn(getPlatform().history, 'get').mockImplementation(() =>
            new Promise(resolve => pending.push(resolve)))
        setActivePinia(createPinia())
        const context = {workspaceStore: useWorkspaceStore(), currentSelectedBoard: ''}

        const first = Board.methods.getBoardHistory.call(context)
        const second = Board.methods.getBoardHistory.call(context)
        pending[1](['new.board.xml'])
        await second
        pending[0](['old.board.xml'])
        await first

        expect(context.workspaceStore.boardHistory).toEqual(['new.board.xml'])
        expect(context.currentSelectedBoard).toBe('new.board.xml')
    })

    it('does not emit a board update when its durable copy fails', async () => {
        vi.stubGlobal('alert', vi.fn())
        vi.spyOn(configurator, 'loadBoard').mockResolvedValue({
            scenarioJSONSchema: {},
            boardInfo: {name: 'new.board.xml', content: '<board/>'}
        })
        vi.spyOn(configurator, 'writeFile').mockRejectedValue(new Error('disk full'))
        const removeFile = vi.spyOn(configurator, 'removeFile')
        const context = {
            importing: false,
            currentSelectedBoard: '/work/old.board.xml',
            newFilePath: '',
            WorkingFolder: '/work/',
            getBoardHistory: vi.fn(),
            $emit: vi.fn()
        }

        await Board.methods.importBoard.call(context)

        expect(context.$emit).not.toHaveBeenCalled()
        expect(removeFile).not.toHaveBeenCalled()
        expect(context.importing).toBe(false)
    })

    it('does not delete an external source board after importing a replacement', async () => {
        vi.spyOn(configurator, 'loadBoard').mockResolvedValue({
            scenarioJSONSchema: {},
            boardInfo: {name: 'new.board.xml', content: '<board/>'}
        })
        vi.spyOn(configurator, 'writeFile').mockResolvedValue()
        vi.spyOn(configurator, 'addHistory').mockResolvedValue()
        const removeFile = vi.spyOn(configurator, 'removeFile')
        const context = {
            importing: false,
            currentSelectedBoard: '/external/old.board.xml',
            newFilePath: '/external/new.board.xml',
            WorkingFolder: '/work/',
            getBoardHistory: vi.fn().mockResolvedValue(),
            $emit: vi.fn()
        }

        await Board.methods.importBoard.call(context, true)

        expect(removeFile).not.toHaveBeenCalled()
        expect(context.$emit).toHaveBeenCalledWith('boardUpdate', {
            name: 'new.board.xml', content: '<board/>'
        }, {})
    })

    it('loads an imported board even when history persistence is unavailable', async () => {
        vi.spyOn(configurator, 'loadBoard').mockResolvedValue({
            scenarioJSONSchema: {},
            boardInfo: {name: 'new.board.xml', content: '<board/>'}
        })
        vi.spyOn(configurator, 'writeFile').mockResolvedValue()
        vi.spyOn(configurator, 'addHistory').mockRejectedValue(new Error('damaged config'))
        vi.spyOn(console, 'warn').mockImplementation(() => {})
        const context = {
            importing: false,
            currentSelectedBoard: '/work/old.board.xml',
            newFilePath: '',
            WorkingFolder: '/work/',
            getBoardHistory: vi.fn(),
            $emit: vi.fn()
        }

        await Board.methods.importBoard.call(context)

        expect(context.$emit).toHaveBeenCalledWith('boardUpdate', {
            name: 'new.board.xml', content: '<board/>'
        }, {})
        expect(context.importing).toBe(false)
    })

    it('does not continue to a new configuration when cleanup fails', async () => {
        vi.stubGlobal('confirm', vi.fn().mockResolvedValue(true))
        vi.stubGlobal('alert', vi.fn())
        vi.stubGlobal('systemInfo', {homeDir: '/home/test', pathSplit: '/'})
        vi.spyOn(configurator, 'readDir').mockResolvedValue([{path: '/work/scenario.xml'}])
        vi.spyOn(configurator, 'removeFile').mockRejectedValue(new Error('permission denied'))
        const createDir = vi.spyOn(configurator, 'creatDir')
        const context = {
            WorkingFolder: '/work/',
            defaultWorkingFolder: '/work/',
            nextPage: vi.fn()
        }
        context.cleanOldFiles = NewConfiguration.methods.cleanOldFiles.bind(context)

        await NewConfiguration.methods.usingWorkingFolder.call(context)

        expect(context.nextPage).not.toHaveBeenCalled()
        expect(createDir).not.toHaveBeenCalled()
    })
})
