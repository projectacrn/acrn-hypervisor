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

    # Parse the configs specified on cmdline

    cmdline_conf = {}
    for sym_val in sys.argv[3:]:
        if sym_val.find("=") == -1:
            continue
        sym_name, val = sym_val.split("=")[:2]
        if sym_name in kconfig.syms.keys() and val:
            cmdline_conf[sym_name] = val

    # Determine the base config.
    #
    # If either
    #
    #    1. no .config exists, or
    #    2. the BOARD in the existing .config is different from the BOARD
    #    specified in the environment variable
    #
    # the defconfig will be used as the base config. Otherwise the existing
    # .config is used as the base.
    #
    # Note that the BOARD given by the environment variable may be different
    # from what is specified in the corresponding defconfig. Thus we get the
    # target_board by loading the defconfig first.
    #
    # To be backward compatible, do not look for any defconfig in the Kconfig if
    # such BOARD is not given. Simply use the .config or all default values
    # (when no .config exists) as the base.
    config_path = sys.argv[2]
    target_board = None
    if 'BOARD' in os.environ:
        defconfig_path = kconfig.defconfig_filename
        if defconfig_path and os.path.isfile(defconfig_path):
            kconfig.load_config(defconfig_path)
            target_board = kconfig.syms['BOARD'].str_value

    need_update = False
    if os.path.isfile(config_path):
        kconfig.load_config(config_path)
        if target_board and kconfig.syms['BOARD'].str_value != target_board:
            kconfig.load_config(defconfig_path)
            sys.stdout.write("Overwrite with default configuration based on %s.\n" % defconfig_path)
            need_update = True
    else:
        # create a default configuration
        if target_board:
            # target_board is not None if and only if a BOARD is specified and the
            # corresponding defconfig has been loaded.
            sys.stdout.write("Default configuration based on %s.\n" % defconfig_path)
        else:
            # If no known defconfig exists, either report an error if a
            # BOARD is specified in the environment, or use the default values in
            # Kconfig.
            if 'BOARD' in os.environ:
                sys.stderr.write("No defconfig found for board %s.\n" % target_board)
                sys.exit(1)
        need_update = True

    # Update the old .config with those specified on cmdline
    #
    # Note: the user shall be careful what configuration symbols to overwrite by
    # silentoldconfig. After changing a symbol value, the invisible symbols are
    # updated accordingly because they always use the default value, while
    # visible symbols keep their original value in the old .config. This may
    # lead to invalid .config for a specific platform.
    #
    # Currently it is recommended to use the following update only for
    # RELEASE. For PLATFORM reinvoke defconfig is preferred.

    for sym_name, val in cmdline_conf.items():
        sym = kconfig.syms[sym_name]
        if sym.str_value and sym.str_value != val:
            kconfig.syms[sym_name].set_value(val)
            need_update = True

    if need_update:
        kconfig.write_config(config_path)
        sys.stdout.write("Configuration written to %s.\n" % config_path)

if __name__ == "__main__":
    main()
