# Supporting libraries of ACRN configuration toolset

This package contains the libraries supporting ACRN configuration toolset, including:

* The manipulators and validators of scenario schemas or XMLs
* The generator of guest ACPI tables

The main objective of this package is to ease the import of the supporting libraries in the ACRN configurator which uses
a Python interpreter built in WebAssembly (WASM). This package is thus NOT intended to be used by users; invoke the
Python scripts directly if needed.
