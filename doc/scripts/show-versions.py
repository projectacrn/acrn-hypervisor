#!/usr/bin/env python3
#
# Copyright (c) 2018, Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Show installed versions of doc building tools

import subprocess

print ("breathe          version: " +
          __import__("breathe").__version__ + "\n" +
       "docutils         version: " +
          __import__("docutils").__version__ + "\n" +
       "sphinx           version: " +
          __import__("sphinx").__version__ + "\n" +
       "sphinx_rtd_theme version: " +
          __import__("sphinx_rtd_theme").__version__ + "\n" +
       "doxygen          version: " +
          subprocess.check_output(["doxygen", "-v"]).decode("utf-8"))
