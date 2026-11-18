# Configurator Engine Protocol

This document is based on the following implementations and does not describe an unimplemented Worker RPC:

- `packages/configurator/src/engine/types.ts`
- `packages/configurator/src/engine/pyodide/{client,protocol,bootstrap}.ts`
- `pyodide/dispatch.py` and the nine registered Python methods

## 1. Invocation Model

### 1.1 Three Entry Points

| Entry point | Purpose | Current behavior |
| --- | --- | --- |
| `EnginePort` / `PyodideEngine` | DTO interface for new code | Returns Promises; the client unpacks DTOs and normalizes validation errors |
| `PyodideClient` | TypeScript wire-protocol adapter | Injects schemas, serializes payloads, invokes the Python dispatcher, and parses strings according to the method |
| `PythonObject` | Synchronous compatibility facade for legacy pages and stores | Retains positional arguments and the legacy `json`/`plaintext` return conventions; still widely used |

The underlying call is equivalent to:

```ts
const dispatcher = window.pyodide.pyimport('configurator.pyodide.dispatch')
const resultText = dispatcher.dispatch(method, JSON.stringify(payload))
```

Python's `dispatch(method, payload)` accepts either a JSON string or an already parsed `dict`, but the TypeScript client always sends a JSON string. The return value is not a `{result, error}` envelope; each method determines its own string result, and the TypeScript client uses `JSON.parse` for JSON methods.

The protocol has no request ID, cancellation marker, timeout, version negotiation, or structured error response. Store revisions only ignore stale results; they do not interrupt Python execution.

### 1.2 Synchronous Execution Limitation

`dispatchSync` directly executes Pyodide's `pyimport(...).dispatch(...)`. Although `PyodideClient.request` returns a Promise, the synchronous call occurs before the Promise return path, so it does not prevent UI-thread blocking. `PyodideEngine` methods likewise provide only an asynchronous facade; they do not imply Worker execution or parallelism.

`generateArtifacts` first awaits `generateConfigSummary` and then calls `generateLaunchScript` after success. A summary failure prevents the launch-script request.

## 2. Wire Protocol

### 2.1 Methods, Payloads, and Raw Results

In the following table, `scenarioJsonSchema` is a JSON **string** automatically injected by `PyodideClient`; all other XML fields are also strings.

| method | Payload fields | Raw dispatcher result | Description |
| --- | --- | --- | --- |
| `loadBoard` | `board`, `path`, `scenarioJsonSchema` | JSON object | Generates dynamic form schemas and extracts Board/CAT information |
| `loadScenario` | `scenario`, `scenarioJsonSchema` | JSON object | Converts XML to a dictionary and corrects array and integer fields using the schema |
| `validateBoardStructure` | `board` | text | Joins multiple structural errors with two newlines; exceptions are also converted to text internally |
| `validateScenarioStructure` | `scenario` | text | Empty text means success; exceptions are also converted to text internally |
| `validateScenario` | `board`, `scenario` | JSON object | Returns separate syntactic and semantic error groups |
| `generateLaunchScript` | `board`, `scenario` | JSON object | Keys are script filenames and values are complete script text |
| `populateDefaultValues` | `scenario`, `scenarioJsonSchema` | JSON object | Returns XML and JSON representations after default population |
| `generateConfigSummary` | `board`, `scenario` | text | Returns the complete `config_summary.rst` content |
| `updateSchema` | `board`, `scenario` | JSON array | Here `scenario` is a `string[]` of PCI devices in use, not XML; returns devices that have not been assigned |

`ENGINE_RESPONSE_FORMATS` marks `updateSchema` as `json`. The legacy `PythonObject.updateSchema` retains its original `plaintext` contract and returns unparsed JSON text; the two conventions are not interchangeable.

### 2.2 Schema Injection

Only the following methods belong to `SCHEMA_METHODS`:

```text
loadBoard · loadScenario · populateDefaultValues
```

When the payload does not define `scenarioJsonSchema`, the client obtains it from the provider injected into the constructor. The default provider first uses the value configured at startup through `configureScenarioJsonSchema`, then falls back to `window.__dynamic__load__scenario__from__pyodide__()` for compatibility. It throws an error when neither is available.

The three Python loading/default-population functions retain default schema-argument fallbacks for direct execution, but normal browser-client requests explicitly supply the value. The other six methods do not automatically receive a schema.

### 2.3 Raw Response Shapes

These are the raw Python method shapes; they are not all exposed directly through `EnginePort`:

```ts
// loadBoard
{
    scenarioJSONSchema: Record<string, any>,
    boardInfo: {
        name: string,
        content: string,
        CAT_INFO: any[],
        BIOS_INFO: string,
        BASE_BOARD_INFO: string,
        // Additional fields may pass through.
    }
}

// loadScenario
{ 'acrn-config': ScenarioDoc }

// populateDefaultValues
{ xml: string, json: { 'acrn-config': ScenarioDoc } }

// validateScenario
{
    syntactic_errors: Array<Record<string, any>>,
    semantic_errors: Array<Record<string, any>>
}

// generateLaunchScript
Record<string, string>

// updateSchema
string[]
```

The two structural-validation methods return plain text. Python validators and generators do not have a common JSON error DTO.

## 3. EnginePort and DTOs

### 3.1 Public Interface

`EnginePort` currently has only these seven methods:

```ts
interface EnginePort {
    loadBoard(req: { xml: string; path: string }): Promise<LoadBoardResult>
    loadScenario(req: { xml: string }): Promise<ScenarioDoc>
    populateDefaults(req: { xml: string }): Promise<ScenarioDoc>
    validate(req: { boardXml: string; scenarioXml: string }): Promise<ValidationReport>
    deriveSchema(req: { boardXml: string; used: string[] }): Promise<string[]>
    generateArtifacts(req: { boardXml: string; scenarioXml: string }): Promise<ArtifactSet>
    dispose(): Promise<void>
}
```

`PyodideEngine` additionally exposes two structural-validation methods that are not yet part of `EnginePort`:

```ts
validateBoardStructure(req: { xml: string }): Promise<string>
validateScenarioStructure(req: { xml: string }): Promise<string>
```

These forward `validateBoardStructure` and `validateScenarioStructure`, respectively. Callers depending on the `EnginePort` type cannot assume that these additional methods exist.

### 3.2 DTO Definitions

```ts
type ScenarioDoc = Record<string, any>

interface BoardInfo {
    name: string
    content: string
    CAT_INFO: any[]
    BIOS_INFO: string
    BASE_BOARD_INFO: string
    [key: string]: any
}

interface LoadBoardResult {
    scenarioJSONSchema: Record<string, any>
    boardInfo: BoardInfo
}

interface Issue {
    path: string[]
    message: string
    code?: string
    paths?: string[]
    source?: string
    [key: string]: any
}

interface ValidationReport {
    syntactic: Issue[]
    semantic: Issue[]
}

interface ArtifactSet {
    configSummary: string
    launchScripts: Record<string, string>
}
```

These types still contain `any` and index signatures. They define adaptation boundaries, not complete types generated from the XML schema.

### 3.3 Method Mapping

| EnginePort method | Wire method | DTO handling |
| --- | --- | --- |
| `loadBoard({xml, path})` | `loadBoard` | Maps `xml` to `board` and returns `LoadBoardResult` directly |
| `loadScenario({xml})` | `loadScenario` | Prefers `result['acrn-config']`; returns the entire result when absent |
| `populateDefaults({xml})` | `populateDefaultValues` | Prefers `result.json['acrn-config']`, then falls back to `result.json`, and finally to the entire result; does not return the populated XML |
| `validate({boardXml, scenarioXml})` | `validateScenario` | Maps `syntactic_errors`/`semantic_errors` to `syntactic`/`semantic`; missing values become empty arrays |
| `deriveSchema({boardXml, used})` | `updateSchema` | Maps `used` to the wire field `scenario` and parses a `string[]` |
| `generateArtifacts({boardXml, scenarioXml})` | Two calls | Requests `generateConfigSummary` and then `generateLaunchScript`, combining them into `{configSummary, launchScripts}` |
| `dispose()` | None | Currently a no-op; the page lifecycle still owns Pyodide |

Each issue returned by `validate` retains its original fields and is normalized as follows:

- `path`: uses an existing array `issue.path`, otherwise an array `issue.paths`, and otherwise `[]`;
- `message`: preserves strings, converts other values to strings, and converts null/undefined to an empty string;
- `paths`, `source`, `code`, and other extension fields continue to pass through.

Note that `ValidationErrors` in `domain/validation/types.ts` still uses the legacy `syntactic_errors`, `semantic_errors`, and issue `paths` fields. The store currently obtains this legacy structure through `PythonObject` and passes it directly to `mapErrors`; it does not use the `ValidationReport` returned by `PyodideEngine.validate`. The two shapes cannot be interchanged without adaptation.

## 4. Error Contract

### 4.1 EngineError

```ts
type EngineErrorKind = 'structure' | 'validation' | 'runtime' | 'io'

class EngineError extends Error {
    readonly kind: EngineErrorKind
    readonly method?: string
    readonly cause?: unknown
}
```

`name` is always `EngineError`. `method` records the wire method that raised the exception, and `cause` retains the original exception. If the value passed to `toEngineError` is already an `EngineError`, it is returned unchanged and is not reclassified.

### 4.2 Classification Rules

TypeScript infers the classification from the method name and exception text, in this priority order:

| kind | Condition |
| --- | --- |
| `io` | The message matches a standalone `IOError`, `OSError`, `FileNotFoundError`, or `PermissionError` |
| `validation` | Not `io`, and the method is `validateScenario` |
| `structure` | Neither condition above applies, and the method is `validateBoardStructure` or `validateScenarioStructure` |
| `runtime` | All other exceptions, including JSON parsing, unknown methods, missing fields, and ordinary Python exceptions |

These are not structured error codes actively returned by Python. Ordinary configuration errors usually appear in arrays within a successful `validateScenario` response and do not throw `EngineError`; exceptions caught inside structural-validation functions likewise become nonempty text. Callers must handle both rejected Promises and successful responses containing errors.

The legacy `PythonObject.api` calls `dispatchSync` directly without the `toEngineError` wrapper in `request`, so legacy callers still see raw exceptions.

## 5. Python Dispatch Registry

At module load, `pyodide/dispatch.py` imports the nine implementations, stores them in `METHOD_REGISTRY`, and uses small adapters to unpack arguments from a dictionary:

| Registered name | Implementation called by the adapter |
| --- | --- |
| `loadBoard` | `loadBoard.main(board, path, scenarioJsonSchema)` |
| `loadScenario` | `loadScenario.main(scenario, scenarioJsonSchema)` |
| `validateBoardStructure` | `validateBoardStructure.main(board)` |
| `validateScenarioStructure` | `validateScenarioStructure.main(scenario)` |
| `validateScenario` | `validateScenario.main(board, scenario)` |
| `generateLaunchScript` | `generateLaunchScript.main(board, scenario)` |
| `populateDefaultValues` | `populateDefaultValues.main(scenario, scenarioJsonSchema)` |
| `generateConfigSummary` | `generateConfigSummary.main(board, scenario)` |
| `updateSchema` | `updateSchema.main(board, scenario)` |

The dispatcher has only these rules:

1. Parse a string payload with `json.loads`;
2. Throw `TypeError('Engine payload must be a JSON object.')` when the result is not a `dict`;
3. Throw `ValueError('Unknown engine method: ...')` for an unregistered method;
4. Return a registered handler's result directly without serializing it again or catching implementation exceptions.

Dynamic-enum sorting in `loadBoard.py` uses the `SORT_KEY_FUNCTIONS` allowlist instead of `eval`. Schemas enter Python through payloads, so Python modules do not need to import a JS global getter.

## 6. Lifecycle and Bootstrap

`bootstrapPyodide` loads the runtime from `/thirdLib/pyodide/`, installs `micropip`, `lxml`, and `beautifulsoup4`, and then installs xmltodict, elementpath, defusedxml, xmlschema, and the `acrn_config_tools` wheel from the project's thirdLib directory. It assigns the runtime to `window.pyodide` after completion.

Before bootstrap, `main.js` selects and installs the Tauri/WebMock platform; the app mounts only after Pyodide initialization. `dispose` currently does not unload packages, release the runtime, or cancel requests.

## 7. Compatibility Entry-Point Reference

| PythonObject method | Wire method | Legacy return convention |
| --- | --- | --- |
| `loadBoard(boardXMLText, path)` | `loadBoard` | Parsed object |
| `loadScenario(scenarioXMLText)` | `loadScenario` | Parsed object still containing `acrn-config` |
| `validateBoardStructure(boardXMLText)` | `validateBoardStructure` | text |
| `validateScenarioStructure(scenarioXMLText)` | `validateScenarioStructure` | text |
| `validateScenario(boardXMLText, scenarioXMLText)` | `validateScenario` | Parsed legacy `ValidationErrors` object |
| `generateLaunchScript(boardXMLText, scenarioXMLText)` | `generateLaunchScript` | Parsed `Record<string, string>` |
| `populateDefaultValues(scenarioXMLText)` | `populateDefaultValues` | Parsed `{xml, json}` object |
| `generateConfigSummary(boardXMLText, scenarioXMLText)` | `generateConfigSummary` | text |
| `updateSchema(boardXMLText, used)` | `updateSchema` | Unparsed JSON text |

When adding a method, first determine its wire return format, then implement the EnginePort DTO mapping and any genuinely necessary legacy adaptation. Do not infer Python's actual return type from the legacy interface's `plaintext` argument.
