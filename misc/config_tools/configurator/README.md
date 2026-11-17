# ACRN Configurator

ACRN Configurator is a desktop application for creating and editing ACRN
configurations. It uses Vue 3 for the user interface and Tauri 2 for native file
access, window management, and packaging.

The Configurator can:

- import a Board XML file produced by ACRN Board Inspector;
- create a Shared, Partitioned, or Hybrid Scenario, or import an existing one;
- edit hypervisor and VM settings through Basic and Advanced parameter views;
- validate the Scenario and save it as `scenario.xml`;
- generate `config_summary.rst` and launch scripts for valid configurations.

For a task-oriented guide, see the
[ACRN Configurator Tool tutorial](../../../doc/tutorials/acrn_configurator_tool.rst).

## Supported platforms

- Linux: Debian packages (`.deb`); AppImage and RPM bundles are also produced
  when their build tools are available.
- Windows 10 and 11: MSI and NSIS installers when built on Windows.
- The native WebDriver test suite is Linux-specific and uses WebKitGTK.

## Development setup

### 1. Install prerequisites

Install the following tools before building from source:

| Tool | Requirement |
| --- | --- |
| Git | Required by the build to determine the package version |
| Python | 3.6 or newer, with `venv` and `pip` |
| Node.js | Node.js 22 LTS is recommended |
| Yarn | Yarn Classic 1.22 |
| Rust | Rust 1.90 or newer, installed with `rustup` |
| Tauri dependencies | See the [Tauri 2 prerequisites](https://v2.tauri.app/start/prerequisites/) |

On Debian or Ubuntu, install the native build dependencies with:

```bash
sudo apt update
sudo apt install -y \
  build-essential curl wget file git libxml2-utils \
  libssl-dev libgtk-3-dev librsvg2-dev libwebkit2gtk-4.1-dev \
  libayatana-appindicator3-dev libxdo-dev \
  python3 python3-venv python3-pip
```

`xmllint`, supplied by `libxml2-utils`, is required to generate the combined
schema. On Windows, follow the Tauri 2 Windows prerequisites and install Python,
Git, and `xsltproc` (which provides `xmllint`), for example with Chocolatey:

```powershell
choco install git python3 xsltproc
```

### 2. Clone and install project dependencies

Using a Python virtual environment avoids modifying the system Python
installation. Keep the environment activated when building or testing.

```bash
git clone https://github.com/projectacrn/acrn-hypervisor.git
cd acrn-hypervisor/misc/config_tools/configurator
python3 -m venv "$HOME/.venvs/acrn-configurator"
source "$HOME/.venvs/acrn-configurator/bin/activate"
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
yarn install --frozen-lockfile
```

On Windows, use `python` instead of `python3` and activate the virtual
environment with the appropriate script under its `Scripts` directory.

## Build and run

### Build a Debian package on Linux

Run the project-standard target from the repository root, with the Python
virtual environment still activated:

```bash
cd ../../..
make configurator
```

This target generates the schema assets, builds the `acrn-config-tools` wheel,
installs frontend third-party assets, builds the Vue application, and invokes
the Tauri 2 bundler. The resulting package is copied to:

```text
build/acrn-configurator-<version>.deb
```

Install and launch it with:

```bash
sudo apt install ./build/acrn-configurator-*.deb
acrn-configurator
```

Tauri's other Linux bundles remain under
`misc/config_tools/configurator/packages/configurator/src-tauri/target/release/bundle/`.

### Run in development mode

Run one full build after cloning so that generated schemas and third-party
assets are present. Then start Vite and the Tauri development application from
this directory:

```bash
yarn dev
```

Repeat `make configurator` after changing schema sources or generated
third-party assets. Use `yarn build` for a direct frontend and Tauri release
build when those generated inputs are already current.

### Build on Windows

From the repository root, generate the schemas and Python wheel, refresh the
frontend assets, and invoke the Tauri build:

```bat
cd misc\config_tools
python scenario_config\schema_slicer.py
python scenario_config\jsonschema\converter.py
xmllint --xinclude schema\datachecks.xsd > schema\allchecks.xsd
python -m build
if exist configurator\packages\configurator\thirdLib\acrn_config_tools-3.0-py3-none-any.whl del configurator\packages\configurator\thirdLib\acrn_config_tools-3.0-py3-none-any.whl

cd configurator
python packages\configurator\thirdLib\manager.py install
yarn install --frozen-lockfile
yarn build
```

Installers are written below
`packages\configurator\src-tauri\target\release\bundle\msi` and
`packages\configurator\src-tauri\target\release\bundle\nsis`.

## Configuration workflow and save behavior

1. **Select a working folder.** Start a new configuration or open an existing
   one. Starting a new configuration removes previous Board, Scenario, and
   launch-script files in that folder after confirmation; use a separate folder
   for each independent configuration.
2. **Import a Board.** Select a Board XML from the history list or browse for a
   file. The source file is preserved and a copy is stored in the working
   folder with a `.board.xml` suffix.
3. **Create or import a Scenario.** The selected Scenario is loaded into the
   editor. Creating or importing a Scenario does not save it immediately.
4. **Edit and save.** Adjust hypervisor and VM settings, then click **Save
   Scenario and Launch Scripts**.

Saving deliberately preserves your work even when validation fails:

- **Valid Scenario:** writes `scenario.xml`, generates `config_summary.rst`,
  and generates one launch script for each post-launched VM. Obsolete generated
  launch scripts are removed after the replacements are written successfully.
- **Invalid Scenario:** still writes the current data to `scenario.xml` and
  displays the validation errors. It does not generate or update the
  configuration summary or launch scripts, and it does not remove existing
  launch scripts. Correct the errors and save again to generate those artifacts.
- **File-system failure:** reports `Scenario xml save failed` when the Scenario
  itself cannot be written. This is distinct from a validation failure.

The source Board and Scenario files selected outside the working folder are
never overwritten; saves target the working folder's own `scenario.xml`.

## Unit and component tests

Run these commands from `misc/config_tools/configurator` with the Python virtual
environment activated:

```bash
yarn workspace acrn-configurator test
yarn workspace acrn-configurator test:component
python -m unittest discover -s packages/configurator/tests -p "test_*.py"
cargo test --manifest-path packages/configurator/src-tauri/Cargo.toml
cargo fmt --check --manifest-path packages/configurator/src-tauri/Cargo.toml
```

- `test` runs all JavaScript unit tests and rendered Vue component tests.
- `test:component` runs only the rendered component suite.
- The Python tests cover third-party download and archive handling.
- The Rust tests cover native history, file replacement, and Board path logic.

## Native end-to-end tests

The E2E suite drives the real Tauri application through `tauri-driver` and
WebKitWebDriver. It does not replace application methods or infer native
coverage from jsdom tests.

### Prerequisites

- Linux with a graphical session, such as a desktop, WSLg, or an X virtual
  framebuffer;
- a current non-instrumented release binary built with `make configurator`;
- `tauri-driver` installed with Cargo;
- WebKitWebDriver, provided on Debian/Ubuntu by `webkit2gtk-driver`.

Install the drivers with:

```bash
sudo apt install -y webkit2gtk-driver
cargo install tauri-driver --locked
```

Run the suite from `misc/config_tools/configurator`:

```bash
yarn workspace acrn-configurator test:e2e
```

The suite uses temporary home, configuration, and working directories. File and
folder chooser interactions are automated at the native IPC boundary; Board and
Scenario discovery, validation, and writes still use the real backend. No
manual interaction with an operating-system file dialog is required.

Coverage includes common configuration and VM workflows, device and CAT
editing, error reporting, invalid-Scenario saves, reloads, and every bundled
Board/Scenario XML round trip under `misc/config_tools/data`.

Screenshots, driver logs, and failure diagnostics are written to:

```text
packages/configurator/build/e2e-artifacts/
```

The following environment variables override executable discovery:

| Variable | Purpose | Default |
| --- | --- | --- |
| `TAURI_E2E_BINARY` | Configurator application under test | `packages/configurator/src-tauri/target/release/acrn-configurator` |
| `TAURI_DRIVER` | Tauri WebDriver adapter | `tauri-driver` |
| `WEBKIT_WEBDRIVER` | Native WebKit driver | `WebKitWebDriver` |

### Measure native E2E coverage

Instrumentation and coverage collection are separate steps. Build an
instrumented application from the repository root, then run the coverage suite:

```bash
cd ../../..
VITE_COVERAGE=true make configurator
cd misc/config_tools/configurator
yarn workspace acrn-configurator test:e2e:coverage
```

`VITE_COVERAGE=true` enables Istanbul instrumentation at build time.
`test:e2e:coverage` collects coverage from the running native application and
enforces at least 95% statement, branch, function, and line coverage for the
frontend sources. Text, HTML, and JSON summaries are written to
`packages/configurator/build/e2e-artifacts/coverage-report/`.

**Rebuild without `VITE_COVERAGE` before producing a release artifact.**

## Continuous integration packages

The [Configurator workflow](../../../.github/workflows/configurator.yml) runs
for pushes and pull requests that change the Configurator, its configuration
schemas, or its packaging inputs. It can also be started manually with **Run
workflow** in GitHub Actions.

The workflow runs the JavaScript and Python tests, builds on Ubuntu 22.04 and
Windows 2022, and uploads these artifacts for 30 days:

- `configurator-linux`: Debian, RPM, and AppImage packages;
- `configurator-windows`: MSI and NSIS installers.

Download artifacts from the completed workflow run on the repository's
**Actions** page. These CI packages are unsigned development builds, not signed
release installers.
