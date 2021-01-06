#!/bin/sh
# Copyright (C) 2021 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

base_dir=$1
board_xml=$2
scenario_xml=$3
out=$4

tool_dir=${base_dir}/../misc/acrn-config

python3 ${tool_dir}/board_config/board_cfg_gen.py --board ${board_xml} --scenario ${scenario_xml} --out ${out} &&
python3 ${tool_dir}/scenario_config/scenario_cfg_gen.py --board ${board_xml} --scenario ${scenario_xml} --out ${out}

if [ $? -ne 0 ]; then
    exit $?
fi
