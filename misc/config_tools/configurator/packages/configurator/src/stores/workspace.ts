import {defineStore} from 'pinia'
import {getPlatform} from '../platform'
import type {HistoryType} from '../platform/types'

type TrackedHistoryType = Extract<HistoryType, 'WorkingFolder' | 'Board' | 'Scenario'>
type HistoryState = Record<TrackedHistoryType, string[]>

export const useWorkspaceStore = defineStore('workspace', {
    state: () => ({
        histories: {
            WorkingFolder: [],
            Board: [],
            Scenario: [],
        } as HistoryState,
        revisions: {
            WorkingFolder: 0,
            Board: 0,
            Scenario: 0,
        } as Record<TrackedHistoryType, number>,
    }),
    getters: {
        boardHistory: (state): string[] => state.histories.Board,
        scenarioHistory: (state): string[] => state.histories.Scenario,
        workingFolderHistory: (state): string[] => state.histories.WorkingFolder,
    },
    actions: {
        async loadHistory(type: TrackedHistoryType): Promise<string[] | null> {
            const revision = ++this.revisions[type]
            const history = await getPlatform().history.get(type)
            if (revision !== this.revisions[type]) {
                return null
            }
            this.histories[type] = history
            return history
        },
        async addHistory(type: TrackedHistoryType, path: string): Promise<void> {
            await getPlatform().history.add(type, path)
        },
        invalidate(type: TrackedHistoryType): void {
            this.revisions[type]++
        },
    },
})
