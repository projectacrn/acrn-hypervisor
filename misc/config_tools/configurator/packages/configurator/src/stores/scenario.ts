import {defineStore} from 'pinia'
import configurator from '../lib/acrn'
import {runSavePipeline} from '../domain/artifacts/pipeline'
import type {SaveReport} from '../domain/artifacts/pipeline'
import {correctSwitches, deriveCatView} from '../domain/cat/derive'
import type {CatRegionView, CatView, Policy} from '../domain/cat/model'
import {buildCacheRegion, setClosMask as updateClosMask} from '../domain/cat/toScenario'
import {removeVm as removeVmReferences, renameVm as renameVmReferences} from '../domain/references/rename'
import {
    deriveActivePciSchema,
    deriveLoadedPciSchema,
    selectFormSchema,
    usedPciDevices,
} from '../domain/schema/derive'
import type {BoardInfo, ScenarioDoc} from '../engine/types'
import {useValidationStore} from './validation'

export type VmId = number
export type FormSchemas = Record<string, any>
export type VmsByLoadOrder = Record<'PRE_LAUNCHED_VM' | 'SERVICE_VM' | 'POST_LAUNCHED_VM', ScenarioDoc[]>

function emptyVmGroups(): VmsByLoadOrder {
    return {
        PRE_LAUNCHED_VM: [],
        SERVICE_VM: [],
        POST_LAUNCHED_VM: [],
    }
}

export const useScenarioStore = defineStore('scenario', {
    state: () => ({
        board: null as BoardInfo | null,
        schemas: null as FormSchemas | null,
        doc: null as ScenarioDoc | null,
        activeVmId: -1 as VmId,
        dirty: false,
        revision: 0,
    }),
    getters: {
        activeVm(state): ScenarioDoc | null {
            return state.doc?.vm?.find((vm: ScenarioDoc) => vm['@id'] === state.activeVmId) ?? null
        },
        activeSchema(state): FormSchemas | null {
            if (!state.schemas) {
                return null
            }
            return selectFormSchema(state.schemas, state.activeVmId, state.doc?.vm || []) ?? null
        },
        activeFormData(state): ScenarioDoc {
            if (!state.doc?.hv || !Array.isArray(state.doc.vm)) {
                return {}
            }
            if (state.activeVmId === -1) {
                return state.doc.hv
            }
            return state.doc.vm.find((vm: ScenarioDoc) => vm['@id'] === state.activeVmId) || {}
        },
        vmsByLoadOrder(state): VmsByLoadOrder {
            const groups = emptyVmGroups()
            state.doc?.vm?.forEach((vm: ScenarioDoc) => {
                if (vm.load_order in groups) {
                    groups[vm.load_order as keyof VmsByLoadOrder].push(vm)
                }
            })
            return groups
        },
        vmNames(state): string[] {
            return state.doc?.vm?.map((vm: ScenarioDoc) => vm.name) || []
        },
    },
    actions: {
        invalidate(): number {
            return ++this.revision
        },
        reset(): void {
            this.board = null
            this.schemas = null
            this.doc = null
            this.activeVmId = -1
            this.dirty = false
            this.revision++
        },
        correctCatSwitches(): void {
            if (!this.doc?.hv) {
                return
            }
            const {corrections} = correctSwitches(this.doc)
            if (!this.doc.hv.FEATURES) {
                this.doc.hv.FEATURES = {}
            }
            if (!this.doc.hv.FEATURES.RDT) {
                this.doc.hv.FEATURES.RDT = {}
            }
            Object.assign(this.doc.hv.FEATURES.RDT, corrections)
        },
        getCatView(): CatView | null {
            if (!this.doc?.hv || !this.board || !this.schemas) {
                return null
            }
            if (this.board.CAT_INFO.length !== 0) {
                this.correctCatSwitches()
            }
            return deriveCatView({
                doc: this.doc,
                board: this.board,
                schema: this.schemas,
            })
        },
        setClosMask(region: CatRegionView, policy: Policy, closMask: string): void {
            if (!this.doc?.hv) {
                return
            }
            this.doc.hv.CACHE_REGION = updateClosMask(this.doc.hv.CACHE_REGION, {
                cacheId: region.id,
                cacheLevel: region.level,
                vm: policy.VM,
                vcpu: policy.VCPU,
                type: policy.TYPE,
                closMask,
            })
            this.dirty = true
            this.revision++
        },
        loadBoard(board: BoardInfo, schemas: FormSchemas): void {
            this.board = board
            this.schemas = schemas
            this.activeVmId = -1
            this.revision++
        },
        load(doc: ScenarioDoc): void {
            this.doc = doc
            this.activeVmId = -1
            this.dirty = false
            this.revision++
        },
        async loadFromFile(path: string, isSelected: () => boolean = () => true): Promise<ScenarioDoc | null> {
            const revision = ++this.revision
            let result: Record<string, ScenarioDoc>
            try {
                result = await configurator.loadScenario(path) as Record<string, ScenarioDoc>
            } catch (error) {
                if (revision !== this.revision || !isSelected()) {
                    return null
                }
                throw error
            }
            if (revision !== this.revision || !isSelected()) {
                return null
            }
            this.load(result['acrn-config'])
            await this.validateCurrent()
            return this.doc
        },
        async importScenario(doc: ScenarioDoc): Promise<void> {
            this.load(doc)
            await this.validateCurrent()
        },
        updateLoadSchema(): void {
            if (this.activeVmId === -1 || !this.doc || !this.board || !this.schemas) {
                return
            }
            const current = configurator.pythonObject.updateSchema(
                this.board.content, usedPciDevices(this.doc.vm))
            this.schemas = deriveLoadedPciSchema(this.schemas, JSON.parse(current))
        },
        updateActivePciSchema(): void {
            if (!this.schemas || !this.activeVm || this.activeVm.load_order !== 'PRE_LAUNCHED_VM') {
                return
            }
            const selectedDevices = this.activeVm.pci_devs?.pci_dev || []
            this.schemas = deriveActivePciSchema(this.schemas, selectedDevices)
        },
        setActiveVm(vmId: VmId): void {
            this.activeVmId = vmId
            this.updateLoadSchema()
            this.updateActivePciSchema()
        },
        updateActiveForm(vmId: VmId, data: ScenarioDoc): void {
            if (!this.doc) {
                return
            }
            if (vmId === -1) {
                this.doc.hv = data
            } else {
                const index = this.doc.vm.findIndex((vm: ScenarioDoc) => vm['@id'] === vmId)
                if (index === -1) {
                    return
                }
                const oldName = this.doc.vm[index].name
                this.doc.vm.splice(index, 1, data)
                if (oldName !== data.name) {
                    if (data.name === '') {
                        removeVmReferences(this.doc, oldName)
                    } else {
                        renameVmReferences(this.doc, oldName, data.name)
                    }
                }
                if (data.vm_type !== 'RTVM') {
                    data.cpu_affinity?.pcpu?.forEach((pcpu: ScenarioDoc) => {
                        pcpu.real_time_vcpu = 'n'
                    })
                }
            }
            this.updateActivePciSchema()
            this.dirty = true
            this.revision++
        },
        update(vmId: VmId, data: ScenarioDoc): void {
            this.updateActiveForm(vmId, data)
        },
        renameVm(oldName: string, newName: string): void {
            if (!this.doc) {
                return
            }
            if (newName === '') {
                removeVmReferences(this.doc, oldName)
            } else {
                renameVmReferences(this.doc, oldName, newName)
            }
            this.dirty = true
            this.revision++
        },
        addVm(loadOrder: string): VmId {
            if (!this.doc) {
                return -1
            }
            let maxVmId = -1
            let haveService = false
            this.doc.vm.forEach((vm: ScenarioDoc) => {
                const vmId = vm['@id']
                if (vm.name && /^VM(\d+)$/.test(vm.name)) {
                    maxVmId = Math.max(maxVmId, Number.parseInt(vm.name.replace(/\D/g, ' '), 10))
                }
                if (vmId > maxVmId) {
                    maxVmId = vmId
                }
                if (vm.load_order === 'SERVICE_VM') {
                    haveService = true
                }
            })
            if (loadOrder === 'POST_LAUNCHED_VM' && !haveService) {
                maxVmId++
                this.doc.vm.push(configurator.newVM(maxVmId, 'SERVICE_VM'))
            }
            maxVmId++
            this.doc.vm.push(configurator.newVM(maxVmId, loadOrder))
            this.dirty = true
            this.revision++
            this.setActiveVm(maxVmId)
            return maxVmId
        },
        removeVm(vmId: VmId = this.activeVmId): string[] {
            if (!this.doc) {
                return []
            }
            const selected = this.doc.vm.find((vm: ScenarioDoc) => vm['@id'] === vmId)
            if (!selected) {
                return []
            }
            const removed = this.doc.vm.filter((vm: ScenarioDoc) =>
                vm['@id'] === vmId ||
                (selected.load_order === 'SERVICE_VM' && vm.load_order === 'POST_LAUNCHED_VM'))
            removed.forEach((vm: ScenarioDoc) => removeVmReferences(this.doc!, vm.name))
            const removedIds = new Set(removed.map((vm: ScenarioDoc) => vm['@id']))
            this.doc.vm = this.doc.vm.filter((vm: ScenarioDoc) => !removedIds.has(vm['@id']))
            this.activeVmId = -1
            this.dirty = true
            this.revision++
            return removed.map((vm: ScenarioDoc) => vm.name)
        },
        async validateCurrent() {
            if (!this.doc || !this.board) {
                return null
            }
            const revision = ++this.revision
            const doc = this.doc
            const scenarioXml = configurator.convertScenarioToXML({
                'acrn-config': JSON.parse(JSON.stringify(doc)),
            })
            return useValidationStore().validate({
                boardXml: this.board.content,
                scenarioXml,
                doc,
                isCurrent: () => revision === this.revision,
                validateScenario: (boardXml, xml) =>
                    configurator.pythonObject.validateScenario(boardXml, xml),
            })
        },
        async save(workingFolder: string, vmNameError: string | false = false): Promise<SaveReport> {
            if (!this.doc || !this.board) {
                throw new Error('No board or scenario has been loaded')
            }
            const revision = ++this.revision
            const validation = useValidationStore()
            let saveDocument: ScenarioDoc | null = null
            const report = await runSavePipeline({
                scenario: this.doc,
                activeVmId: this.activeVmId,
                boardXml: this.board.content,
                workingFolder,
                vmNameError,
            }, {
                scenarioToXml: scenario => configurator.convertScenarioToXML({
                    'acrn-config': JSON.parse(JSON.stringify(scenario)),
                }),
                populateDefaults: xml => configurator.pythonObject.populateDefaultValues(xml),
                getCacheRegion: () => buildCacheRegion(this.getCatView()),
                writeFile: (path, contents) => configurator.writeFile(path, contents),
                validateScenario: (boardXml, scenarioXml) =>
                    configurator.pythonObject.validateScenario(boardXml, scenarioXml),
                generateConfigSummary: (boardXml, scenarioXml) =>
                    configurator.pythonObject.generateConfigSummary(boardXml, scenarioXml),
                generateLaunchScript: (boardXml, scenarioXml) =>
                    configurator.pythonObject.generateLaunchScript(boardXml, scenarioXml),
                listFiles: (path, recursive) => configurator.readDir(path, recursive),
                removeFile: path => configurator.removeFile(path),
                onVmIdsAssigned: activeVmId => {
                    this.activeVmId = activeVmId
                },
                onScenarioPopulated: scenario => {
                    this.doc = scenario
                    saveDocument = this.doc
                },
                onValidated: (errors, scenario) => {
                    // Form refreshes keep the populated document identity, but
                    // a newly loaded workspace must not receive stale errors.
                    if (saveDocument !== null && saveDocument === this.doc) {
                        validation.apply(errors, scenario)
                    }
                },
            })
            if (revision === this.revision) {
                this.dirty = !report.success
            }
            return report
        },
    },
})
