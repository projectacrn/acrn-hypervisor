# Copyright (C) 2018 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# This script
#
#     1. takes a Kconfig and a .config and an optional list of symbol-value pairs,
#     2. checks whether the specified symbols have the specified values in the
#        given .config, and
#     3. reconstruct .config with the given list of symbol-value pairs if there
#        is any disagreement.

import sys, os

# Kconfiglib: Copyright (c) 2011-2018, Ulf Magnusson
# SPDX-License-Identifier: ISC
# Refer to scripts/kconfig/LICENSE.kconfiglib for the permission notice.
import kconfiglib

def usage():
    sys.stdout.write("%s: <Kconfig file> <.config file> [<symbol1>=<value1> ...]\n" % sys.argv[0])

def main():
    if len(sys.argv) < 3:
        usage()
        sys.exit(1)

    kconfig_path = sys.argv[1]
    if not os.path.isfile(kconfig_path):
        sys.stderr.write("Cannot find file %s\n" % kconfig_path)
        sys.exit(1)

    kconfig = kconfiglib.Kconfig(kconfig_path)

    config_path = sys.argv[2]
    has_old_config = False
    if os.path.isfile(config_path):
        kconfig.load_config(config_path)
        has_old_config = True

    # Parse the configs specified on cmdline

    cmdline_conf = {}
    for sym_val in sys.argv[3:]:
        if sym_val.find("=") == -1:
            continue
        sym_name, val = sym_val.split("=")[:2]
        if sym_name in kconfig.syms.keys():
            cmdline_conf[sym_name] = val

    # Check if the old .config conflicts with those specified on cmdline

    has_conflict = False
    for sym_name, val in cmdline_conf.items():
        sym = kconfig.syms[sym_name]
        if sym.str_value and sym.str_value != val:
            has_conflict = True
            break

    # If there's any conflict, reconfigure using those from the cmdline. The
    # default values are used for unspecified symbols.

    if not has_old_config or has_conflict:
        kconfig.unset_values()
        for sym_name, val in cmdline_conf.items():
            kconfig.syms[sym_name].set_value(val)

        kconfig.write_config(config_path)
        sys.stdout.write("Configuration written to %s.\n" % config_path)

if __name__ == "__main__":
    main()
