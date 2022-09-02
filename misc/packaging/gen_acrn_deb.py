#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import os
import json
import shlex
import shutil
import subprocess
import argparse
import re

from pathlib import Path

DEBUG = False


def run_command(cmd, path):
    if DEBUG:
        print("cmd = %s, path = %s" % (cmd, path))
    cmd_proc = subprocess.Popen(
        shlex.split(cmd),
        cwd=path,
        stdout=sys.stdout,
        stderr=sys.stderr,
        universal_newlines=True
    )
    while True:
        ret_code = cmd_proc.poll()
        if ret_code is not None:
            break
    return ret_code


def add_cmd_list(cmd_list, cmd_str, dir_str):
    cmd = {
        'cmd': cmd_str,
        'dir': dir_str
    }
    cmd_list.append(cmd)


def run_cmd_list(cmd_list):
    for cmd in cmd_list:
        ret = run_command(cmd['cmd'], cmd['dir'])
        if ret != 0:
            print("cmd(%s) run in dir(%s) failed and exit" % (cmd['cmd'], cmd['dir']))
            exit(-1)
    return


def create_acrn_deb(board, scenario, version, build_dir):
    cur_dir = build_dir + '/../'
    deb_dir = build_dir + '/acrn_release_deb/'
    cmd_list = []
    if os.path.exists(deb_dir):
        add_cmd_list(cmd_list, 'rm -rf acrn_release_deb', build_dir)
    add_cmd_list(cmd_list, 'mkdir -p acrn_release_deb', build_dir)
    add_cmd_list(cmd_list, 'mkdir -p acrn_release_deb/boot', build_dir)
    add_cmd_list(cmd_list, 'mkdir DEBIAN', deb_dir)
    add_cmd_list(cmd_list, 'touch DEBIAN/control', deb_dir)
    deb_bin_name = 'acrn.%s.%s.bin' % (scenario, board)
    deb_out_name = 'acrn.%s.%s.32.out' % (scenario, board)
    build_bin_name = build_dir + '/hypervisor/acrn.bin'
    build_out_name = build_dir + '/hypervisor/acrn.32.out'
    add_cmd_list(cmd_list, 'cp %s acrn_release_deb/boot/%s' % (build_bin_name, deb_bin_name), build_dir)
    add_cmd_list(cmd_list, 'cp %s acrn_release_deb/boot/%s' % (build_out_name, deb_out_name), build_dir)
    run_cmd_list(cmd_list)

    lines = []
    f = open(cur_dir + "/misc/packaging/acrn-hypervisor.postinst", 'r')
    for line in f:
        lines.append(line)
    f.close()
    start = lines.index('#Build info Start\n')
    end = lines.index('#Build info End\n')
    del lines[(start + 1):(end - 1)]
    lines.insert(start + 1, "\nSCENARIO=(%s)\n" % scenario)
    lines.insert(start + 2, "\nBOARD=(%s)\n" % board)

    a_f = open(build_dir + "/hypervisor/.scenario.xml", 'r')
    for a_line in a_f:
        l = re.search("<CPU_PERFORMANCE_POLICY>(\w*)</CPU_PERFORMANCE_POLICY>", a_line)
        if l != None:
            break;
    start = lines.index('#ACRN parameters Start\n')
    end = lines.index('#ACRN parameters End\n')
    del lines[(start + 1):(end - 1)]
    if l == None:
        lines.insert(start + 1, "\nGENERATED_PARAMS=(cpu_perf_policy=%s)\n" % "Performance")
    else:
        lines.insert(start + 1, "\nGENERATED_PARAMS=(cpu_perf_policy=%s)\n" % l.group(1))

    with open(cur_dir + "/misc/packaging/acrn-hypervisor.postinst", "w") as f:
        for line in lines:
            f.write(line)
    f.close()

    listcontrol = [
        'Package: acrn-hypervisor\n',
        'version: %s \n' % version,
        'Depends: libcjson1\n',
        'Pre-Depends: libsdl2-2.0-0\n',
        'Section: free \n',
        'Priority: optional \n',
        'Architecture: amd64 \n',
        'Maintainer: acrn-dev@lists.projectacrn.org \n',
        'Description: ACRN Hypervisor for IoT \n',
        '\n'
    ]
    with open(deb_dir + '/DEBIAN/control', 'w', encoding='utf-8') as fr:
        fr.writelines(listcontrol)

    # design in acrn_data
    with open(cur_dir + "/.deb.conf", "r") as load_deb:
        deb_info = json.load(load_deb)
    load_deb.close()

    deb_info_list = list(deb_info)
    run_command('rm -rf acrn_release_img/usr/*', build_dir)

    for i in deb_info_list:
        source = deb_info[i]['source']
        target = deb_info[i]['target']
        if target == 'boot/':
            continue
        source = cur_dir + source
        target = deb_dir + target
        if os.path.exists(source):
            if os.path.exists(target):
                run_command('cp %s %s' % (source, target), cur_dir)
            else:
                run_command('mkdir -p %s' % target, cur_dir)
                run_command('cp %s %s' % (source, target), cur_dir)

    run_command('cp ./misc/packaging/acrn-hypervisor.postinst ./build/acrn_release_deb/DEBIAN/postinst', cur_dir)
    run_command('chmod +x ./build/acrn_release_deb/etc/grub.d/100_ACRN', cur_dir)
    run_command('chmod +x ./build/acrn_release_deb/DEBIAN/postinst', cur_dir)
    run_command('sed -i \'s/\r//\' ./build/acrn_release_deb/DEBIAN/postinst', cur_dir)
    if os.path.exists('./build/acrn_release_deb/DEBIAN/preinst'):
        run_command('chmod +x ./build/acrn_release_deb/DEBIAN/preinst', cur_dir)
        run_command('sed -i \'s/\r//\' ./build/acrn_release_deb/DEBIAN/preinst', cur_dir)

    ret = run_command('dpkg -b acrn_release_deb acrn-%s-%s-%s.deb' % (board, scenario, version), build_dir)
    if ret != 0:
        print("ERROR : generate ACRN debian package acrn-{}-{}-{}.deb failed! \
Please check all the files in {}/acrn_release_deb".format(board, scenario, version, build_dir))
    else:
        print(
            "ACRN debian package acrn-{}-{}-{}.deb was successfully created in the {}.".format(board, scenario, version,
                                                                                               build_dir))
    return


def create_acrn_board_inspector_deb(version, build_dir):
    cur_dir = build_dir + '/../'
    deb_dir = build_dir + '/acrn_board_inspector_deb/'
    cmd_list = []
    if os.path.exists(deb_dir):
        add_cmd_list(cmd_list, 'rm -rf acrn_board_inspector_deb', build_dir)
    add_cmd_list(cmd_list, 'mkdir -p acrn_board_inspector_deb', build_dir)
    add_cmd_list(cmd_list, 'mkdir DEBIAN', deb_dir)
    add_cmd_list(cmd_list, 'touch DEBIAN/control', deb_dir)
    run_cmd_list(cmd_list)

    # control file description
    listcontrol = [
        'Package: acrn-board-inspector\n',
        'version: %s \n' % version,
        'Section: free \n',
        'Priority: optional \n',
        'Architecture: amd64 \n',
        'Maintainer: acrn-dev@lists.projectacrn.org \n',
        'Description: ACRN board inspector tools \n',
        'Depends: cpuid, msr-tools, pciutils, dmidecode, python3, python3-pip, python3-lxml, python3-tqdm \n',
        '\n'
    ]
    with open(deb_dir + '/DEBIAN/control', 'w', encoding='utf-8') as fr:
        fr.writelines(listcontrol)
    run_command('cp -r ./misc/config_tools/board_inspector/ ./build/acrn_board_inspector_deb/bin/', cur_dir)
    run_command('cp ./misc/packaging/acrn-board-inspector.postinst ./build/acrn_board_inspector_deb/DEBIAN/postinst',
                cur_dir)
    run_command('chmod +x ./build/acrn_board_inspector_deb/DEBIAN/postinst', cur_dir)
    run_command('sed -i \'s/\r//\' ./build/acrn_board_inspector_deb/DEBIAN/postinst', cur_dir)
    run_command('cp ./misc/packaging/acrn-board-inspector.prerm ./build/acrn_board_inspector_deb/DEBIAN/prerm', cur_dir)
    run_command('chmod +x ./build/acrn_board_inspector_deb/DEBIAN/prerm', cur_dir)
    run_command('sed -i \'s/\r//\' ./build/acrn_board_inspector_deb/DEBIAN/prerm', cur_dir)

    ret = run_command('dpkg -b acrn_board_inspector_deb acrn-board-inspector-%s.deb' % version, build_dir)
    if ret != 0:
        print(
            "ERROR : generate board_inspector debian package acrn-board-inspector-{}.deb failed! "
            "Please check all the files in {}/acrn_board_inspector_deb".format(version, build_dir)
        )
    else:
        print(
            "board_inspector debian package acrn-board-inspector-{}.deb"
            " was successfully created in the {}.".format(version, build_dir)
        )
    return


def create_configurator_deb(version, build_dir):
    cmd_list = []

    # get folder path
    project_base = Path(__file__).parent.parent.parent
    config_tools_path = Path(__file__).parent.parent / 'config_tools'
    configurator_path = config_tools_path / 'configurator'
    scenario_config_path = project_base / "misc" / "config_tools" / "scenario_config"
    deb_dir = configurator_path / 'packages' / 'configurator' / 'src-tauri' / 'target' / 'release' / 'bundle' / 'deb'

    # clean old directory
    if os.path.isdir(deb_dir):
        shutil.rmtree(deb_dir)

    # build command, if you update this, please update misc/config_tools/configurator/README.md#L55
    add_cmd_list(cmd_list, 'python3 schema_slicer.py', scenario_config_path)
    add_cmd_list(cmd_list, 'python3 converter.py', scenario_config_path / "jsonschema")
    add_cmd_list(cmd_list, 'bash -c "xmllint --xinclude schema/datachecks.xsd > schema/allchecks.xsd"', config_tools_path)
    add_cmd_list(cmd_list, 'python3 -m build', config_tools_path)
    add_cmd_list(cmd_list, 'bash -c "rm -f ./configurator/packages/configurator/thirdLib/acrn_config_tools-3.0-py3-none-any.whl"', config_tools_path)
    add_cmd_list(cmd_list, 'python3 packages/configurator/thirdLib/manager.py install', configurator_path)
    add_cmd_list(cmd_list, 'yarn', configurator_path)
    add_cmd_list(cmd_list, 'yarn build', configurator_path)
    run_cmd_list(cmd_list)

    orig_deb_name = [x for x in os.listdir(deb_dir) if x.endswith('.deb')]
    if not orig_deb_name:
        print('ERROR! No acrn-configurator deb found!')
        return
    orig_deb_name = orig_deb_name[0]
    dist_deb_name = 'acrn-configurator-{ver}.deb'.format(ver=version)
    with open(deb_dir / orig_deb_name, 'rb') as src:
        with open(os.path.join(build_dir, dist_deb_name), 'wb') as dest:
            dest.write(src.read())
    return

def clean_configurator_deb(version, build_dir):
    cmd_list = []

    # get folder path
    project_base = Path(__file__).parent.parent.parent
    config_tools_path = Path(__file__).parent.parent / 'config_tools'

    add_cmd_list(cmd_list, 'bash -c "find -name "*.log" -delete"', config_tools_path)
    add_cmd_list(cmd_list, 'bash -c "find -name "*.whl" -delete"', config_tools_path)
    add_cmd_list(cmd_list, 'bash -c "find -name "*.egg-info" -prune -exec rm -rf {} \;"', config_tools_path)
    add_cmd_list(cmd_list, 'bash -c "find -name "node_modules" -prune -exec rm -rf {} \;"', config_tools_path)
    add_cmd_list(cmd_list, 'bash -c "find -name "build" -prune -exec rm -rf {} \;"', config_tools_path)
    add_cmd_list(cmd_list, 'bash -c "find -name "target" -prune -exec rm -rf {} \;"', config_tools_path)
    add_cmd_list(cmd_list, 'bash -c "rm -rf dist"', config_tools_path)
    add_cmd_list(cmd_list, 'bash -c "rm -rf schema/sliced.xsd"', config_tools_path)
    add_cmd_list(cmd_list, 'bash -c "rm -rf schema/allchecks.xsd"', config_tools_path)
    add_cmd_list(cmd_list, 'bash -c "python3 ./configurator/packages/configurator/thirdLib/manager.py clean"', config_tools_path)
    run_cmd_list(cmd_list)
    return

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("deb_mode", help="choose deb mode, e.g. acrn_all, board_inspector, configurator or clean")
    parser.add_argument("build_dir", help="the absolute address of the acrn-hypervisor build directory")
    parser.add_argument("--version", default="1.0", help="the acrn-hypervisor version")
    parser.add_argument("--board_name", default="board", help="the name of the board that runs the ACRN hypervisor")
    parser.add_argument("--scenario", default="scenario", help="the acrn hypervisor scenario setting")
    parser.add_argument("--debug", default=False, help="debug mode")
    args = parser.parse_args()

    DEBUG = args.debug

    if args.deb_mode == 'board_inspector':
        create_acrn_board_inspector_deb(args.version, args.build_dir)
    elif args.deb_mode == 'acrn_all':
        create_acrn_deb(args.board_name, args.scenario, args.version, args.build_dir)
    elif args.deb_mode == 'configurator':
        create_configurator_deb(args.version, args.build_dir)
    elif args.deb_mode == 'clean':
        clean_configurator_deb(args.version, args.build_dir)
    else:
        print("ERROR: Please check the value of the first argument: the value shall be acrn_all, board_inspector, configurator or clean.")
