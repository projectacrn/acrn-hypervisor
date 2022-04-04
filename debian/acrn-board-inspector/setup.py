"""
Out-of-tree setup.py for board-inspector and its associated libraries tweaked
to perform the required actions at setup time.

This enables the usage of dh_python3 even in absence of an upstream setup.py.
"""

import os
from setuptools import find_namespace_packages
from setuptools import setup

setup(
    name="acrn_board_inspector",
    version=os.environ["ACRNVERSION"],
    description="ACRN Board Inspector",
    long_description="acrn-board-inspector will collect all board related info to generate a board configuration file for ACRN.",
    url="https://projectacrn.org/",
    license="BSD-3-Clause",
    # add additional namespace acrn_board_inspector
    packages=[
        "acrn_board_inspector",
        "acrn_board_inspector.acpiparser",
        "acrn_board_inspector.acpiparser.aml",
        "acrn_board_inspector.cpuparser",
        "acrn_board_inspector.extractors",
        "acrn_board_inspector.inspectorlib",
        "acrn_board_inspector.legacy",
        "acrn_board_inspector.memmapparser",
        "acrn_board_inspector.pcieparser",
        "acrn_board_inspector.schema",
        "acrn_board_inspector.smbiosparser",
    ],
    package_dir={
        "acrn_board_inspector": "../../misc/config_tools/board_inspector",
        "acrn_board_inspector.acpiparser": "../../misc/config_tools/board_inspector/acpiparser",
        "acrn_board_inspector.acpiparser.aml": "../../misc/config_tools/board_inspector/acpiparser/aml",
        "acrn_board_inspector.cpuparser": "../../misc/config_tools/board_inspector/cpuparser",
        "acrn_board_inspector.extractors": "../../misc/config_tools/board_inspector/extractors",
        "acrn_board_inspector.inspectorlib": "../../misc/config_tools/board_inspector/inspectorlib",
        "acrn_board_inspector.legacy": "../../misc/config_tools/board_inspector/legacy",
        "acrn_board_inspector.memmapparser": "../../misc/config_tools/board_inspector/memmapparser",
        "acrn_board_inspector.pcieparser": "../../misc/config_tools/board_inspector/pcieparser",
        "acrn_board_inspector.schema": "../../misc/config_tools/board_inspector/schema",
        "acrn_board_inspector.smbiosparser": "../../misc/config_tools/board_inspector/smbiosparser",
    },
    package_data={
        "acrn_board_inspector.schema": ["*", "checks/*"],
    },

    # use namespace packages from board inspector 
    #packages=find_namespace_packages(
    #    where="../../misc/config_tools/board_inspector",
    #),
    #package_dir={"": "../../misc/config_tools/board_inspector"},
    ## add the standalone board_inspector.py file
    #py_modules=["board_inspector"],

    install_requires=[
        "lxml",
        "xmlschema"
    ],
    # use wrapper script to call board_inspector.py
    scripts=["acrn-board-inspector"],
)
