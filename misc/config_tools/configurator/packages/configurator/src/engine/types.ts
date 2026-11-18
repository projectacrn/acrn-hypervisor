export type ScenarioDoc = Record<string, any>;

export interface BoardInfo {
    name: string;
    content: string;
    CAT_INFO: any[];
    BIOS_INFO: string;
    BASE_BOARD_INFO: string;
    [key: string]: any;
}

export interface LoadBoardResult {
    scenarioJSONSchema: Record<string, any>;
    boardInfo: BoardInfo;
}

export interface Issue {
    path: string[];
    message: string;
    code?: string;
    paths?: string[];
    source?: string;
    [key: string]: any;
}

export interface ValidationReport {
    syntactic: Issue[];
    semantic: Issue[];
}

export interface ArtifactSet {
    configSummary: string;
    launchScripts: Record<string, string>;
}

export type EngineErrorKind = 'structure' | 'validation' | 'runtime' | 'io';

export class EngineError extends Error {
    readonly kind: EngineErrorKind;
    readonly method?: string;
    readonly cause?: unknown;

    constructor(kind: EngineErrorKind, message: string, method?: string, cause?: unknown) {
        super(message);
        this.name = 'EngineError';
        this.kind = kind;
        this.method = method;
        this.cause = cause;
        Object.setPrototypeOf(this, new.target.prototype);
    }
}

export interface EnginePort {
    loadBoard(req: { xml: string; path: string }): Promise<LoadBoardResult>;
    loadScenario(req: { xml: string }): Promise<ScenarioDoc>;
    populateDefaults(req: { xml: string }): Promise<ScenarioDoc>;
    validate(req: { boardXml: string; scenarioXml: string }): Promise<ValidationReport>;
    deriveSchema(req: { boardXml: string; used: string[] }): Promise<string[]>;
    generateArtifacts(req: { boardXml: string; scenarioXml: string }): Promise<ArtifactSet>;
    dispose(): Promise<void>;
}
