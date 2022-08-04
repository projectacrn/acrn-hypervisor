# Copyright (C) 2020-2022 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

RED="\033[0;31m"
YELLOW="\033[1;33m"
GREEN="\033[0;32m"
NO_COLOR="\033[0m"

has_error=0

function do_step() {
    local prompt=$1
    local func=$2
    shift 2

    echo -e "$(date -Iseconds) ${logger_prefix}${YELLOW}[ Starting ]${NO_COLOR} ${prompt}"
    if $func $*; then
        echo -e "$(date -Iseconds) ${logger_prefix}${GREEN}[   Done   ]${NO_COLOR} ${prompt}"
    else
        echo -e "$(date -Iseconds) ${logger_prefix}${RED}[  Failed  ]${NO_COLOR} ${prompt}"
        has_error=1
    fi
}

function try_step() {
    local prompt=$1
    shift 1

    if [[ ${has_error} != 0 ]]; then
        echo -e "$(date -Iseconds) ${logger_prefix}${YELLOW}[ Skipped  ]${NO_COLOR} ${prompt}"
    else
        do_step "$prompt" $*
    fi
}

function print_info() {
    if [[ ${has_error} == 0 ]]; then
        echo -e "$(date -Iseconds) ${logger_prefix}${YELLOW}[   Info   ]${NO_COLOR} $*"
    fi
}
