# ACRN Configurator Refactoring Design and Implementation Plan

- Branch: `refine_configurator`
- Baseline: `fix/configurator-tauri2-maintenance` (Tauri 2 migration completed)
- Scope: `misc/config_tools/configurator/` (Vue 3 frontend, Pyodide engine, Tauri 2 Rust backend)
- Status: P0–P4 core changes implemented; P5 not implemented; architecture and protocol documentation completed

This document is both the design and the implementation task plan. Each phase (P0–P5) defines its goals, affected areas, acceptance criteria, and prohibited changes.

## Implementation Status Overview (2026-11-18)

| Phase | Current status | Main deviations / incomplete items |
| --- | --- | --- |
| P0 safety net | Implemented | Behavior snapshots and save-pipeline unit tests exist; complete golden comparisons of all downstream artifacts for shared / hybrid / partitioned scenarios do not |
| P1 platform and engine ports | Implemented | `PyodideEngine` exists, but stores/pages still primarily use the `PythonObject` compatibility facade; Promises do not provide background execution |
| P2 domain logic extraction | Implemented | CAT still normalizes and writes back `CACHE_REGION` once before saving |
| P3 Pinia and state management | Core implemented | `pages`/`forms` directory migration, session store, path aliases, and strict TS remain incomplete; compatibility globals such as runtime/systemInfo remain |
| P4 Rust modularization | Core implemented | `AppState`, modules, and locks are in place; errors remain `String`, without thiserror or `{code, message}` |
| P5 performance and experience | Not implemented | No Worker/CSP spike, cancellation, queue/cache, naive-ui consolidation, or undo/redo |
| Architecture and protocol docs | Implemented | Documents describe actual code and deviations without presenting target APIs as existing APIs |

See [architecture.md](./architecture.md) for the implemented structure and [engine-protocol.md](./engine-protocol.md) for the actual EnginePort, wire DTOs, error classification, and Python registry.

---

## 1. Current-State Diagnosis

Code size: approximately 4,000 lines in frontend `src/` (37 files), 600 lines in the Rust backend, and 12 Python modules on the Pyodide side.

| Problem | Evidence | Consequence |
| --- | --- | --- |
| God component owns all business logic | `packages/configurator/src/pages/Config.vue`: `saveScenario` (save pipeline), `vmNameChange` (cross-VM reference repair), `translateErrors` (error mapping), `updateLoadSchema` (schema derivation), `assignVMID` (VM renumbering) | Difficult to unit test; every change requires full regression |
| `window` used as dependency injection | `Config.vue` mounted installs `getSchemaData` / `getCurrentFormSchemaData` / `getCurrentScenarioData` / `getBoardData`; `src/lib/acrn.ts` installs `window.configurator`; `src/pyodide.js` installs `__dynamic__load__scenario__from__pyodide__` for Python callbacks | Implicit timing coupling; tests can only mock globals |
| Domain layer depends upward on UI library | CAT classes in `src/lib/acrn.ts` import `vueUtils` from `@lljj/vue3-form-naive` for path reads/writes | Domain logic cannot run independently of the form library |
| Two sources of truth + Proxy side effects | CAT maintains both `CATDB` and `scenario.hv.CACHE_REGION`, with implicit write-back through `Proxy.set`; merging occurs only during save | Unpredictable intermediate state; no reliable dirty checking or undo |
| Synchronous Python calls block | `PythonObject.api()` directly calls `pyimport(...).main(...)` synchronously | UI freezes during validation/generation; no cancellation or concurrency |
| Engine interface based on string conventions | `api('loadBoard', 'json', ...)` dynamically imports and manually calls `JSON.parse` | No typed contract; errors are raw strings |
| No Rust state management; single file | Every command recreates `Configurator::new()` and rereads disk; global `WORKING_FOLDER` duplicates a struct field; all errors are `String` | Read/write races, duplicated semantics, and string-only frontend errors |
| Missing engineering standards | `tsconfig.json` has `strict:false` and legacy `jsx:react`; only one `.ts` file; no ESLint/Prettier; both bootstrap-vue-3 and naive-ui; no Pinia | Type safety is ineffective and styles drift |
| Nonstandard path handling | Frontend uses `this.WorkingFolder + 'scenario.xml'`, depending on a trailing separator; Python uses a `path.replace('\\','/')` workaround | Fragile cross-platform behavior |

### Mandatory Constraints

1. **Byte-identical artifacts**: `scenario.xml`, `config_summary.rst`, and `launch_*.sh` are inputs to downstream ACRN builds.
2. **Existing tests must remain green**: baseline is 8 test files / 36 passing tests.
3. **Save semantics must not change**: write `scenario.xml` first; on validation failure, retain the editable working state and do not generate downstream artifacts; generate `config_summary.rst` and launch scripts only after validation succeeds; remove surplus `launch_*.sh` files; generate no launch scripts when there are no Post-launched VMs.
4. **Do not modify the XML schema or artifact formats**, rewrite UI interaction flows, or replace frameworks.
5. **Keep DOM structure and class/id values stable**: `tests/e2e/configurator.e2e.js` is tightly coupled to existing selectors (`.aTab`, `.cpu_affinity`, `.VUART_SETTING`, `.IVSH_REGION`, `.virtio_console` / `.virtio_network` / `.virtio_input` / `.virtio_gpu`, `.slider-handle`, `#newScenarioModal`, `.modal.show`, and others). Refactoring may move logic and module boundaries, but must not change template structure, class names, IDs, or component rendering order. Any necessary adjustment requires corresponding E2E selector updates and an explanation.
6. **Combined coverage threshold**: `test:coverage` combines Istanbul data from Vitest and native E2E, requiring at least 95% statements/branches/functions/lines over `src/**`. `test:e2e:coverage` only collects data. Before merging, browser-bundle coordinates must be remapped to original source using the source map embedded in the E2E data, avoiding duplicate counting with Vitest data.

---

## 2. Design Principles

1. **Schema as the only source of truth**: `schema/*.xsd` → JSON Schema → TS types / Python models.
2. **Unidirectional data flow**: UI → Store Action → pure domain function → Store State → UI. Engine and filesystem are replaceable ports (interfaces).
3. **Pure functions first**: CAT derivation, schema derivation, error mapping, and rename propagation all become `(input) => output`.
4. **Stable artifacts**: use golden-file comparisons to lock output during refactoring.
5. **Avoid excessive decomposition**: the frontend is small enough for **layers within one package**; do not split it into multiple npm packages or introduce micro-frontends, event buses, or CQRS.

---

## 3. Target Architecture

```text
views/        Welcome · Config · dialogs (read stores and dispatch actions only)
forms/        SchemaForm + widgets/ (CAT/VUART/IVSHMEM/Virtio)
stores/       workspace · scenario · validation · session (Pinia)
domain/       Pure TS; zero Vue / Tauri / Pyodide dependencies
engine/       EnginePort ← PyodideEngine (asynchronous RPC, Worker-ready)
platform/     FsPort · DialogPort · HistoryPort ← Tauri / WebMock
                  │ invoke (typed DTOs)
src-tauri/    commands/ · state.rs · error.rs · config.rs · fsutil.rs
```

Target directories (`packages/configurator/src/`):

```text
src/
  main.ts                 # Composition only: app.use + provide + start engine
  app/                    # App.vue, router, providers, bootstrap
  views/                  # Former pages/; components retain templates/interaction
  forms/
    SchemaForm.vue        # Sole wrapper around @lljj/vue3-form-naive
    uiSchema.ts           # Declarative ui:field registry
    widgets/              # CAT.vue / VUART.vue / ... (props + emit only)
  domain/
    scenario/  model.ts  factory.ts  commands.ts  normalize.ts
    references/ rename.ts  graph.ts        # Cross-VM reference integrity
    cat/       model.ts  derive.ts  masks.ts  toScenario.ts
    schema/    registry.ts  derive.ts      # Dynamic enums, Basic/Advanced split
    validation/ mapErrors.ts  types.ts
    artifacts/ pipeline.ts  plan.ts
  engine/
    types.ts              # EnginePort + DTO + EngineError
    pyodide/  client.ts  bootstrap.ts  protocol.ts
  platform/
    types.ts              # FsPort/DialogPort/HistoryPort
    tauri.ts  webMock.ts
  stores/    workspace.ts scenario.ts validation.ts session.ts
  types/     generated/scenario.d.ts   # Generated from JSON Schema
```

**Hard dependency rule**: `domain` must not import higher-level modules (`views` / `forms` / `stores` / `engine` / `platform`), Vue, Tauri APIs, or `@lljj/vue3-form-naive`.

---

## 4. Core Contracts

### 4.1 Scenario Store (replacing `window.*` and component data)

```ts
interface ScenarioState {
  board: BoardInfo | null        // Includes CAT_INFO and original XML
  schemas: FormSchemas | null    // Complete JSON Schemas returned by the engine
  doc: ScenarioDoc | null        // { hv, vm[] }, the only mutable document
  activeVmId: VmId               // -1 for HV
  dirty: boolean
  revision: number               // Monotonic; used to discard stale responses
}
```

- Use VM `@id` as the internal key, not name; name is only an XML artifact field.
- Route all form `@change` events through `updateActiveForm` instead of multiple emit layers; remove the rename-guessing logic based on `vmNameCache` in `ConfigForm.vue`.
- Replace hand-written `requestID` counters in `Board.vue` / `Scenario.vue` with the `revision` mechanism.
- Use getters/selectors for derived data (`activeSchema`, `vmsByLoadOrder`, `activeVm`) rather than computing it in components.

### 4.2 EnginePort (replacing `PythonObject`)

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

interface ValidationReport { syntactic: Issue[]; semantic: Issue[] }
interface Issue { path: string[]; message: string; code?: string }
class EngineError extends Error { kind: 'structure' | 'validation' | 'runtime' | 'io' }
```

- All methods are asynchronous and return structured DTOs; branches no longer depend on error-message strings.
- Consolidate the nine one-file-one-main Python scripts into a single `dispatch(method, payload)` entry point with registry routing.
- Pass schema JSON to Python as an **argument**, removing the reverse global dependency `from js import __dynamic__load__scenario__from__pyodide__`.
- Remove `eval(sorted_func)` from `loadBoard.py` and replace it with allowlisted sort-key functions.

### 4.3 PlatformPort (replacing `window.configurator` filesystem/history methods)

```ts
interface FsPort {
  readText(p: string): Promise<string>
  writeTextAtomic(p: string, content: string): Promise<void>
  list(p: string, recursive?: boolean): Promise<DirEntry[]>
  removeFile(p: string): Promise<void>
  createDir(p: string, recursive?: boolean): Promise<void>
  join(...parts: string[]): string
}
```

- Promote the browser mock from `tests/mock.js` into a supported `platform/webMock.ts` implementation, and inject Tauri or WebMock from `main.ts` according to `isTauri`.
- Consolidate path joining into `join` or Rust; prohibit string addition such as `WorkingFolder + 'scenario.xml'`.

### 4.4 Rust Backend Modularization

```text
src-tauri/src/
  main.rs            # Builder + manage(state) + generate_handler
  error.rs           # thiserror → serialized { code, message }
  state.rs           # AppState { config: Mutex<UserConfig>, workspace: Mutex<Workspace> }
  config.rs          # UserConfig I/O and corruption protection (preserve semantics/tests)
  fsutil.rs          # Atomic replacement, read_dir (retain Windows MoveFileExW branch)
  commands/ fs.rs · history.rs · workspace.rs
```

- Use `tauri::State<AppState>` instead of rereading disk in every command; perform history read-modify-write under one lock.
- Eliminate duplicate state in the `WORKING_FOLDER` static and struct field.
- Preserve existing Rust unit-test semantics: atomic writes leave no temporary files, permissions are preserved, corrupt configuration is not overwritten, missing fields fall back to defaults, and Board replacement treats paths literally.

---

## 5. Focused Designs

### 5.1 CAT (`src/lib/acrn.ts`, the most complex area)

Split into three pure functions and one store slice, **removing all Proxies**:

```ts
// domain/cat/derive.ts — side-effect free; snapshot input, view-model output
deriveCatView(input: { doc; board; schema }): CatView   // { errorMsg, regions[], summary }
// domain/cat/toScenario.ts
buildCacheRegion(view: CatView): CacheRegion | null
// domain/cat/masks.ts
hexToRange(hex: string, max: number): [number, number]
rangeToHex(range: [number, number], max: number): string
```

- Make **`doc.hv.CACHE_REGION` the single source of truth** and remove the `CATDB` shadow array.
- Widget edits dispatch `setClosMask(...)` directly to the document; saving no longer needs a merge step.
- Switch correction (RDT/CDP/VCAT mutual exclusion) and CPU-affinity checks become explicit return values; required persisted corrections are performed by store actions instead of implicit Proxy getters.
- Truncation/padding in `hexToRange` is existing behavior and **must be locked by snapshot tests**; do not “fix” it incidentally during refactoring.

### 5.2 Cross-VM Reference Integrity

The current `vmNameChange` handles only IVSHMEM and VUART, while CAT policy `VM` fields and launch-script filenames are two additional implicit references. Replace this with declarative reference rules:

```ts
const VM_NAME_REFERENCES: RefRule[] = [
  { path: 'hv.FEATURES.IVSHMEM.IVSHMEM_REGION[].IVSHMEM_VMS.IVSHMEM_VM[].VM_NAME',
    onRemove: 'drop-entry', pruneWhen: 'fewer-than-2-siblings' },
  { path: 'hv.vuart_connections.vuart_connection[].endpoint[].vm_name',
    onRemove: 'drop-connection' },
  { path: 'hv.CACHE_REGION.CACHE_ALLOCATION[].POLICY[].VM', onRemove: 'drop-entry' },
]
renameVm(doc, oldName, newName)
removeVm(doc, vmName)
```

Adding a reference point should require one table entry rather than another review of the entire component.

### 5.3 Save Pipeline and Validation

Replace `stepDone` counters with an explicit pipeline while **strictly preserving constraint 3 in section 1**:

```text
normalize(assignVmIds) → populateDefaults → writeScenario → validate
  → configSummary → launchScripts → pruneStaleScripts
```

- Return `SaveReport { steps: StepReport[] }`, where `StepReport = { id, status: 'ok'|'failed'|'skipped', detail? }`.
- Generate result-dialog text in the view from the report instead of maintaining `msg` / `errmsg` arrays in the component.
- Isolate validation error mapping in `domain/validation/mapErrors.ts`: `Issue.path` → `{ vmId, fieldPath }`, shared by TabBox badges and field-level messages.
- Schedule validation through the store and use `revision` to discard stale results.

---

## 6. Engineering Standardization

| Item | Target |
| --- | --- |
| TypeScript | Use `.ts` for all new modules and make them `strict`-compatible; remove `jsx: react` from `tsconfig.json`; raise `strict` incrementally |
| Type generation | JSON Schema → `types/generated/scenario.d.ts` (commit generated output) |
| Lint/format | ESLint (flat) + `eslint-plugin-vue` + Prettier + dependency-direction rules; **no repository-wide formatting PR**, only new/changed files |
| UI library | Use naive-ui for new code; replace bootstrap-vue-3 incrementally, not forcibly in this work |
| Path aliases | `@domain/*`, `@engine/*`, `@platform/*` to avoid long `../../../` chains |
| Build | Separate asset generation, frontend build, and Tauri build; `vite build` should not implicitly trigger `tauri build` through the current `closeBundle` side effect — **recorded only; build entry points are not changed on this branch** |
| Tests | Pure domain unit tests → store integration → components → E2E; add golden artifact comparisons |
| Documentation | `docs/architecture.md` (layers and dependency rules), `docs/engine-protocol.md` (DTOs), and this document |

---

## 7. Phased Implementation

### P0 Safety Net (implemented, with coverage deviations)

- Add golden tests with fixed inputs (shared / hybrid / partitioned, including complex CAT, IVSHMEM, and VUART scenarios), snapshotting `convertScenarioToXML` and save-pipeline artifact plans.
- Add snapshot tests for existing `hexToRange` / `rangeToHex` / `translateErrors` / `vmNameChange` behavior, including seemingly odd behavior.
- Introduce test-directory conventions: `tests/domain/`, `tests/golden/`.
- Acceptance: all new tests pass with no regression in the original 36 tests.
- Actual delivery: Scenario XML, CAT mask/view, validation errors, VM-reference snapshots, and save-pipeline unit tests are covered. Byte-level golden comparisons of `config_summary.rst` / all `launch_*.sh` for the three Scenario types have not been established; current snapshots must not be considered a complete artifact safety net.

### P1 Platform and Engine Port Abstraction (implemented, compatibility entries retained)

- Add `platform/` and `engine/`; turn `src/lib/acrn.ts` into a thin forwarding adapter while temporarily retaining `window.configurator`.
- Unify Python under a `dispatch` entry point, inject schemas as parameters, and remove `eval`.
- Initially wrap synchronous interfaces in Promises with **unchanged behavior**; leave UI freezing to P5.
- Acceptance: all tests pass; add `engine`/`platform` unit tests with mocked Tauri invoke.
- Actual delivery: nine-method Python registry, schema parameter injection, sort allowlist, EnginePort, Tauri/WebMock, and their unit tests are implemented. `window.configurator` has been removed, but stores and pages still primarily call through `lib/acrn.ts` / `PythonObject`; `PyodideEngine` is not yet the sole entry point. The schema getter remains as a TS-client compatibility fallback, but Python no longer depends on it.

### P2 Domain Logic Extraction (implemented)

- Order: `cat` → `validation/mapErrors` → `references` → `schema/derive` → `artifacts/pipeline`.
- After each extraction, remove the corresponding method from `Config.vue` while keeping golden and existing tests green.
- Acceptance: `Config.vue` no longer implements CAT, reference repair, error mapping, or save-step orchestration; domain modules have independent unit tests.
- Actual delivery: five domain-module groups and independent tests are implemented; components retain only limited forwarding and presentation. To preserve legacy save behavior, the pipeline still normalizes and writes back `CACHE_REGION` through `buildCacheRegion(getCatView())` before default population; save-time merging has not been completely removed.

### P3 Introduce Pinia and Remove Window Globals (core implemented)

- Add `scenario` / `workspace` / `validation` stores and migrate components to store reads/writes.
- Remove the four getters such as `window.getSchemaData` and `window.configurator`.
- Use TS for new code and remove `jsx: react`.
- Acceptance: all tests pass; `grep -rn "window\.\(configurator\|getSchemaData\|getCurrentScenarioData\|getBoardData\|getCurrentFormSchemaData\)" src/` returns no matches.
- Actual delivery: three stores, revision-based stale-response protection, and component integration are complete; the five legacy globals above and `jsx: react` have been removed. `window.pyodide`, the schema-getter fallback, `window.systemInfo`, and `window.isDev` remain; no session store was added. `strict` remains `false`, and directories/path aliases have not been fully migrated as described in sections 3/6.

### P4 Rust Modularization (core implemented, structured errors incomplete)

- Split files according to 4.4 and introduce `AppState` and structured errors.
- Preserve all existing Rust unit-test semantics.
- Acceptance: `cargo test` passes (retaining the original five test semantics), and `cargo fmt --check` passes.
- Actual delivery: commands/state/config/fsutil are split; shared `AppState`, history write locks, and a single Workspace state are implemented; there are now seven Rust tests. For frontend compatibility, `error.rs` still uses `Result<T, String>` without thiserror or `{code, message}`, so the structured-error goal is incomplete.

### P5 Performance and Experience (not implemented; optional; spike required first)

- Move Pyodide into a Web Worker with request cancellation. Risks: Worker/WASM loading and CSP under the Tauri 2 custom protocol.
- If that is not viable, fall back to a serial task queue, validation-result caching, and a generation overlay.
- Consolidate naive-ui and add undo/redo.
- **P5 is not mandatory on this branch**.
- `dispatchSync` still executes synchronously on the page thread; the Promise facade provides no cancellation, parallelism, or background execution, and `dispose()` remains a no-op.

### Architecture and Protocol Documentation (implemented)

- Add `docs/architecture.md`: actual layers, dependency rules, responsibilities and data flows, plus guides for VM references, custom widgets, and engine-method extensions.
- Add `docs/engine-protocol.md`: EnginePort, wire methods, raw/adapted DTOs, error classification, compatibility facades, and the Python dispatch registry.
- Record implementation status, deviations, remaining work, and measured verification in this document.
- Development and test commands in `README.md` did not change in this phase, so no unrelated edits were made.

### Remaining Work

1. Evaluate and complete full-artifact golden comparisons for all Scenario types, especially summaries and launch scripts.
2. Make stores/new page code depend directly on EnginePort and gradually reduce `PythonObject` and compatibility globals while preserving existing artifacts and error presentation.
3. Decide whether to complete directory migration, session store, path aliases, generated types, stricter TS, and lint/format configuration; these engineering goals are currently unimplemented.
4. Design a Rust structured-error migration that preserves current frontend error semantics; the error contract in section 4.4 cannot currently be described as implemented.
5. P5 must begin with a Worker/WASM/CSP spike under the Tauri 2 custom protocol; if not viable, use a serial queue, caching, and generation overlay.

---

## 8. Verification and Local Environment

The local machine is Windows 11 + WSL Ubuntu 24.04, and the **complete toolchain is installed**. All verification runs inside WSL (`node_modules` contains Linux packages; Windows lacks `@rollup/rollup-win32-x64-msvc`, so do not run `npx`/`yarn` or reinstall with `yarn install` on Windows).

The toolchain uses standard locations (legacy isolated paths are fallback only, as described below):

| Tool | Standard location |
| --- | --- |
| Node 22 / npm | `/usr/local/bin/node` (official tarball installed under `/usr/local`, the standard FHS location; the NodeSource apt repository was unreachable and was not used) |
| Yarn Classic 1.22 | `/usr/local/bin/yarn` (provided by `corepack enable`; the `packageManager` field in `package.json` pins the version) |
| Rust / cargo | rustup defaults under `~/.cargo/bin` and `~/.rustup`; `~/.profile` and `~/.bashrc` source `~/.cargo/env`, making tools available in login shells |
| Cargo build output | Project default `src-tauri/target` (no `CARGO_TARGET_DIR` override) |
| Python venv | `~/.local/acrn-configurator-venv` |
| WebKitWebDriver / tauri-driver | `/usr/bin/WebKitWebDriver`, `/usr/local/bin/tauri-driver` |

Earlier isolated installation directories were removed, reclaiming approximately 4.9 GB: `~/.local/node-v22.14.0`, `~/.local/node-v22.22.2`, `~/.local/acrn-cargo`, `~/.local/acrn-rustup`, `~/.local/acrn-target`, and `~/.local/opt/`. `~/.local/` now retains only `bin`, `share`, and the venv still in use. Standard reinstallation uses the official Node tarball under `/usr/local`, rustup defaults (`~/.cargo` + `~/.rustup`), and `tauri-driver` in `/usr/local/bin`.

Wrapper scripts (installed under `~/.local/bin/`, outside the repository):

```bash
# Frontend unit/component tests (Vitest)
MSYS_NO_PATHCONV=1 wsl -d Ubuntu-24.04 -- bash -lc 'bash ~/.local/bin/acrn-vitest'

# Rust unit tests
MSYS_NO_PATHCONV=1 wsl -d Ubuntu-24.04 -- bash -lc 'bash ~/.local/bin/acrn-cargo-test'

# Run yarn in the configurator workspace (corepack, Yarn Classic 1.22)
MSYS_NO_PATHCONV=1 wsl -d Ubuntu-24.04 -- bash -lc 'bash ~/.local/bin/acrn-yarn workspace acrn-configurator test'
```

Full build and end-to-end tests (final regression after refactoring):

```bash
# Full build: schemas, wheel, thirdLib assets, frontend, and Tauri packages
cd ~/acrn-hypervisor
make configurator

# End-to-end tests (WSLg display, tauri-driver + WebKitWebDriver)
cd ~/acrn-hypervisor/misc/config_tools/configurator/packages/configurator
DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 \
TAURI_DRIVER=$(command -v tauri-driver) WEBKIT_WEBDRIVER=/usr/bin/WebKitWebDriver \
  node --test --test-timeout=1800000 tests/e2e/configurator.e2e.js
```

Baseline (measured at branch start): Vitest **8 test files / 36 tests passed**; `cargo test` **5 tests passed**.

Measured during the documentation phase (2026-11-18, after P0–P4 code):

| Command actually executed | Result |
| --- | --- |
| `MSYS_NO_PATHCONV=1 wsl -d Ubuntu-24.04 -- bash -lc 'bash ~/.local/bin/acrn-vitest'` | **21 test files / 114 tests passed, 0 failed** |
| `MSYS_NO_PATHCONV=1 wsl -d Ubuntu-24.04 -- bash -lc 'bash ~/.local/bin/acrn-cargo-test'` | **7 tests passed, 0 failed** |
| `cargo fmt --check --manifest-path packages/configurator/src-tauri/Cargo.toml` from `misc/config_tools/configurator` | **Passed** |

That documentation phase did not run `make configurator`, Python unittest, native E2E, or E2E coverage, so it made no passing claim for the full build, Python tests, real WebDriver workflows, or 95% coverage. Disk-full, permission-denied, and validation-failure messages in Vitest output came from error-path tests; the final result was all tests passing.

Measured after all phases (2026-11-18, including independent review):

| Verification | Result |
| --- | --- |
| Vitest (independent-review rerun) | **21 test files / 119 tests passed, 0 failed** |
| `cargo test` (independent-review rerun) | **7 tests passed, 0 failed** |
| `cargo fmt --check` | Passed |
| Python unittest (`python -m unittest discover -s tests -p "test_*.py"`, project venv) | **5 tests passed** |
| Full `make configurator` build | Passed; produced executable, DEB, RPM, and AppImage |
| Native WebDriver E2E (`tests/e2e/configurator.e2e.js`) | **57 tests passed, 0 failed, 0 skipped**, including 30 bundled Scenario round trips |

Combined coverage completion (2026-11-18):

- Added `@vitest/coverage-istanbul` and `scripts/coverage.cjs`; the latter remaps native E2E bundle coverage, merges through nyc, generates reports, and checks all four 95% thresholds.
- Added domain-boundary, save-error-path, ControlBar, JSON2XML, WebMock/IPC, Pyodide error-mapping, and Pinia stale-response/concurrency tests. No files were excluded and no thresholds were lowered to inflate coverage.
- Combined result: lines **97.86%**, statements **97.45%**, functions **96.81%**, branches **95.48%**; Vitest **28 test files / 184 tests passed**.
- The unified entry point `yarn workspace acrn-configurator test:coverage` completed successfully (exit code 0): Vitest **28 test files / 184 tests**, native WebDriver **57 tests passed, 0 failed, 0 skipped**. With existing collected data, use `coverage:report` to rebuild reports only, or run `node scripts/coverage.cjs check` in the package directory to rebuild reports and check thresholds. The previous standalone 86.48% native-E2E line coverage is no longer a threshold; only the combined metric above applies.

Note: the 30 round trips verify semantic Scenario equivalence and successful artifact generation, not byte-for-byte golden comparison of every `scenario.xml` / `config_summary.rst` / `launch_*.sh`. Independent TypeScript checking (`tsc` / `vue-tsc`) was not run because the workspace does not install `typescript`; this remains a future engineering item.

When invoking WSL across environments, add `MSYS_NO_PATHCONV=1` and avoid unquoted `$PATH` (the outer shell may expand it early); prefer the wrapper scripts above.

Delivery reports must state exactly which verification ran, its result, and what was not run.

---

## 9. Explicit Non-Goals

- Do not rewrite UI/interaction flows, replace frameworks (retain Vue 3), or replace Rust with a Node sidecar.
- Do not replace Pyodide with a native Python/Rust engine in this work—`EnginePort` only makes that possible later.
- Do not split into multiple npm packages or introduce micro-frontends, event buses, or CQRS.
- Do not modify the XML schema or artifact formats.
- Do not create a repository-wide formatting PR or modify repository content unrelated to Configurator.
