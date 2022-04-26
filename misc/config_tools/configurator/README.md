# ACRN Configurator

This version is based on Tauri, WIP.

## Features

### Support Platforms

- [x] Linux (.deb, AppImage)
- [x] Windows 7,8,10 (.exe, .msi)


## Setting Up

### 1. Install System Dependencies

Please follow [this guide](https://tauri.studio/docs/getting-started/prerequisites)
to install system dependencies **(including yarn)**.

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
cd acrn-hypervisor/misc/config_tools
python3 -m pip install -r requirements.txt
cd configurator
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
cd misc/config_tools
python scenario_config/schema_slicer.py
python scenario_config/jsnoschema/convert.py
xmllint --xinclude schema/datachecks.xsd > schema/allchecks.xsd

python -m build

cd configurator
python thirdLib/manager.py install
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
`misc/config_tools/configurator/src-tauri/target/release/bundle/msi`
directory, the installer in the folder.
