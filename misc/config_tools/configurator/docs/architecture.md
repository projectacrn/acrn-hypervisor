# ACRN Configurator Architecture

This document describes the structure **already implemented** on the `refine_configurator` branch; it is not a preview of a proposed directory layout. See the [refactoring plan](./refactoring-plan.md) for the design background, acceptance constraints, and remaining work, and the [engine protocol](./engine-protocol.md) for engine invocation details.

## 1. Layering Overview

Configurator remains a Vue 3 + Pyodide + Tauri 2 desktop application. The current refactoring uses layers within a single package. It neither splits the application into additional npm packages nor replaces the UI framework or Python engine.

```text
pages/ and components/       Pages, dialogs, forms, and custom widgets
              │  Read/write state and invoke actions; some legacy flows still
              │  pass through the Configurator compatibility layer
              ▼
stores/                     scenario · validation · workspace (Pinia)
              │  Orchestrate domain functions; the scenario store still uses
              │  the compatibility layer
              ▼
domain/                     CAT, references, schema, error mapping, save pipeline
              │  No dependency on Vue, Tauri, Pyodide, or the form library
              ▼
engine/ + platform/         EnginePort / Pyodide adapter; filesystem, dialog,
              │             and history ports
              ▼                         ▼
pyodide/ (Python)           Tauri commands → AppState / config / fsutil
```

`lib/acrn.ts` is the transitional `Configurator` compatibility facade. It forwards legacy filesystem, history, dialog, and `PythonObject` calls to the new ports while retaining responsibilities that have not yet moved into the domain layer, including `newVM`, `createNewScenario`, and `convertScenarioToXML`. New business rules must not be added to this class.

### Current Directory Layout

```text
misc/config_tools/configurator/
├── packages/configurator/src/
│   ├── main.js                  # Plugin, Pinia, platform, and Pyodide bootstrap
│   ├── App.vue / router.js      # Root component and router
│   ├── pages/                   # Pages and ConfigForm/CustomWidget; not yet
│   │                            # renamed to views/forms
│   ├── components/common/       # Banner, ControlBar, Footer
│   ├── stores/
│   │   ├── scenario.ts          # Current Board/Scenario, editing, CAT,
│   │   │                        # validation, and save orchestration
│   │   ├── validation.ts        # Validation results and stale-response control
│   │   └── workspace.ts         # Three history types and their revisions
│   ├── domain/
│   │   ├── cat/                 # View derivation, switch correction, mask
│   │   │                        # conversion, and CACHE_REGION write-back
│   │   ├── references/          # Declarative VM-name reference rules and
│   │   │                        # rename/delete propagation
│   │   ├── schema/              # HV/VM schema selection and dynamic PCI enums
│   │   ├── validation/          # Python error paths to VM tab mapping
│   │   └── artifacts/           # VM renumbering, defaults, saving, and stale
│   │                            # launch-script cleanup
│   ├── engine/
│   │   ├── types.ts             # EnginePort, DTOs, EngineError
│   │   └── pyodide/             # Bootstrap, synchronous client, Promise facade,
│   │                            # and protocol mapping
│   ├── platform/
│   │   ├── types.ts             # FsPort, DialogPort, HistoryPort, PlatformPorts
│   │   ├── index.ts             # set/getPlatform and Tauri/WebMock selection
│   │   ├── tauri.ts             # invoke/dialog adapters and joinPath
│   │   └── webMock.ts           # In-memory filesystem, history, and IPC mock
│   └── lib/acrn.ts              # Legacy Configurator/PythonObject entry point
├── pyodide/                    # Nine Python methods and dispatch.py registry
└── packages/configurator/src-tauri/src/
    ├── main.rs                 # Builder, manage(AppState), command registration
    ├── commands/               # fs, history, workspace
    ├── state.rs                # AppState and Workspace
    ├── config.rs               # UserConfig, history, corrupt-config protection
    ├── fsutil.rs               # Atomic replacement and directory traversal
    └── error.rs                # Currently Result<T, String>
```

The `app/`, `views/`, `forms/`, `stores/session.ts`, and generated `types/generated/scenario.d.ts` paths mentioned in the design do not currently exist. `ConfigForm.vue` remains the wrapper around `@lljj/vue3-form-naive`, and custom widgets remain under `pages/Config/ConfigForm/CustomWidget/`.

## 2. Dependency Rules and Responsibility Boundaries

### 2.1 Hard Rules

| Layer | May depend on | Must not depend on / notes |
| --- | --- | --- |
| `domain/` | TypeScript and pure utility libraries (CAT currently uses lodash) | Must not import Vue, Pinia, Tauri, Pyodide, `@lljj/vue3-form-naive`, or higher layers such as `pages`, `stores`, `engine`, and `platform` |
| `engine/` | DTOs, Pyodide runtime, protocol helpers | Does not store Scenario editing state, manipulate the DOM, or determine save policy |
| `platform/` | Port types, Tauri APIs, browser mocks | Contains no CAT/VM/schema business rules; WebMock and Tauri must honor the same port contracts |
| `stores/` | `domain`, `engine`/`platform`, or the transitional compatibility layer | Orchestrates state without reimplementing domain algorithms; asynchronous results must check the appropriate revision |
| `pages/`, `components/` | Stores, props/emit, form library, compatibility layer | Primarily presentation and interaction; reusable computations belong in lower layers; existing DOM selectors and interaction flows must not change |
| Python `pyodide/` | `acrn_config_tools`, XML/JSON libraries | No longer imports a schema getter from JS; the client explicitly supplies schemas to requests that need them |
| Rust | `commands` → `state` / `config` / `fsutil` | Commands do not independently reread user configuration; shared configuration read-modify-write operations use the same `Mutex` |

“Pure function” in `domain` means that a function does not access the UI, runtime, or filesystem. Some existing functions intentionally mutate the supplied `ScenarioDoc` in place, including `renameVm`, `removeVm`, and `assignVmIds`; callers must not assume that every function returns an immutable snapshot.

### 2.2 Explicit Transitional Exceptions

- `stores/scenario.ts` and several pages still call the synchronous `PythonObject` through `lib/acrn.ts`; `PyodideEngine` is implemented but is not yet the only entry point for all stores.
- `platform/webMock.ts` reuses `joinPath` from `platform/tauri.ts`, which contains no Tauri call. This shares a path utility and does not mean that WebMock invokes the native backend.
- `main.js` still exposes the runtime as `window.pyodide` and retains `window.systemInfo` and `window.isDev`. The schema getter also remains as a client compatibility fallback, although Python no longer imports it.
- There are currently no `@domain/*`, `@engine/*`, or `@platform/*` path aliases; imports use relative paths.
- `domain/artifacts/pipeline.ts` still contains its own `artifactPath`; it has not yet been converted to `FsPort.join`.

New code should follow the target boundaries rather than expand the compatibility layer by copying these exceptions.

## 3. State and Core Data Flows

### 3.1 Scenario Store

`useScenarioStore` owns:

- `board`: `BoardInfo`, including the original Board XML, filename, and CAT/BIOS/Base Board information;
- `schemas`: Basic/Advanced schemas organized by `HV`, `PreLaunchedVM`, `ServiceVM`, and `PostLaunchedVM`;
- `doc`: the current `{ hv, vm[] }`; VM selection uses the internal `@id` key, while the name is used for display, references, and artifacts;
- `activeVmId`: `-1` for the HV;
- `dirty` and `revision`: edit state and a monotonically increasing version.

The main getters are `activeVm`, `activeSchema`, `activeFormData`, `vmsByLoadOrder`, and `vmNames`. `activeSchema` uses `domain/schema/derive.ts` to select the current HV schema or the schema for the VM's load order. For optional PCI devices, the store invokes the engine and then merges the result through a pure derivation function.

`revision` discards stale results from Scenario loads and post-save operations. The `validation` store has an independent revision, and both checks must pass before validation results are applied. The `workspace` store maintains separate revisions for WorkingFolder, Board, and Scenario. These are store state guards, not request IDs in the engine protocol.

### 3.2 Loading and Editing

1. The Board page uses `Configurator.loadBoard` to read the file, perform structural validation, and invoke `loadBoard`, then passes `boardInfo` and dynamic schemas to `scenarioStore.loadBoard`.
2. The Scenario page loads through `scenarioStore.loadFromFile`; success sets `doc` and triggers validation. Newly created and imported documents enter the same state through `load`/`importScenario`.
3. `@change` in `ConfigForm.vue` only calls `updateActiveForm`. The store replaces HV/VM data by `@id`, detects VM renames and invokes `renameVm`, and performs the existing RTVM `real_time_vcpu` correction and PCI schema update.
4. When a VM is deleted, `removeVm` first cleans references to every name being deleted and then removes the VM. Deleting the Service VM also deletes its Post-launched VMs, preserving the existing interaction semantics.

### 3.3 CAT

`doc.hv.CACHE_REGION` is the editing data source. The legacy `CATDB` and `Proxy` have been removed.

- `deriveCatView` derives `regions`, `summary`, and `errorMsg` from Board CAT capabilities, schemas, CPU affinity, and the Scenario;
- `correctSwitches` returns RDT/CDP/VCAT switch values and required corrections, and the store explicitly writes those corrections to the document;
- the `setClosMask` action writes one mask through immutable allocation/policy updates and increments `dirty` and `revision`;
- `hexToRange` / `rangeToHex` retain the legacy truncation, padding, and noncontiguous-bit compression behavior, which is locked by snapshot tests;
- saving still calls `buildCacheRegion(this.getCatView())` to normalize `CACHE_REGION`. This compatibility step remains; save-time merging has not been completely eliminated.
- The `CAT.vue` template reads a read-only view snapshot, including switches and regions, explicitly rebuilt by `updateCatInfo()`. Every programmatic change must refresh through the same entry point. The component explicitly watches `scenarioStore.revision` to cover mask edits and external Scenario updates. Each of the three checkboxes uses its switch value as a `key`, ensuring that WebKit recreates the native control after a prevented default click instead of retaining a stale checked state. Do not create hidden reactive dependencies with semantically empty reads such as `void this.someRevision`. Confirmation dialogs must explicitly import the asynchronous `confirm` from `@tauri-apps/plugin-dialog`; the browser's global `confirm` returns a synchronous boolean and has no `.then()` method. Delayed enables and native confirmation callbacks use a component operation sequence and must not write to an old form after unmounting.

### 3.4 Save Pipeline

`runSavePipeline` always executes:

```text
assignVmIds → populateDefaults → writeScenario → validate
  → configSummary / launchScripts → pruneStaleScripts
```

It returns a `SaveReport` containing seven `StepReport` entries, the final Scenario/XML, launch scripts, validation errors, the current VM ID, and `success`. `Config.vue` only converts the report into the existing dialog text.

The following behavior must be preserved:

- VMs are renumbered in Pre-launched, Service, Post-launched order while retaining the current object selection;
- `scenario.xml` is written first, and an editable working state remains even when validation fails;
- duplicate VM names or syntactic/semantic validation errors prevent summary/launch-script updates and stale-script cleanup;
- `config_summary.rst` and the generated launch scripts are written only after validation succeeds;
- stale `launch*.sh` files are deleted only after all replacement writes succeed;
- with no Post-launched VM, the launch-script generator is not invoked, but stale-script cleanup still runs;
- after any step fails, subsequent steps remain `skipped`, with the cause reported through `error` and the failed step's `detail`.

## 4. Platform and Native Backend

`getPlatform()` selects Tauri or WebMock based on `window.__TAURI_INTERNALS__`; tests can inject a platform explicitly with `setPlatform`. The ports include:

- `FsPort`: read text, atomic write, list directories, create directories, delete files, and join paths;
- `DialogPort`: file and directory selection;
- `HistoryPort`: read and append WorkingFolder/Board/Scenario history;
- `getHome`: retrieve the home directory.

The Tauri backend creates `AppState { config: Mutex<UserConfig>, workspace: Mutex<Workspace> }` once at startup. History read-modify-write operations occur under one lock, and Workspace owns the only working-folder value. `fsutil::write_file` uses a temporary file in the same directory, `sync_all`, and a replacement operation, preserving existing permissions. Windows uses `MoveFileExW`, and failures clean up temporary files.

Native command names and serialized arguments remain compatible, including `acrn_read`, `acrn_write`, `get_history`, and `set_working_folder`. `error.rs` currently provides `CommandResult<T> = Result<T, String>`, not structured `{code, message}` errors, so the frontend cannot depend on error codes.

## 5. Extension Guides

### 5.1 Add a VM Reference Rule

Use this process for fields that store VM names, must follow renames, and must be cleaned up when a VM is deleted.

1. Add a rule to `VM_NAME_REFERENCES` in `src/domain/references/graph.ts`:

   ```ts
   {
       path: 'hv.SOME_CONTAINER.SOME_LIST[].SOME_ITEM[].VM_NAME',
       onRemove: 'drop-entry'
   }
   ```

   `[]` marks array segments, and the leaf is the field containing the name. The existing `onRemove` values are:
   - `drop-entry`: delete the immediate array item containing the reference;
   - `drop-connection`: delete the outermost connection (currently used for VUART).

   The existing `pruneWhen: 'fewer-than-2-siblings'` implementation is specific to IVSHMEM region cleanup. Before using it for a new structure, inspect and extend `rename.ts`; changing only the declaration table is insufficient.

2. Add rename, deletion, missing-reference, and array-item retention/removal tests to `tests/domain/references.spec.ts`. If existing snapshots are affected, also inspect `tests/golden/vm-references.spec.js`; do not change legacy semantics merely because different behavior seems more reasonable.
3. No rename branch is needed in `Config.vue`: `updateActiveForm` automatically calls `renameVm`, and `scenarioStore.removeVm` automatically calls `removeVm`.
4. If the new field is not a string name, requires a cross-VM ID reference, or has deletion semantics outside the two categories above, extend `RefRule` and the traversal implementation before adding the rule.
5. Launch-script filenames are not managed by the reference table; regeneration during save and `pruneStaleLaunchScripts` keep them consistent.

Finally, run the full Vitest suite and the relevant native E2E workflows. Do not modify the XML schema or artifact format to accommodate a reference rule.

### 5.2 Add a Custom Form Widget

1. Create a `.vue` file in the current `src/pages/Config/ConfigForm/CustomWidget/` directory. The proposed `forms/widgets/` directory has not been introduced; do not combine this work with directory migration.
2. Spread `fieldProps` according to the existing widget contract and use the form library's `rootSchema`, `schema`, `uiSchema`, `rootFormData`, and `curNodePath`. Read with `vueUtils.getPathVal` and write with `setPathVal`; nested-object updates must retain the existing deep-watch behavior.
   Note that `rootFormData` is the form library's internal editing copy. The outer `modelValue` synchronization watch observes only the reference; do not assume that in-place nested changes to the same object in Pinia will refresh an already mounted form. Recreating forms with keys for VM/Basic/Advanced changes does not replace a deliberate synchronization boundary. Component regression tests must assert both actual control state and store data, not store data alone.
3. Use Pinia `mapStores(useScenarioStore)` when cross-VM names, the current schema, or CAT actions are required. Put independently testable derivations in `domain/` without importing the form library.
4. Import the component in `ConfigForm.vue` and set `ui:field` at the appropriate field path in `uiSchema`; declare nested fields using the existing `virtio_devices` and `FEATURES.IVSHMEM` patterns. Configure `ui:title`, `ui:sortable`, and similar options at the same registration point.
5. Preserve template hierarchy, rendering order, class/id values, and interactions. Refactoring must not rename E2E selectors such as `.cpu_affinity`, `.VUART_SETTING`, `.IVSH_REGION`, `.virtio_*`, and `.slider-handle`.
6. Add component tests for initialization, item addition/removal, empty values, and write-back. When VM references or saved artifacts are involved, also add domain/golden tests and run E2E.

### 5.3 Add an Engine Method

See the [engine protocol](./engine-protocol.md) for the complete wire protocol, DTOs, and error rules. Implement methods in this order:

1. Add the Python implementation under `pyodide/`, set `__package__ = 'configurator.pyodide'`, and preserve the existing return content. Use `convert_result` for JSON results and return text results directly as strings.
2. Add a payload-unpacking function in `pyodide/dispatch.py` and register it in `METHOD_REGISTRY`. Explicitly index required fields; missing fields and unknown methods use the existing exception paths.
3. Register the actual return format in `ENGINE_RESPONSE_FORMATS` in `src/engine/pyodide/protocol.ts`. If the method needs the Scenario JSON Schema, add it to `SCHEMA_METHODS`; the client automatically injects `scenarioJsonSchema`.
4. Add request/response DTOs and the method to `EnginePort` in `src/engine/types.ts`, and map raw results to DTOs in `PyodideEngine`. Do not bypass DTOs and make pages parse private Python structures.
5. Add compatibility entries to `createLegacyPayload` and `PythonObject` only when legacy callers genuinely require them; new code should not use these as its primary interface.
6. In `tests/engine/pyodide.spec.ts`, use a fake dispatcher to verify method names, payloads, schema injection, parsing, and exception classification. Then add Python tests and the necessary integration/E2E coverage.

`dispatchSync` currently executes synchronously on the page thread, and the Promise returned by `request` does not move Python work into the background. New methods must not assume cancellation, parallelism, or Worker isolation; these belong to the unimplemented P5 phase.

## 6. Verification and Change Discipline

See the [README](../README.md) for general development commands and [section 8 of the refactoring plan](./refactoring-plan.md#8-verification-and-local-environment) for local WSL wrapper scripts and standard tool paths. Run at least the following after architectural changes:

```bash
# From misc/config_tools/configurator
yarn workspace acrn-configurator test
python -m unittest discover -s packages/configurator/tests -p "test_*.py"
cargo test --manifest-path packages/configurator/src-tauri/Cargo.toml
cargo fmt --check --manifest-path packages/configurator/src-tauri/Cargo.toml
```

Changes involving the real UI, native IPC, or the save flow also require a build and `test:e2e`; use `test:e2e:coverage` for the coverage release check. Test counts in these documents apply only to the recorded runs and do not replace regression testing for the current change.

All changes must continue to preserve the content and format of `scenario.xml`, `config_summary.rst`, and `launch_*.sh`, the XML schema, DOM class/id values, and interactions. Diffs must remain within the Configurator scope of the current task.
