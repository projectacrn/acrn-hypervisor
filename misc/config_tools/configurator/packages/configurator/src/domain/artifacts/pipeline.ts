type ScenarioDoc = Record<string, any>
type MaybePromise<T> = T | Promise<T>

export type SaveStepId =
    | 'assignVmIds'
    | 'populateDefaults'
    | 'writeScenario'
    | 'validate'
    | 'configSummary'
    | 'launchScripts'
    | 'pruneStaleScripts'

export interface StepReport {
    id: SaveStepId
    status: 'ok' | 'failed' | 'skipped'
    detail?: string
}

export interface SaveReport {
    steps: StepReport[]
    scenario: ScenarioDoc
    scenarioXml: string
    activeVmId: number
    needSaveLaunchScript: boolean
    launchScripts: Record<string, string>
    validationErrors: LegacyValidation | null
    error?: unknown
    success: boolean
}

export interface LegacyValidation {
    syntactic_errors: any[]
    semantic_errors: any[]
    [key: string]: any
}

export interface SavePipelineInput {
    scenario: ScenarioDoc
    activeVmId: number
    boardXml: string
    workingFolder: string
    vmNameError?: string | false
}

export interface SavePipelineDependencies {
    scenarioToXml(scenario: ScenarioDoc): string
    populateDefaults(scenarioXml: string): MaybePromise<any>
    getCacheRegion?(): any
    writeFile(path: string, contents: string): Promise<void>
    validateScenario(boardXml: string, scenarioXml: string): LegacyValidation
    generateConfigSummary(boardXml: string, scenarioXml: string): string
    generateLaunchScript(boardXml: string, scenarioXml: string): Record<string, string>
    listFiles(path: string, recursive: boolean): Promise<Array<{path: string}>>
    removeFile(path: string): Promise<void>
    onVmIdsAssigned?(activeVmId: number): void
    onScenarioPopulated?(scenario: ScenarioDoc): void
    onScenarioWritten?(scenario: ScenarioDoc): void
    onValidated?(errors: LegacyValidation, scenario: ScenarioDoc): void
}

export interface LaunchScriptCleanupDependencies {
    listFiles(path: string, recursive: boolean): Promise<Array<{path: string}>>
    removeFile(path: string): Promise<void>
}

const SAVE_STEP_IDS: SaveStepId[] = [
    'assignVmIds',
    'populateDefaults',
    'writeScenario',
    'validate',
    'configSummary',
    'launchScripts',
    'pruneStaleScripts'
]

function artifactPath(workingFolder: string, filename: string): string {
    if (/[\\/]$/.test(workingFolder)) {
        return workingFolder + filename
    }
    return workingFolder + (workingFolder.includes('\\') ? '\\' : '/') + filename
}

function errorDetail(error: unknown): string {
    return error instanceof Error ? error.message : String(error)
}

function setStep(report: SaveReport, id: SaveStepId, status: StepReport['status'], detail?: string): void {
    const step = report.steps.find(candidate => candidate.id === id)
    if (step) {
        step.status = status
        step.detail = detail
    }
}

/** Reassign IDs in load-order priority while keeping the active object selected. */
export function assignVmIds(scenario: ScenarioDoc, activeVmId: number): number {
    const vmPriority: Record<string, number> = {
        PRE_LAUNCHED_VM: 0,
        SERVICE_VM: 1,
        POST_LAUNCHED_VM: 2
    }
    const activeVm = scenario.vm.find((vm: ScenarioDoc) => vm['@id'] === activeVmId)
    scenario.vm.sort((a: ScenarioDoc, b: ScenarioDoc) =>
        vmPriority[a.load_order] - vmPriority[b.load_order]
    ).map((vm: ScenarioDoc, index: number) => {
        vm['@id'] = index
    })
    return activeVm ? activeVm['@id'] : activeVmId
}

/** Populate defaults and preserve the existing RDT/CACHE_REGION normalization. */
export async function populateScenarioDefaults(
    scenario: ScenarioDoc,
    scenarioToXml: (scenario: ScenarioDoc) => string,
    populateDefaults: (scenarioXml: string) => MaybePromise<any>
): Promise<ScenarioDoc> {
    const scenarioXml = scenarioToXml(scenario)
    const result = await populateDefaults(scenarioXml)
    const scenarioWithDefaults = result.json['acrn-config']
    if (scenarioWithDefaults.hv.FEATURES.RDT.RDT_ENABLED === 'n') {
        delete scenarioWithDefaults.hv.CACHE_REGION
    }
    return scenarioWithDefaults
}

/** Delete every old launch script not produced by the current save. */
export async function pruneStaleLaunchScripts(
    workingFolder: string,
    keep: Set<string>,
    dependencies: LaunchScriptCleanupDependencies
): Promise<void> {
    const files = await dependencies.listFiles(workingFolder, false)
    const removals = files
        .filter(file => {
            const basename = file.path.split(/[\\/]/).pop()
            return /^launch.*\.sh$/.test(basename || '') && !keep.has(basename || '')
        })
        .map(file => dependencies.removeFile(file.path))
    await Promise.all(removals)
}

/**
 * Execute the legacy save flow in its original order. Failures are reported at
 * the step that stopped the pipeline; later steps remain skipped.
 */
export async function runSavePipeline(
    input: SavePipelineInput,
    dependencies: SavePipelineDependencies
): Promise<SaveReport> {
    const report: SaveReport = {
        steps: SAVE_STEP_IDS.map(id => ({id, status: 'skipped'} as StepReport)),
        scenario: input.scenario,
        scenarioXml: '',
        activeVmId: input.activeVmId,
        needSaveLaunchScript: false,
        launchScripts: {},
        validationErrors: null,
        success: false
    }
    let currentStep: SaveStepId = 'assignVmIds'

    try {
        report.activeVmId = assignVmIds(input.scenario, input.activeVmId)
        dependencies.onVmIdsAssigned?.(report.activeVmId)
        setStep(report, 'assignVmIds', 'ok')

        currentStep = 'populateDefaults'
        if (dependencies.getCacheRegion) {
            input.scenario.hv.CACHE_REGION = dependencies.getCacheRegion()
        }
        const scenarioWithDefaults = await populateScenarioDefaults(
            input.scenario, dependencies.scenarioToXml, dependencies.populateDefaults)
        const scenarioXml = dependencies.scenarioToXml(scenarioWithDefaults)
        report.scenario = scenarioWithDefaults
        report.scenarioXml = scenarioXml
        dependencies.onScenarioPopulated?.(scenarioWithDefaults)
        report.needSaveLaunchScript = scenarioWithDefaults.vm.some(
            (vm: ScenarioDoc) => vm.load_order === 'POST_LAUNCHED_VM')
        setStep(report, 'populateDefaults', 'ok')

        currentStep = 'writeScenario'
        await dependencies.writeFile(artifactPath(input.workingFolder, 'scenario.xml'), scenarioXml)
        setStep(report, 'writeScenario', 'ok')

        currentStep = 'validate'
        dependencies.onScenarioWritten?.(scenarioWithDefaults)
        const allErrors = dependencies.validateScenario(input.boardXml, scenarioXml)
        report.validationErrors = allErrors
        dependencies.onValidated?.(allErrors, scenarioWithDefaults)
        const validationFailed = input.vmNameError || allErrors.syntactic_errors.length !== 0 ||
            allErrors.semantic_errors.length !== 0
        if (validationFailed) {
            throw new Error(input.vmNameError || 'validation failed')
        }
        setStep(report, 'validate', 'ok')

        currentStep = 'configSummary'
        const configSummary = dependencies.generateConfigSummary(input.boardXml, scenarioXml)

        currentStep = 'launchScripts'
        report.launchScripts = report.needSaveLaunchScript
            ? dependencies.generateLaunchScript(input.boardXml, scenarioXml)
            : {}

        currentStep = 'configSummary'
        await dependencies.writeFile(artifactPath(input.workingFolder, 'config_summary.rst'), configSummary)
        setStep(report, 'configSummary', 'ok')

        currentStep = 'launchScripts'
        const writeDone: Promise<void>[] = []
        for (const filename in report.launchScripts) {
            writeDone.push(dependencies.writeFile(
                artifactPath(input.workingFolder, filename), report.launchScripts[filename]))
        }
        await Promise.all(writeDone)
        if (report.needSaveLaunchScript) {
            setStep(report, 'launchScripts', 'ok')
        }

        currentStep = 'pruneStaleScripts'
        await pruneStaleLaunchScripts(
            input.workingFolder, new Set(Object.keys(report.launchScripts)), dependencies)
        setStep(report, 'pruneStaleScripts', 'ok')
        report.success = true
    } catch (error) {
        report.error = error
        setStep(report, currentStep, 'failed', errorDetail(error))
    }

    return report
}
