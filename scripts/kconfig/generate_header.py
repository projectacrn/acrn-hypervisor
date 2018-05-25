# Copyright (C) 2018 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# This script takes a Kconfig and a .config, and generates a C header file with
# all the configuration data defined as object-like macros.

import sys, os

# Kconfiglib: Copyright (c) 2011-2018, Ulf Magnusson
# SPDX-License-Identifier: ISC
# Refer to scripts/kconfig/LICENSE.kconfiglib for the permission notice.
import kconfiglib

def usage():
    sys.stdout.write("%s: <Kconfig file> <.config file> <path to config.h>\n" % sys.argv[0])

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
    kconfig.write_autoconf(sys.argv[3])
    sys.stdout.write("Configuration header written to %s.\n" % sys.argv[3])

if __name__ == "__main__":
    main()
