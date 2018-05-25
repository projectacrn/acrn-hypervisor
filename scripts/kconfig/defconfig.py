# Copyright (C) 2018 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# This script takes a Kconfig and a defconfig file, and expands it to a .config
# with all default values listed explicitly.

import sys, os

# Kconfiglib: Copyright (c) 2011-2018, Ulf Magnusson
# SPDX-License-Identifier: ISC
# Refer to scripts/kconfig/LICENSE.kconfiglib for the permission notice.
import kconfiglib

def usage():
    sys.stdout.write("%s: <Kconfig file> <defconfig> <path to .config>\n" % sys.argv[0])

def main():
    if len(sys.argv) < 4:
        usage()
        sys.exit(1)

    kconfig_path = sys.argv[1]
    if not os.path.isfile(kconfig_path):
        sys.stderr.write("Cannot find file %s\n" % kconfig_path)
        sys.exit(1)

    defconfig_path = sys.argv[2]
    if not os.path.isfile(defconfig_path):
        sys.stderr.write("Cannot find file %s\n" % defconfig_path)
        sys.exit(1)

    sys.stdout.write("Default configuration based on %s.\n" % defconfig_path)
    kconfig = kconfiglib.Kconfig(kconfig_path)
    kconfig.load_config(defconfig_path)
    kconfig.write_config(sys.argv[3])
    sys.stdout.write("Configuration written to %s.\n" % sys.argv[3])

if __name__ == "__main__":
    main()
