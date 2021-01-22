# Copyright (C) 2018 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# This script takes a Kconfig and a defconfig file, and expands it to a .config
# with all default values listed explicitly.

import sys
import os

# Kconfiglib: Copyright (c) 2011-2018, Ulf Magnusson
# SPDX-License-Identifier: ISC
# Refer to scripts/kconfig/LICENSE.kconfiglib for the permission notice.
import kconfiglib

def usage():
    sys.stdout.write("%s: <Kconfig file> <path to .config>\n" % sys.argv[0])

def main():
    if len(sys.argv) < 3:
        usage()
        sys.exit(1)

    target_board = os.environ['BOARD']
    target_scenario = os.environ['SCENARIO']

    kconfig_path = sys.argv[1]
    if not os.path.isfile(kconfig_path):
        sys.stderr.write("Cannot find file %s\n" % kconfig_path)
        sys.exit(1)

    kconfig = kconfiglib.Kconfig(kconfig_path)
    defconfig_path = kconfig.defconfig_filename
    if not defconfig_path or not os.path.isfile(defconfig_path):
        sys.stderr.write("No defconfig found for BOARD %s on SCENARIO %s.\n" % (target_board, target_scenario))
        sys.exit(1)

    kconfig.load_config(defconfig_path)

    config_path = sys.argv[2]
    if os.path.isfile(config_path):
        # No need to change .config if it is already equivalent to the specified
        # default.
        kconfig_current = kconfiglib.Kconfig(kconfig_path)
        kconfig_current.load_config(config_path)
        same_config = True
        for sym in kconfig_current.syms:
            if kconfig_current.syms[sym].str_value != kconfig.syms[sym].str_value:
                same_config = False
                break
        if same_config:
            sys.exit(0)

    sys.stdout.write("Default configuration based on %s.\n" % defconfig_path)
    kconfig.write_config(config_path)
    sys.stdout.write("Configuration written to %s.\n" % config_path)

if __name__ == "__main__":
    main()
