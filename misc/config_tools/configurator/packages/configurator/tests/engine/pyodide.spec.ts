import {afterEach, describe, expect, it, vi} from 'vitest';
import {
    configureScenarioJsonSchema,
    PyodideClient,
    PyodideEngine,
    PythonObject,
} from '../../src/engine/pyodide/client';
import {EngineError} from '../../src/engine/types';

const SCHEMA = '{"type":"object"}';
afterEach(() => {
    configureScenarioJsonSchema(undefined);
    vi.unstubAllGlobals();
});


function createClient(handler: (method: string, payload: any) => string) {
    const dispatch = vi.fn((method: string, payload: string) => handler(method, JSON.parse(payload)));
    const runtime = {pyimport: vi.fn(() => ({dispatch}))};
    const client = new PyodideClient(() => runtime, () => SCHEMA);
    return {client, dispatch, runtime};
}

describe('Pyodide client and compatibility facade', () => {
    it('uses the single dispatcher and injects the schema for board loading', () => {
        const {client, dispatch, runtime} = createClient(() => JSON.stringify({
            scenarioJSONSchema: {}, boardInfo: {name: 'board.xml'},
        }));

        const result = new PythonObject(client).loadBoard('<board/>', '/work/board.xml');

        expect(result.boardInfo.name).toBe('board.xml');
        expect(runtime.pyimport).toHaveBeenCalledWith('configurator.pyodide.dispatch');
        expect(dispatch).toHaveBeenCalledWith('loadBoard', JSON.stringify({
            board: '<board/>', path: '/work/board.xml', scenarioJsonSchema: SCHEMA,
        }));
    });

    it('passes the schema to scenario loading and default population', () => {
        const {client, dispatch} = createClient(method => method === 'loadScenario'
            ? JSON.stringify({'acrn-config': {hv: {}, vm: []}})
            : JSON.stringify({xml: '<scenario/>', json: {'acrn-config': {hv: {}, vm: []}}}));
        const pythonObject = new PythonObject(client);

        pythonObject.loadScenario('<scenario/>');
        const populated = pythonObject.populateDefaultValues('<scenario/>');

        expect(populated.xml).toBe('<scenario/>');
        for (const [method, payload] of dispatch.mock.calls) {
            expect(['loadScenario', 'populateDefaultValues']).toContain(method);
            expect(JSON.parse(payload).scenarioJsonSchema).toBe(SCHEMA);
        }
    });

    it('keeps legacy text results while exposing parsed DTOs to the engine', async () => {
        const {client} = createClient(() => JSON.stringify(['00:01.0', '00:02.0']));
        const legacyResult = new PythonObject(client).updateSchema('<board/>', ['00:01.0']);

        expect(legacyResult).toBe('["00:01.0","00:02.0"]');
        await expect(new PyodideEngine(client).deriveSchema({
            boardXml: '<board/>', used: ['00:01.0'],
        })).resolves.toEqual(['00:01.0', '00:02.0']);
    });

    it('returns Promise-based APIs and normalizes runtime failures', async () => {
        const {client} = createClient(() => {
            throw new Error('OSError: permission denied');
        });

        const request = client.request('loadBoard', {board: '<board/>', path: '/board.xml'});
        expect(request).toBeInstanceOf(Promise);
        await expect(request).rejects.toMatchObject({
            name: 'EngineError', kind: 'io', method: 'loadBoard',
        });
    });
});

describe('PyodideEngine DTO mapping', () => {
    it('unwraps scenario documents and maps validation issues', async () => {
        const {client} = createClient(method => {
            if (method === 'loadScenario') {
                return JSON.stringify({'acrn-config': {hv: {RELEASE: 'n'}, vm: []}});
            }
            if (method === 'populateDefaultValues') {
                return JSON.stringify({xml: '<scenario/>', json: {'acrn-config': {hv: {}, vm: []}}});
            }
            return JSON.stringify({
                syntactic_errors: [{message: 'bad value', paths: ['/acrn-config/hv/RELEASE']}],
                semantic_errors: [{message: 'duplicate', path: ['/acrn-config/vm/name'], code: 'duplicate'}],
            });
        });
        const engine = new PyodideEngine(client);

        await expect(engine.loadScenario({xml: '<scenario/>'})).resolves.toEqual({
            hv: {RELEASE: 'n'}, vm: [],
        });
        await expect(engine.populateDefaults({xml: '<scenario/>'})).resolves.toEqual({hv: {}, vm: []});
        await expect(engine.validate({boardXml: '<board/>', scenarioXml: '<scenario/>'})).resolves.toEqual({
            syntactic: [{message: 'bad value', path: ['/acrn-config/hv/RELEASE'], paths: ['/acrn-config/hv/RELEASE']}],
            semantic: [{message: 'duplicate', path: ['/acrn-config/vm/name'], code: 'duplicate'}],
        });
    });

    it('aggregates existing artifact generators without changing their contents', async () => {
        const {client, dispatch} = createClient(method => method === 'generateConfigSummary'
            ? 'summary rst'
            : JSON.stringify({'launch_user_vm_id1.sh': '#!/bin/bash\n'}));
        const engine = new PyodideEngine(client);

        await expect(engine.generateArtifacts({
            boardXml: '<board/>', scenarioXml: '<scenario/>',
        })).resolves.toEqual({
            configSummary: 'summary rst',
            launchScripts: {'launch_user_vm_id1.sh': '#!/bin/bash\n'},
        });
        expect(dispatch.mock.calls.map(call => call[0])).toEqual([
            'generateConfigSummary', 'generateLaunchScript',
        ]);
        await expect(engine.dispose()).resolves.toBeUndefined();
    });
});

describe('Pyodide default providers', () => {
    it('uses configured string and object schemas with the browser runtime', () => {
        const dispatch = vi.fn(() => '{"loaded":true}');
        const pyimport = vi.fn(() => ({dispatch}));
        vi.stubGlobal('pyodide', {pyimport});
        const client = new PyodideClient();
        const objectSchema = {type: 'object', additionalProperties: false};

        configureScenarioJsonSchema(SCHEMA);
        expect(client.dispatchSync('loadBoard', {
            board: '<board/>', path: '/work/board.xml',
        })).toBe('{"loaded":true}');
        configureScenarioJsonSchema(objectSchema);
        expect(client.dispatchSync('loadScenario', {
            scenario: '<scenario/>',
        })).toBe('{"loaded":true}');

        expect(pyimport).toHaveBeenCalledTimes(2);
        expect(dispatch).toHaveBeenNthCalledWith(1, 'loadBoard', JSON.stringify({
            board: '<board/>', path: '/work/board.xml', scenarioJsonSchema: SCHEMA,
        }));
        expect(dispatch).toHaveBeenNthCalledWith(2, 'loadScenario', JSON.stringify({
            scenario: '<scenario/>', scenarioJsonSchema: JSON.stringify(objectSchema),
        }));
    });

    it('loads an unconfigured schema from the legacy browser hook', () => {
        const loader = vi.fn(() => SCHEMA);
        const dispatch = vi.fn(() => 'scenario loaded');
        vi.stubGlobal('__dynamic__load__scenario__from__pyodide__', loader);
        vi.stubGlobal('pyodide', {pyimport: vi.fn(() => ({dispatch}))});

        expect(new PyodideClient().dispatchSync('loadScenario', {
            scenario: '<scenario/>',
        })).toBe('scenario loaded');
        expect(loader).toHaveBeenCalledOnce();
        expect(dispatch).toHaveBeenCalledWith('loadScenario', JSON.stringify({
            scenario: '<scenario/>', scenarioJsonSchema: SCHEMA,
        }));
    });

    it('reports a missing schema before trying to access the runtime', () => {
        const runtimeProvider = vi.fn(() => ({pyimport: vi.fn()}));
        const client = new PyodideClient(runtimeProvider);

        expect(() => client.dispatchSync('loadScenario', {
            scenario: '<scenario/>',
        })).toThrow('The scenario JSON schema has not been configured.');
        expect(runtimeProvider).not.toHaveBeenCalled();
    });

    it('reports an uninitialized browser runtime', () => {
        vi.stubGlobal('pyodide', undefined);
        const client = new PyodideClient(undefined, () => SCHEMA);

        expect(() => client.dispatchSync('loadBoard', {
            board: '<board/>', path: '/work/board.xml',
        })).toThrow('Pyodide has not been initialized.');
    });
});

describe('PyodideEngine fallback behavior', () => {
    it('delegates board loading and structure validation with their DTO payloads', async () => {
        const boardResult = {
            scenarioJSONSchema: {}, boardInfo: {name: 'board.xml'},
        };
        const {client, dispatch} = createClient(method => {
            if (method === 'loadBoard') {
                return JSON.stringify(boardResult);
            }
            return method === 'validateBoardStructure' ? 'board valid' : 'scenario valid';
        });
        const engine = new PyodideEngine(client);

        await expect(engine.loadBoard({xml: '<board/>', path: '/work/board.xml'})).resolves.toEqual(boardResult);
        await expect(engine.validateBoardStructure({xml: '<board/>'})).resolves.toBe('board valid');
        await expect(engine.validateScenarioStructure({xml: '<scenario/>'})).resolves.toBe('scenario valid');
        expect(dispatch.mock.calls).toEqual([
            ['loadBoard', JSON.stringify({
                board: '<board/>', path: '/work/board.xml', scenarioJsonSchema: SCHEMA,
            })],
            ['validateBoardStructure', JSON.stringify({board: '<board/>'})],
            ['validateScenarioStructure', JSON.stringify({scenario: '<scenario/>'})],
        ]);
    });

    it('accepts an already unwrapped scenario document', async () => {
        const scenario = {hv: {RELEASE: 'y'}, vm: []};
        const {client} = createClient(() => JSON.stringify(scenario));

        await expect(new PyodideEngine(client).loadScenario({
            xml: '<scenario/>',
        })).resolves.toEqual(scenario);
    });

    it.each([
        ['the JSON document', {json: {hv: {RELEASE: 'n'}, vm: []}}, {hv: {RELEASE: 'n'}, vm: []}],
        ['the whole result', {hv: {RELEASE: 'y'}, vm: []}, {hv: {RELEASE: 'y'}, vm: []}],
    ])('falls back to %s when defaults have no acrn-config wrapper', async (_name, result, expected) => {
        const {client} = createClient(() => JSON.stringify(result));

        await expect(new PyodideEngine(client).populateDefaults({
            xml: '<scenario/>',
        })).resolves.toEqual(expected);
    });

    it('defaults missing issue collections and normalizes malformed issues', async () => {
        const {client} = createClient((_method, payload) => payload.scenario === '<empty/>'
            ? '{}'
            : JSON.stringify({semantic_errors: [{message: 42, path: 'invalid'}, null]}));
        const engine = new PyodideEngine(client);

        await expect(engine.validate({
            boardXml: '<board/>', scenarioXml: '<empty/>',
        })).resolves.toEqual({syntactic: [], semantic: []});
        await expect(engine.validate({
            boardXml: '<board/>', scenarioXml: '<issues/>',
        })).resolves.toEqual({
            syntactic: [],
            semantic: [{message: '42', path: []}, {message: '', path: []}],
        });
    });
});

describe('Pyodide response error handling', () => {
    it('wraps malformed JSON with the original parse failure as its cause', async () => {
        const {client} = createClient(() => '{not json');
        const request = client.request('loadScenario', {scenario: '<scenario/>'});

        await expect(request).rejects.toMatchObject({
            name: 'EngineError', kind: 'runtime', method: 'loadScenario',
        });
        await expect(request).rejects.toHaveProperty('cause', expect.any(SyntaxError));
    });

    it('preserves an existing EngineError instead of wrapping it again', async () => {
        const failure = new EngineError('validation', 'original failure', 'validateScenario');
        const {client} = createClient(() => {
            throw failure;
        });

        await expect(client.request('validateScenario', {
            board: '<board/>', scenario: '<scenario/>',
        })).rejects.toBe(failure);
    });

    it('maps validation, structure, and non-Error failures to observable error kinds', async () => {
        const cases = [
            ['validateScenario', 'invalid scenario', 'validation'],
            ['validateBoardStructure', new Error('invalid board'), 'structure'],
            ['validateScenarioStructure', new Error('invalid structure'), 'structure'],
            ['loadBoard', 'unexpected failure', 'runtime'],
        ] as const;

        for (const [method, failure, kind] of cases) {
            const {client} = createClient(() => {
                throw failure;
            });
            await expect(client.request(method, {})).rejects.toMatchObject({
                name: 'EngineError',
                kind,
                method,
                message: failure instanceof Error ? failure.message : failure,
                cause: failure,
            });
        }
    });
});
