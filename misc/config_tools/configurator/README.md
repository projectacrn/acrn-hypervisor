# ACRN Configurator

This version is based on Tauri, WIP.

## Features

### Support Platforms

- [x] Linux (.deb, AppImage)
- [x] Windows 7,8,10 (.exe, .msi)
- [x] macOS (.app, .dmg)


## Setting Up

### 1. Install System Dependencies

Please follow [this guide](https://tauri.studio/docs/getting-started/prerequisites)
to install system dependencies **(including yarn)**.

In Windows, [chocolatey](https://chocolatey.org/) is a Windows package manager,
you can use `choco install xsltproc` to install `xsltproc` package,
which provide `xmllint` command.

### 2. Clone Project And Install Project Dependencies.

#### Linux

```bash
sudo apt install git
git clone https://github.com/projectacrn/acrn-hypervisor
cd acrn-hypervisor/misc/config_tools/configurator
python3 -m pip install -r requirements.txt
yarn
```

#### Windows && macOS

Similar to Linux.

On macOS, you may need to install git and python3 via `brew`.

In the Windows environment maybe you need to install git and python3 via chocolatey or manually,
and replace the command line `python3` with `py -3`.

### 3. How To Build

#### Linux

Run this command in the acrn-hypervisor directory.

```shell
make configurator
```

#### Windows/macOS

Run following command in the 'acrn-hypervisor' directory.

```shell
cd misc/config_tools
python scenario_config/schema_slicer.py
python scenario_config/xs2js.py
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

#### Windows/macOS

You can find msi(Windows)/dmg(macOS) folder under the
`misc/config_tools/configurator/src-tauri/target/release/bundle`
directory, the installer in the folder.
