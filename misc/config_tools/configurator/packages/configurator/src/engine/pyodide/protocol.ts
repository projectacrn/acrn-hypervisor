import {EngineError} from "../types";
import type {EngineErrorKind} from "../types";

export const ENGINE_RESPONSE_FORMATS = {
    loadBoard: 'json',
    loadScenario: 'json',
    validateBoardStructure: 'text',
    validateScenarioStructure: 'text',
    validateScenario: 'json',
    generateLaunchScript: 'json',
    populateDefaultValues: 'json',
    generateConfigSummary: 'text',
    updateSchema: 'json',
} as const;

export type EngineMethod = keyof typeof ENGINE_RESPONSE_FORMATS;
export type EnginePayload = Record<string, any>;
export type LegacyOutputFormat = 'json' | 'plaintext';

export const SCHEMA_METHODS = new Set<EngineMethod>([
    'loadBoard',
    'loadScenario',
    'populateDefaultValues',
]);

export function createLegacyPayload(method: EngineMethod, params: any[]): EnginePayload {
    switch (method) {
        case 'loadBoard':
            return {board: params[0], path: params[1]};
        case 'loadScenario':
        case 'populateDefaultValues':
        case 'validateScenarioStructure':
            return {scenario: params[0]};
        case 'validateBoardStructure':
            return {board: params[0]};
        case 'validateScenario':
        case 'generateLaunchScript':
        case 'generateConfigSummary':
            return {board: params[0], scenario: params[1]};
        case 'updateSchema':
            return {board: params[0], scenario: params[1]};
    }
}

export function decodeEngineResponse<T>(result: string, format: 'json' | 'text' | 'plaintext'): T {
    return (format === 'json' ? JSON.parse(result) : result) as T;
}

export function toEngineError(error: unknown, method: string): EngineError {
    if (error instanceof EngineError) {
        return error;
    }

    const message = error instanceof Error ? error.message : String(error);
    let kind: EngineErrorKind = 'runtime';
    if (/\b(IOError|OSError|FileNotFoundError|PermissionError)\b/.test(message)) {
        kind = 'io';
    } else if (method === 'validateScenario') {
        kind = 'validation';
    } else if (method === 'validateBoardStructure' || method === 'validateScenarioStructure') {
        kind = 'structure';
    }
    return new EngineError(kind, message, method, error);
}
