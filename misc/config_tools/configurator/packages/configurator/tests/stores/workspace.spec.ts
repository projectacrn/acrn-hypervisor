import {beforeEach, describe, expect, it, vi} from 'vitest'
import {createPinia, setActivePinia} from 'pinia'
import {setPlatform} from '../../src/platform'
import {createWebMockPlatform} from '../../src/platform/webMock'
import {useWorkspaceStore} from '../../src/stores/workspace'

function deferred<T>() {
    let resolve!: (value: T) => void
    const promise = new Promise<T>(resolvePromise => {
        resolve = resolvePromise
    })
    return {promise, resolve}
}

beforeEach(() => {
    setActivePinia(createPinia())
})

describe('workspace history', () => {
    it('loads history, delegates additions, and reloads the persisted ordering', async () => {
        const platform = createWebMockPlatform({history: {Scenario: ['/old.xml']}})
        setPlatform(platform)
        const add = vi.spyOn(platform.history, 'add')
        const store = useWorkspaceStore()

        await expect(store.loadHistory('Scenario')).resolves.toEqual(['/old.xml'])
        expect(store.scenarioHistory).toEqual(['/old.xml'])

        await store.addHistory('Scenario', '/new.xml')

        expect(add).toHaveBeenCalledWith('Scenario', '/new.xml')
        expect(store.scenarioHistory).toEqual(['/old.xml'])
        await expect(platform.history.get('Scenario')).resolves.toEqual(['/new.xml', '/old.xml'])

        await store.loadHistory('Scenario')
        expect(store.scenarioHistory).toEqual(['/new.xml', '/old.xml'])
    })

    it('keeps the current history when invalidation makes a pending load stale', async () => {
        const platform = createWebMockPlatform()
        setPlatform(platform)
        const pending = deferred<string[]>()
        const get = vi.spyOn(platform.history, 'get').mockReturnValue(pending.promise)
        const store = useWorkspaceStore()
        store.histories.Board = ['/current.xml']

        const loading = store.loadHistory('Board')
        store.invalidate('Board')
        pending.resolve(['/stale.xml'])

        await expect(loading).resolves.toBeNull()
        expect(get).toHaveBeenCalledWith('Board')
        expect(store.boardHistory).toEqual(['/current.xml'])
        expect(store.revisions.Board).toBe(2)
        expect(store.revisions.Scenario).toBe(0)
        expect(store.revisions.WorkingFolder).toBe(0)
    })
})
