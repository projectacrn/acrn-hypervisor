# Copyright (C) 2018 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# Given a Kconfig, this script minimize a given .config by removing the symbols
# having the default values. The minimized config can act as a defconfig for
# future use.

import sys
import os

# Kconfiglib: Copyright (c) 2011-2018, Ulf Magnusson
# SPDX-License-Identifier: ISC
# Refer to scripts/kconfig/LICENSE.kconfiglib for the permission notice.
import kconfiglib

def usage():
    sys.stdout.write("%s: <Kconfig file> <.config file> <path to output .config>\n" % sys.argv[0])

def main():
    if len(sys.argv) < 4:
        usage()
        sys.exit(1)

    kconfig_path = sys.argv[1]
    if not os.path.isfile(kconfig_path):
        sys.stderr.write("Cannot find file %s\n" % kconfig_path)
        sys.exit(1)

    config_path = sys.argv[2]
    if not os.path.isfile(config_path):
        sys.stderr.write("Cannot find file %s\n" % config_path)
        sys.exit(1)

    kconfig = kconfiglib.Kconfig(kconfig_path)
    kconfig.load_config(config_path)
    kconfig.write_min_config(sys.argv[3])
    sys.stdout.write("Minimized configuration written to %s.\n" % sys.argv[3])

if __name__ == "__main__":
    main()
