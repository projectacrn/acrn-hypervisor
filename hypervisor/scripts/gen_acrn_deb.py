# -*- coding: utf-8 -*-
#* Copyright (c) 2020 Intel Corporation
import os,sys,copy,json
import subprocess
import datetime
import time
import shlex
import glob
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

def create_acrn_deb(cur_dir, deb_dir, board, scenario, version, build_dir):
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
                'Section: free \n',
                'Priority: optional \n',
                'Architecture: amd64 \n',
                'Maintainer: Intel\n',
                'Description: ACRN Hypervisor for IoT \n',
                'Depends: libssl-dev, libpciaccess-dev, uuid-dev, libsystemd-dev, libevent-dev, libxml2-dev, acpica-tools (>= 20200326) \n',
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
        source = cur_dir + '/../' + source
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

    run_command('cp ./misc/packaging/acrn-hypervisor.preinst ./build/acrn_release_deb/DEBIAN/preinst', cur_dir)
    run_command('chmod +x ./build/acrn_release_deb/DEBIAN/preinst', cur_dir)
    run_command('sed -i \'s/\r//\' ./build/acrn_release_deb/DEBIAN/preinst', cur_dir)

    run_command('dpkg -b acrn_release_deb acrn-%s-%s-%s.deb' %(board, scenario, version), build_dir)

    return

def create_acrn_board_inspector_deb(cur_dir, deb_dir, version, build_dir):
    #control file description
    listcontrol=['Package: acrn-board-inspector\n',
                'version: %s \n'% version,
                'Section: free \n',
                'Priority: optional \n',
                'Architecture: amd64 \n',
                'Maintainer: Intel\n',
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

    run_command('dpkg -b acrn_board_inspector_deb acrn-board-inspector-%s.deb' %(version), build_dir)

    return

if __name__ == "__main__":
    if len(sys.argv) < 5:
        BUILD_DIR = sys.argv[1]
        HV_VERSION = sys.argv[2]
        DEB_MODE = sys.argv[3]
    else:
        BOARD = sys.argv[1]
        SCENARIO = sys.argv[2]
        BUILD_DIR = sys.argv[3]
        HV_VERSION = sys.argv[4]
        DEB_MODE = 'no'

    cur_dir = os.getcwd()
    if DEB_MODE != 'board_inspect':
        deb_dir = BUILD_DIR + '/acrn_release_deb/'
        cmd_list = []
        if os.path.exists(deb_dir):
            add_cmd_list(cmd_list, 'rm -rf acrn_release_deb', BUILD_DIR)
        add_cmd_list(cmd_list, 'mkdir -p acrn_release_deb', BUILD_DIR)
        add_cmd_list(cmd_list, 'mkdir -p acrn_release_deb/boot', BUILD_DIR)
        add_cmd_list(cmd_list, 'mkdir DEBIAN', deb_dir)
        add_cmd_list(cmd_list, 'touch DEBIAN/control', deb_dir)

        deb_bin_name ='acrn.%s.%s.bin' % (SCENARIO,BOARD)
        deb_out_name ='acrn.%s.%s.32.out' % (SCENARIO,BOARD)
        build_bin_name = BUILD_DIR + '/hypervisor/acrn.bin'
        build_out_name = BUILD_DIR + '/hypervisor/acrn.32.out'


        add_cmd_list(cmd_list, 'cp %s acrn_release_deb/boot/%s' %(build_bin_name, deb_bin_name), BUILD_DIR)
        add_cmd_list(cmd_list, 'cp %s acrn_release_deb/boot/%s' %(build_out_name, deb_out_name), BUILD_DIR)
        run_cmd_list(cmd_list)

        create_acrn_deb(cur_dir, deb_dir, BOARD, SCENARIO, HV_VERSION, BUILD_DIR)
    else:
        deb_dir = BUILD_DIR + '/acrn_board_inspector_deb/'
        cmd_list = []
        if os.path.exists(deb_dir):
            add_cmd_list(cmd_list, 'rm -rf acrn_board_inspector_deb', BUILD_DIR)
        add_cmd_list(cmd_list, 'mkdir -p acrn_board_inspector_deb', BUILD_DIR)
        add_cmd_list(cmd_list, 'mkdir DEBIAN', deb_dir)
        add_cmd_list(cmd_list, 'touch DEBIAN/control', deb_dir)

        run_cmd_list(cmd_list)

        create_acrn_board_inspector_deb(cur_dir, deb_dir, HV_VERSION, BUILD_DIR)
