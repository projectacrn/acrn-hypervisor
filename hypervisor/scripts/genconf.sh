#!/bin/sh
# Copyright (C) 2021 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

base_dir=$1
board_xml=$2
scenario_xml=$3
out=$4

apply_patch () {
    echo "Applying patch ${1}:"
    patch -p1 < ${1}
    if [ $? -ne 0 ]; then
        echo "Applying patch ${1} failed."
        exit 1
    fi
}

tool_dir=${base_dir}/../misc/acrn-config
diffconfig_list=${out}/.diffconfig

python3 ${tool_dir}/board_config/board_cfg_gen.py --board ${board_xml} --scenario ${scenario_xml} --out ${out} &&
python3 ${tool_dir}/scenario_config/scenario_cfg_gen.py --board ${board_xml} --scenario ${scenario_xml} --out ${out}

if [ $? -ne 0 ]; then
    exit $?
fi

if [ -f ${diffconfig_list} ]; then
    cd ${out} &&
    cat ${diffconfig_list} | while read line; do
        if [ -f ${line} ]; then
            apply_patch ${line}
        elif [ -d ${line} ]; then
            find ${line} -maxdepth 1 -name '*.patch' | while read f; do
                apply_patch ${f}
            done
        else
            echo "${line}: No such file or directory"
            exit 1
        fi
    done
fi
