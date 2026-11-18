import type {
    ArtifactSet,
    EnginePort,
    Issue,
    LoadBoardResult,
    ScenarioDoc,
    ValidationReport,
} from "../types";
import {
    createLegacyPayload,
    decodeEngineResponse,
    ENGINE_RESPONSE_FORMATS,
    SCHEMA_METHODS,
    toEngineError,
} from "./protocol";
import type {EngineMethod, EnginePayload, LegacyOutputFormat} from "./protocol";

export interface PyodideDispatcher {
    dispatch(method: string, payload: string): string;
}

export interface PyodideRuntime {
    pyimport(name: string): PyodideDispatcher;
}

export type ScenarioJsonSchemaProvider = () => string;

let configuredScenarioJsonSchema: string | undefined;

export function configureScenarioJsonSchema(schema: unknown): void {
    configuredScenarioJsonSchema = typeof schema === 'string' ? schema : JSON.stringify(schema);
}

function defaultScenarioJsonSchemaProvider(): string {
    if (configuredScenarioJsonSchema !== undefined) {
        return configuredScenarioJsonSchema;
    }
    const loader = typeof window !== 'undefined'
        ? (window as any).__dynamic__load__scenario__from__pyodide__
        : undefined;
    if (typeof loader === 'function') {
        return loader();
    }
    throw new Error('The scenario JSON schema has not been configured.');
}

function defaultRuntimeProvider(): PyodideRuntime {
    const runtime = typeof window !== 'undefined' ? (window as any).pyodide : undefined;
    if (!runtime) {
        throw new Error('Pyodide has not been initialized.');
    }
    return runtime;
}

export class PyodideClient {
    private runtimeProvider: () => PyodideRuntime;
    private scenarioJsonSchemaProvider: ScenarioJsonSchemaProvider;

    constructor(
        runtimeProvider: () => PyodideRuntime = defaultRuntimeProvider,
        scenarioJsonSchemaProvider: ScenarioJsonSchemaProvider = defaultScenarioJsonSchemaProvider,
    ) {
        this.runtimeProvider = runtimeProvider;
        this.scenarioJsonSchemaProvider = scenarioJsonSchemaProvider;
    }

    private withSchema(method: EngineMethod, payload: EnginePayload): EnginePayload {
        if (!SCHEMA_METHODS.has(method) || payload.scenarioJsonSchema !== undefined) {
            return payload;
        }
        return {...payload, scenarioJsonSchema: this.scenarioJsonSchemaProvider()};
    }

    dispatchSync(method: EngineMethod, payload: EnginePayload): string {
        const requestPayload = this.withSchema(method, payload);
        const dispatcher = this.runtimeProvider().pyimport('configurator.pyodide.dispatch');
        return dispatcher.dispatch(method, JSON.stringify(requestPayload));
    }

    async request<T>(method: EngineMethod, payload: EnginePayload): Promise<T> {
        try {
            const result = this.dispatchSync(method, payload);
            return decodeEngineResponse<T>(result, ENGINE_RESPONSE_FORMATS[method]);
        } catch (error) {
            throw toEngineError(error, method);
        }
    }
}

export const pyodideClient = new PyodideClient();

/**
 * Synchronous compatibility facade. Existing Vue callers continue to use this
 * class while new code consumes the Promise-based PyodideEngine below.
 */
export class PythonObject {
    private client: PyodideClient;

    constructor(client: PyodideClient = pyodideClient) {
        this.client = client;
    }

    api(scriptName: EngineMethod, outputFormat: LegacyOutputFormat, ...params: any[]): any {
        const result = this.client.dispatchSync(scriptName, createLegacyPayload(scriptName, params));
        return decodeEngineResponse(result, outputFormat);
    }

    loadBoard(boardXMLText: string, path: string): any {
        return this.api('loadBoard', 'json', boardXMLText, path);
    }

    loadScenario(scenarioXMLText: string): any {
        return this.api('loadScenario', 'json', scenarioXMLText);
    }

    validateBoardStructure(boardXMLText: string): string {
        return this.api('validateBoardStructure', 'plaintext', boardXMLText);
    }

    validateScenarioStructure(scenarioXMLText: string): string {
        return this.api('validateScenarioStructure', 'plaintext', scenarioXMLText);
    }

    validateScenario(boardXMLText: string, scenarioXMLText: string): any {
        return this.api('validateScenario', 'json', boardXMLText, scenarioXMLText);
    }

    generateLaunchScript(boardXMLText: string, scenarioXMLText: string): Record<string, string> {
        return this.api('generateLaunchScript', 'json', boardXMLText, scenarioXMLText);
    }

    populateDefaultValues(scenarioXMLText: string): any {
        return this.api('populateDefaultValues', 'json', scenarioXMLText);
    }

    generateConfigSummary(boardXMLText: string, scenarioXMLText: string): string {
        return this.api('generateConfigSummary', 'plaintext', boardXMLText, scenarioXMLText);
    }

    updateSchema(boardXMLText: string, scenarioXMLText: string[]): string {
        return this.api('updateSchema', 'plaintext', boardXMLText, scenarioXMLText);
    }
}

function normalizeIssue(issue: any): Issue {
    return {
        ...issue,
        path: Array.isArray(issue?.path) ? issue.path : (Array.isArray(issue?.paths) ? issue.paths : []),
        message: typeof issue?.message === 'string' ? issue.message : String(issue?.message ?? ''),
    };
}

export class PyodideEngine implements EnginePort {
    private client: PyodideClient;

    constructor(client: PyodideClient = pyodideClient) {
        this.client = client;
    }

    loadBoard(req: { xml: string; path: string }): Promise<LoadBoardResult> {
        return this.client.request('loadBoard', {board: req.xml, path: req.path});
    }

    async loadScenario(req: { xml: string }): Promise<ScenarioDoc> {
        const result = await this.client.request<Record<string, ScenarioDoc>>('loadScenario', {scenario: req.xml});
        return result['acrn-config'] ?? result;
    }

    async populateDefaults(req: { xml: string }): Promise<ScenarioDoc> {
        const result = await this.client.request<any>('populateDefaultValues', {scenario: req.xml});
        return result.json?.['acrn-config'] ?? result.json ?? result;
    }

    async validate(req: { boardXml: string; scenarioXml: string }): Promise<ValidationReport> {
        const result = await this.client.request<any>('validateScenario', {
            board: req.boardXml,
            scenario: req.scenarioXml,
        });
        return {
            syntactic: (result.syntactic_errors ?? []).map(normalizeIssue),
            semantic: (result.semantic_errors ?? []).map(normalizeIssue),
        };
    }

    deriveSchema(req: { boardXml: string; used: string[] }): Promise<string[]> {
        return this.client.request('updateSchema', {board: req.boardXml, scenario: req.used});
    }

    async generateArtifacts(req: { boardXml: string; scenarioXml: string }): Promise<ArtifactSet> {
        const payload = {board: req.boardXml, scenario: req.scenarioXml};
        const configSummary = await this.client.request<string>('generateConfigSummary', payload);
        const launchScripts = await this.client.request<Record<string, string>>('generateLaunchScript', payload);
        return {configSummary, launchScripts};
    }

    validateBoardStructure(req: { xml: string }): Promise<string> {
        return this.client.request('validateBoardStructure', {board: req.xml});
    }

    validateScenarioStructure(req: { xml: string }): Promise<string> {
        return this.client.request('validateScenarioStructure', {scenario: req.xml});
    }

    async dispose(): Promise<void> {
        // Pyodide currently owns a page-lifetime runtime. Worker ownership and
        // actual teardown are deliberately left to the later performance stage.
    }
}

export const pyodideEngine = new PyodideEngine();
