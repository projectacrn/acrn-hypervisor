#!/bin/sh
# Copyright (C) 2021-2022 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

base_dir=$1
board_xml=$2
scenario_xml=$3
out=$4
unified_xml=$5
scenario=$(xmllint --xpath "string(//@scenario)" --xinclude $unified_xml)
year=$(date +'%Y')

apply_patch () {
    echo "Applying patch ${1}:"
    patch -p1 < ${1}
    if [ $? -ne 0 ]; then
        echo "Applying patch ${1} failed."
        exit 1
    fi
}

tool_dir=${base_dir}/../misc/config_tools
diffconfig_list=${out}/.diffconfig

python3 ${tool_dir}/board_config/board_cfg_gen.py --board ${board_xml} --scenario ${scenario_xml} --out ${out} &&
python3 ${tool_dir}/acpi_gen/asl_gen.py --board ${board_xml} --scenario ${scenario_xml} --out ${out}
exitcode=$?
if [ $exitcode -ne 0 ]; then
    exit $exitcode
fi

if ! which xsltproc ; then
    echo "xsltproc cannot be found, please install it and make sure it is in your PATH."
    exit 1
fi

transform() {
    echo "Generating ${1}:"
    xsltproc -o ${out}/scenarios/${scenario}/${1} --xinclude --xincludestyle ${tool_dir}/xforms/${1}.xsl ${unified_xml}
    if [ $? -ne 0 ]; then
        echo "Failed to generate ${1} with xsltproc!"
        exit 1
    fi
    sed -i -e "s/YEAR/$year/" ${out}/scenarios/${scenario}/${1}
    echo "${1} was generated using xsltproc successfully."
}

transform_board() {
    echo "Generating ${1}:"
    xsltproc -o ${out}/boards/${1} --xinclude --xincludestyle ${tool_dir}/xforms/${1}.xsl ${unified_xml}
    if [ $? -ne 0 ]; then
        echo "Failed to generate ${1} with xsltproc!"
        exit 1
    fi
    sed -i -e "s/YEAR/$year/" ${out}/boards/${1}
    echo "${1} was generated using xsltproc successfully."
}

transform vm_configurations.c
transform vm_configurations.h
transform pt_intx.c
transform ivshmem_cfg.h
transform misc_cfg.h
transform pci_dev.c
transform_board board_info.h

if which clang-format ; then
    find ${out}/scenarios/${scenario} -iname *.h -o -iname *.c \
    | xargs clang-format --style=file -i --fallback-style=none
else
    echo "clang-format cannot be found. The generated files under ${out}/scenarios/${scenario} are not formatted."
    echo "clang-format is a tool to format the C code automatically and improve the code readability."
    echo "Please install clang-format and format the generated files if those need to be included and reviewed."
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
