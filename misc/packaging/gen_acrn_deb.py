# -*- coding: utf-8 -*-
#* Copyright (c) 2020 Intel Corporation
import os,sys,copy,json
import subprocess
import datetime
import time
import shlex
import glob
import argparse
import multiprocessing

def run_command(cmd, path):
	ret_code = 0
	#print("cmd = %s, path = %s" % (cmd, path))
	cmd_proc = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd = path, universal_newlines=True)
	while True:
		output = cmd_proc.stdout.readline()
		#print(output.strip())
		ret_code = cmd_proc.poll()
		if ret_code is not None:
			break
	return ret_code

def add_cmd_list(cmd_list, cmd_str, dir_str):
    cmd = {}
    cmd['cmd'] = cmd_str
    cmd['dir'] = dir_str
    cmd_list.append(cmd)

def run_cmd_list(cmd_list):
    for i, cmd in enumerate(cmd_list):
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
    deb_bin_name ='acrn.%s.%s.bin' % (scenario,board)
    deb_out_name ='acrn.%s.%s.32.out' % (scenario,board)
    build_bin_name = build_dir + '/hypervisor/acrn.bin'
    build_out_name = build_dir + '/hypervisor/acrn.32.out'
    add_cmd_list(cmd_list, 'cp %s acrn_release_deb/boot/%s' %(build_bin_name, deb_bin_name), build_dir)
    add_cmd_list(cmd_list, 'cp %s acrn_release_deb/boot/%s' %(build_out_name, deb_out_name), build_dir)
    run_cmd_list(cmd_list)

    lines=[]
    f=open(cur_dir + "/misc/packaging/acrn-hypervisor.postinst",'r')
    for line in f:
        lines.append(line)
    f.close()
    start = lines.index('#Build info Start\n')
    end = lines.index('#Build info End\n')
    del lines[(start+1):(end-1)]
    lines.insert(start+1,"\nSCENARIO=(%s)\n"%scenario)
    lines.insert(start+2,"\nBOARD=(%s)\n"%board)
    with open(cur_dir + "/misc/packaging/acrn-hypervisor.postinst", "w") as f:
        for line in lines:
            f.write(line)
    f.close()

    listcontrol=['Package: acrn-hypervisor\n',
                'version: %s \n'% version,
		'Depends: libcjson1\n',
                'Section: free \n',
                'Priority: optional \n',
                'Architecture: amd64 \n',
                'Maintainer: acrn-dev@lists.projectacrn.org \n',
                'Description: ACRN Hypervisor for IoT \n',
                '\n']
    with open(deb_dir + '/DEBIAN/control','w',encoding='utf-8') as fr:
            fr.writelines(listcontrol)

    #design in acrn_data
    with open(cur_dir + "/.deb.conf","r") as load_deb:
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
        if os.path.exists(target):
            run_command('cp %s %s' % (source, target), cur_dir)
        else:
            run_command('mkdir -p %s' % target, cur_dir)
            run_command('cp %s %s' % (source, target), cur_dir)

    run_command('cp ./misc/packaging/acrn-hypervisor.postinst ./build/acrn_release_deb/DEBIAN/postinst', cur_dir)
    run_command('chmod +x ./build/acrn_release_deb/etc/grub.d/100_ACRN', cur_dir)
    run_command('chmod +x ./build/acrn_release_deb/DEBIAN/postinst', cur_dir)
    run_command('sed -i \'s/\r//\' ./build/acrn_release_deb/DEBIAN/postinst', cur_dir)
    run_command('chmod +x ./build/acrn_release_deb/DEBIAN/preinst', cur_dir)
    run_command('sed -i \'s/\r//\' ./build/acrn_release_deb/DEBIAN/preinst', cur_dir)

    ret = run_command('dpkg -b acrn_release_deb acrn-%s-%s-%s.deb' %(board, scenario, version), build_dir)
    if ret != 0:
        print("ERROR : generate ACRN debian package acrn-{}-{}-{}.deb failed! \
Please check all the files in {}/acrn_release_deb".format(board, scenario, version, build_dir))
    else:
        print("ACRN debian package acrn-{}-{}-{}.deb was successfully created in the {}.".format(board, scenario, version, build_dir))
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

    #control file description
    listcontrol=['Package: acrn-board-inspector\n',
                'version: %s \n'% version,
                'Section: free \n',
                'Priority: optional \n',
                'Architecture: amd64 \n',
                'Maintainer: acrn-dev@lists.projectacrn.org \n',
                'Description: ACRN board inspector tools \n',
                'Depends: cpuid, msr-tools, pciutils, dmidecode, python3, python3-pip, python3-lxml \n',
                '\n']
    with open(deb_dir + '/DEBIAN/control','w',encoding='utf-8') as fr:
            fr.writelines(listcontrol)
    run_command('cp -r ./misc/config_tools/board_inspector/ ./build/acrn_board_inspector_deb/bin/', cur_dir)
    run_command('cp ./misc/packaging/acrn-board-inspector.postinst ./build/acrn_board_inspector_deb/DEBIAN/postinst', cur_dir)
    run_command('chmod +x ./build/acrn_board_inspector_deb/DEBIAN/postinst', cur_dir)
    run_command('sed -i \'s/\r//\' ./build/acrn_board_inspector_deb/DEBIAN/postinst', cur_dir)
    run_command('cp ./misc/packaging/acrn-board-inspector.prerm ./build/acrn_board_inspector_deb/DEBIAN/prerm', cur_dir)
    run_command('chmod +x ./build/acrn_board_inspector_deb/DEBIAN/prerm', cur_dir)
    run_command('sed -i \'s/\r//\' ./build/acrn_board_inspector_deb/DEBIAN/prerm', cur_dir)

    ret = run_command('dpkg -b acrn_board_inspector_deb acrn-board-inspector-%s.deb' %(version), build_dir)
    if ret != 0:
        print("ERROR : generate board_inspector debian package acrn-board-inspector-{}.deb failed! \
Please check all the files in {}/acrn_board_inspector_deb".format(version, build_dir))
    else:
        print("board_inspector debian package acrn-board-inspector-{}.deb was successfully created in the {}.".format(version, build_dir))
    return

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("deb_mode", help="choose deb mode, e.g. acrn_all or board_inspector")
    parser.add_argument("build_dir", help="the absolute address of the acrn-hypervisor build directory")
    parser.add_argument("--version", default="1.0", help="the acrn-hypervisor version")
    parser.add_argument("--board_name", default="board", help="the name of the board that runs the ACRN hypervisor")
    parser.add_argument("--scenario", default="scenario", help="the acrn hypervisor scenario setting")
    args = parser.parse_args()

    if args.deb_mode == 'board_inspector':
        create_acrn_board_inspector_deb(args.version, args.build_dir)
    elif args.deb_mode == 'acrn_all':
        create_acrn_deb(args.board_name, args.scenario, args.version, args.build_dir)
    else:
        print("ERROR: Please check the value of deb_mode: the value shall be acrn_all or board_inspector.")
