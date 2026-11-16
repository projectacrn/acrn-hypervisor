# ACRN Configurator

This version is based on Tauri, WIP.

## Features

### Support Platforms

- [x] Linux (.deb, AppImage)
- [x] Windows 10, 11 (.exe, .msi)

## Setting Up

### 1. Install System Dependencies

1. Please follow [this guide](https://v2.tauri.app/start/prerequisites/)
to install system dependencies.
2. Download and install [Nodejs](https://nodejs.org/en/download/).
3. Please follow [this guide](https://yarnpkg.com/lang/en/docs/install/) to install yarn.

#### Linux

In Linux, make sure your already install `git`, `python3`(version>=3.6) and `python3-venv` library,

```bash
$ sudo apt install git python3 python3-venv
# check python3 version
$ python3 --version
Python 3.8.10
```

#### Windows

[Chocolatey](https://chocolatey.org/) is a package manager tool for windows,
you can use `choco install xsltproc` to install `xsltproc` package,
which provide `xmllint` command.

Make sure your system have python which version>3.6,
you can check it by following command line:

```bash
$ python --version
Python 3.8.10
```

If your system doesn't have git and python, you can install it by
`choco install git python3`.

### 2. Clone Project And Install Project Dependencies.

#### Linux

```bash
git clone https://github.com/projectacrn/acrn-hypervisor
cd acrn-hypervisor/misc/config_tools/configurator
python3 -m pip install -r requirements.txt
yarn
```

#### Windows

Similar to Linux, in the Windows environment,
you need replace the command line `python3` with `python`.

### 3. How To Build

#### Linux

Run this command in the acrn-hypervisor directory.

```shell
make configurator
```

#### Windows

Run following command in the 'acrn-hypervisor' directory.

```shell
cd misc\config_tools
python scenario_config\schema_slicer.py
python scenario_config\jsonschema\converter.py
xmllint --xinclude schema\datachecks.xsd > schema\allchecks.xsd

python -m build
del configurator\packages\configurator\thirdLib\acrn_config_tools-3.0-py3-none-any.whl

cd configurator
python packages\configurator\thirdLib\manager.py install
yarn
yarn build
```

### 4. How To Run

#### Linux

Run this command in the acrn-hypervisor directory.

```shell
sudo apt install ./build/acrn-configurator_*.deb
acrn-configurator
```

#### Windows

You can find installer under the
`misc\config_tools\configurator\packages\configurator\src-tauri\target\release\bundle\msi`
directory, the installer in the folder.

## Testing

Run the JavaScript unit and rendered component tests from this directory:

```shell
yarn workspace acrn-configurator test
yarn workspace acrn-configurator test:component
python3 -m unittest discover -s packages/configurator/tests -p test_manager.py
cargo test --manifest-path packages/configurator/src-tauri/Cargo.toml
```

The native end-to-end suite requires Linux, a release build of the application,
`tauri-driver`, and WebKitWebDriver. On Debian/Ubuntu, the native driver is
provided by the `webkit2gtk-driver` package. Install `tauri-driver` with Cargo.
Build the application and its generated schema assets by running
`make configurator` from the repository root, then run the suite from this
directory:

```shell
yarn workspace acrn-configurator test:e2e
```

The suite starts the real Tauri application in an isolated configuration and
working directory. It discovers every bundled Board and Scenario XML under
`misc/config_tools/data`, checks load/save/reload semantic equivalence, and
verifies that an invalid Scenario reports a validation error. Set
`TAURI_E2E_BINARY`, `TAURI_DRIVER`, or `WEBKIT_WEBDRIVER` to override the default
binary paths. Screenshots, driver logs, and failure diagnostics are written to
`packages/configurator/build/e2e-artifacts/`.

To measure native E2E coverage, build a separate instrumented binary and run
the coverage-enabled suite:

```shell
VITE_COVERAGE=true make configurator
yarn workspace acrn-configurator test:e2e:coverage
```

The coverage command enforces 95% statement, branch, function, and line
coverage for the instrumented frontend sources. Text, HTML, and JSON summaries
are written to `packages/configurator/build/e2e-artifacts/coverage-report/`.
Rebuild without `VITE_COVERAGE` before producing a release artifact.
