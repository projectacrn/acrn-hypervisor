# -*- coding: utf-8 -*-
#* Copyright (c) 2020 Intel Corporation
import os,sys,copy,json
import subprocess
import datetime
import time
import shlex
import glob
import multiprocessing

#parse json file
with open("release.json","r") as load_f:
	load_dict = json.load(load_f)
load_f.close()

with open("deb.json","r") as load_fdeb:
	load_dictdeb = json.load(load_fdeb)
load_fdeb.close()

def run_command(cmd, path):
	ret_code = 0
	print("cmd = %s, path = %s" % (cmd, path))
	cmd_proc = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd = path, universal_newlines=True)

	while True:
		output = cmd_proc.stdout.readline()
		print(output.strip())

		ret_code = cmd_proc.poll()
		if ret_code is not None:
			for output in cmd_proc.stdout.readlines():
				print(output.strip())
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
# install compile package
def install_compile_package():
	#check compile env
	os.system('sudo apt install gcc \
     git \
     make \
     gnu-efi \
     libssl-dev \
     libpciaccess-dev \
     uuid-dev \
     libsystemd-dev \
     libevent-dev \
     libxml2-dev \
     libusb-1.0-0-dev \
     python3 \
     python3-pip \
     libblkid-dev \
     e2fslibs-dev \
     pkg-config \
     libelf-dev\
     libnuma-dev -y')
	gcc_version_out = os.popen('gcc --version')
	gcc_version = gcc_version_out.read()
	if tuple(gcc_version.split(' ')[3].split("\n")[0].split(".")) < tuple(load_dict['gcc_version'].split('.')):
		print("your gcc version is too old")
	binutils_version_out = os.popen('ld -v')
	binutils_version = binutils_version_out.read().split(" ")
	if tuple(binutils_version[-1].split("\n")[0].split(".")) < tuple(load_dict['binutils'].split('.')):
		print("your binutils version is too old")
	os.system('sudo apt install python-pip -y')
	os.system('sudo pip3 install kconfiglib')
	os.system('sudo apt-get install bison -y')
	os.system('sudo apt-get install flex -y')
	os.system('sudo apt install liblz4-tool -y')
	os.system('sudo apt install bc -y')

# build acrn
def build_acrn():
	cmd_list = []
	cur_dir = os.getcwd()
	hv_dir = cur_dir + '/' + 'acrn-hypervisor'
	if os.path.exists('acrn_release_img'):
		add_cmd_list(cmd_list, 'rm -rf acrn_release_img', cur_dir)
	add_cmd_list(cmd_list, 'mkdir -p acrn_release_img', cur_dir)

	if load_dict['sync_acrn_code'] == 'true':
		if os.path.exists('acrn-hypervisor'):
			add_cmd_list(cmd_list, 'rm -rf acrn-hypervisor', cur_dir)
		add_cmd_list(cmd_list, 'git clone %s' % load_dict['acrn_repo'], cur_dir)
		add_cmd_list(cmd_list, 'git checkout -b mybranch %s'% load_dict['release_version'], hv_dir)

	else:
		#clean git code directory
		if os.path.exists('acrn-hypervisor'):
			add_cmd_list(cmd_list, 'git reset --hard HEAD', hv_dir)
			add_cmd_list(cmd_list, 'git checkout master', hv_dir)
			add_cmd_list(cmd_list, 'git branch -D mybranch', hv_dir)
			add_cmd_list(cmd_list, 'git checkout -b mybranch %s' % load_dict['release_version'], hv_dir)


	if load_dict['acrn_patch']['patch_need'] == 'true':
		#Need to apply additional acrn patch
		patch_path = os.getcwd() + '/' + load_dict['acrn_patch']['patch_dir']

		for patch in load_dict['acrn_patch']['patch_list']:
			patch_full_name = patch_path + '/' + patch
			add_cmd_list(cmd_list, 'git am %s/%s' % (patch_path, patch), hv_dir)

	run_cmd_list(cmd_list)
	cmd_list = []
	make_cmd_list =[]

	release = load_dict['build_cmd']['release']
	scenario = load_dict['build_cmd']['scenario']
	board = load_dict['build_cmd']['board']
	info_list = []

	for i in scenario:
		if scenario[i] == 'true':
			for j in board:
				if board[j] == 'true':
					info_list.append((i,j))
					if load_dict['build_cmd']['build_method']['use_xml'] == 'true':
						make_cmd = 'make all BOARD_FILE=misc/acrn-config/xmls/board-xmls/%s.xml SCENARIO_FILE=misc/acrn-config/xmls/config-xmls/%s/%s.xml RELEASE=%s'%(j,j,i,release)
					else:
						make_cmd = 'make all BOARD=%s SCENARIO=%s RELEASE=%s'%(j,i,release)

					make_cmd_list.append(make_cmd)

	for i in range(len(make_cmd_list)):

		add_cmd_list(cmd_list, 'make clean', hv_dir)
		add_cmd_list(cmd_list, make_cmd_list[i], hv_dir)

		bin_name ='acrn.%s.%s.bin' % (info_list[i][0],info_list[i][1])
		out_name ='acrn.%s.%s.32.out' % (info_list[i][0],info_list[i][1])
		efi_name ='acrn.%s.%s.efi' % (info_list[i][0],info_list[i][1])

		add_cmd_list(cmd_list, 'cp %s acrn_release_img/%s' %(load_dictdeb['acrn.bin']['source'],bin_name), cur_dir)
		add_cmd_list(cmd_list, 'cp %s acrn_release_img/%s' %(load_dictdeb['acrn.32.out']['source'],out_name), cur_dir)


		if os.path.exists(load_dictdeb['acrn.efi']['source']):
				add_cmd_list(cmd_list, 'cp %s acrn_release_img/%s' %(load_dictdeb['acrn.efi']['source'],efi_name), cur_dir)
	run_cmd_list(cmd_list)
	return

def create_acrn_kernel_deb():

	cmd_list = []
	cur_dir = os.getcwd()
	kernel_dir = cur_dir + '/' + 'acrn-kernel'
	kernel_deb_dir = cur_dir + '/' + 'acrn_kernel_deb'
	kernel_boot_dir = kernel_dir + '/' + 'boot'
	if os.path.exists('acrn_kernel_deb'):
		add_cmd_list(cmd_list, 'rm -rf acrn_kernel_deb', cur_dir)

	add_cmd_list(cmd_list, 'mkdir -p acrn_kernel_deb', cur_dir)
	add_cmd_list(cmd_list, 'mkdir DEBIAN', kernel_deb_dir)
	add_cmd_list(cmd_list, 'touch DEBIAN/control', kernel_deb_dir)

	# following operations depends on the previous cmmd. Run and clear cmd list here
	run_cmd_list(cmd_list)
	cmd_list = []

	#control file description

	listcontrol=['Package: acrn-kernel-package\n','version: %s \n'% datetime.date.today(),'Section: free \n','Priority: optional \n','Architecture: amd64 \n','Installed-Size: 66666 \n','Maintainer: Intel\n','Description: service_vm_kernel \n','\n']


	with open('acrn_kernel_deb/DEBIAN/control','w',encoding='utf-8') as fr:
			fr.writelines(listcontrol)

	cmd = 'cd acrn-kernel' + '&&' + 'ls *.gz'
	filename = os.popen(cmd).read().replace('\n', '').replace('\r', '')
	add_cmd_list(cmd_list, 'cp acrn-kernel/%s acrn_kernel_deb/' % filename, cur_dir)
	add_cmd_list(cmd_list, 'tar -zvxf %s' % filename, kernel_deb_dir)
	# following operations depends on the previous cmmd. Run and clear cmd list here
	run_cmd_list(cmd_list)
	cmd_list = []

	cmd = 'cd acrn_kernel_deb/boot' + '&&' + 'ls vmlinuz*'
	version = os.popen(cmd)

	f = open("acrn_kernel_deb/boot/version.txt",'w')
	f.write(version.read())
	f.close()

	add_cmd_list(cmd_list, 'cp acrn-kernel.postinst acrn_kernel_deb/DEBIAN/postinst', cur_dir)
	add_cmd_list(cmd_list, 'chmod +x acrn_kernel_deb/DEBIAN/postinst', cur_dir)
	add_cmd_list(cmd_list, 'sed -i \'s/\r//\' acrn_kernel_deb/DEBIAN/postinst', cur_dir)
	add_cmd_list(cmd_list, 'rm acrn_kernel_deb/%s' % filename, cur_dir)
	add_cmd_list(cmd_list, 'dpkg -b acrn_kernel_deb acrn_kernel_deb_package.deb ', cur_dir)

	run_cmd_list(cmd_list)
	cmd_list = []
	return

def build_acrn_kernel(acrn_repo,acrn_version):

	cmd_list = []
	cur_dir = os.getcwd()
	kernel_dir = cur_dir + '/' + 'acrn-kernel'

	if load_dict['sync_acrn_kernel_code'] == 'true':
		if os.path.exists('acrn-kernel'):
			add_cmd_list(cmd_list, 'rm -rf acrn-kernel', cur_dir)

		add_cmd_list(cmd_list, 'git clone %s' % acrn_repo, cur_dir)
		add_cmd_list(cmd_list, 'git checkout %s'% acrn_version, kernel_dir)

	if load_dict['kernel_patch']['patch_need'] == 'true':
		#Need to apply additional acrn patch
		patch_path = os.getcwd() + '/' + load_dict['kernel_patch']['patch_dir']

		for patch in load_dict['kernel_patch']['patch_list']:
			patch_full_name = patch_path + '/' + patch
			add_cmd_list(cmd_list, 'git am %s/%s' % (patch_path, patch), hv_dir)
	add_cmd_list(cmd_list, 'make clean', kernel_dir)
	add_cmd_list(cmd_list, 'cp kernel_config_service_vm .config', kernel_dir)
	add_cmd_list(cmd_list, 'make olddefconfig', kernel_dir)

	cpu_cnt = multiprocessing.cpu_count()
	cmd = 'make targz-pkg' + " -j%s" % str(cpu_cnt)
	add_cmd_list(cmd_list, cmd, kernel_dir)

	run_cmd_list(cmd_list)
	cmd_list = []
	return



#install acrn
def install_acrn_deb():
	os.system('dpkg -r acrn-package')
	os.system('dpkg -i acrn_deb_package.deb ')


#install kernel
def install_acrn_kernel_deb():
	os.system('dpkg -r acrn-kernel-package ')
	os.system('dpkg -i acrn_kernel_deb_package.deb ')


def create_acrn_deb():
	cmd_list = []
	cur_dir = os.getcwd()
	hv_dir = cur_dir + '/' + 'acrn_release_deb'
	path = 'acrn_release_deb'
	if os.path.exists(path):
		add_cmd_list(cmd_list, 'rm -rf acrn_release_deb', cur_dir)
	add_cmd_list(cmd_list, 'mkdir -p acrn_release_deb', cur_dir)
	add_cmd_list(cmd_list, 'mkdir DEBIAN', hv_dir)
	add_cmd_list(cmd_list, 'touch DEBIAN/control', hv_dir)

	# following operations depends on the previous cmmd. Run and clear cmd list here
	run_cmd_list(cmd_list)
	cmd_list = []
	#control file description
	acrn_info = load_dict['release_version']
	scenario = load_dict['build_cmd']['scenario']
	board = load_dict['build_cmd']['board']

	scenario_info=""
	board_info=""
	for i in scenario:
		scenario_info = scenario_info + i +" "

	for i in board:
		board_info = board_info + i +" "


	lines=[]
	f=open("acrn-hypervisor.postinst",'r')
	for line in f:
		lines.append(line)
	f.close()

	start = lines.index('echo "please choose <scenario> ,<board> ,<disk type>"\n')

	end = lines.index('echo "Scenario is ->"\n')

	del lines[(start+1):(end-1)]

	lines.insert(start+1,"\nscenario_info=(%s)\n"%scenario_info)
	lines.insert(start+2,"\nboard_info=(%s)\n"%board_info)
	with open("acrn-hypervisor.postinst", "w") as f:
		for line in lines:
			f.write(line)
	f.close()
	listcontrol=['Package: acrn-package\n','version: %s \n'% datetime.date.today(),'Section: free \n','Priority: optional \n','Architecture: amd64 \n','Installed-Size: 66666 \n','Maintainer: Intel\n','Description: %s \n' % acrn_info,'\n']


	with open('acrn_release_deb/DEBIAN/control','w',encoding='utf-8') as fr:
			fr.writelines(listcontrol)

	#design in acrn_data

	with open("deb.json","r") as load_deb:
		deb_info = json.load(load_deb)

	load_deb.close()

	deb_info_list = list(deb_info)

	for i in deb_info_list:
		source = deb_info[i]['source']
		target = deb_info[i]['target']
		if target == 'boot':
			continue
		if os.path.exists(target):
			add_cmd_list(cmd_list, 'cp %s %s' % (source,target), cur_dir)
		else:
			add_cmd_list(cmd_list, 'mkdir -p %s' % target, cur_dir)
			add_cmd_list(cmd_list, 'cp %s %s' % (source,target), cur_dir)

	add_cmd_list(cmd_list, 'mkdir -p %s' % target, cur_dir)
	add_cmd_list(cmd_list, 'cp %s %s' % (source,target), cur_dir)

	add_cmd_list(cmd_list, 'cp -r usr acrn_release_deb', cur_dir)
	add_cmd_list(cmd_list, 'rm -rf usr', cur_dir)
	add_cmd_list(cmd_list, 'mkdir -p acrn_release_deb/boot', cur_dir)



	for filename in glob.glob(r'acrn_release_img/acrn.*'):
		add_cmd_list(cmd_list, 'cp %s acrn_release_deb/boot' % filename, cur_dir)

	add_cmd_list(cmd_list, 'cp acrn-hypervisor.postinst acrn_release_deb/DEBIAN/postinst', cur_dir)
	add_cmd_list(cmd_list, 'chmod +x acrn_release_deb/DEBIAN/postinst', cur_dir)
	add_cmd_list(cmd_list, 'sed -i \'s/\r//\' acrn_release_deb/DEBIAN/postinst', cur_dir)

	add_cmd_list(cmd_list, 'cp acrn-hypervisor.preinst acrn_release_deb/DEBIAN/preinst', cur_dir)
	add_cmd_list(cmd_list, 'chmod +x acrn_release_deb/DEBIAN/preinst', cur_dir)
	add_cmd_list(cmd_list, 'sed -i \'s/\r//\' acrn_release_deb/DEBIAN/preinst', cur_dir)

	add_cmd_list(cmd_list, 'dpkg -b acrn_release_deb acrn_deb_package.deb ', cur_dir)
	run_cmd_list(cmd_list)
	return



def install_process():

	if load_dict['install_package'] == 'true':
		print('start install package')
		install_compile_package()

	if load_dict['build_acrn'] == 'true':
		print('start build acrn')
		build_acrn()

	if load_dict['build_acrn_kernel'] == 'true':
		print('start build_acrn_kernel')
		build_acrn_kernel(load_dict['service_vm_kernel_repo'],load_dict['kernel_release_version'])

	if load_dict['acrn_deb_package'] == 'true':
		print('start create acrn_deb_package deb')
		create_acrn_deb()

	if load_dict['acrn_kernel_deb_package'] == 'true':
		print('start create acrn_kernel_deb_package deb')
		create_acrn_kernel_deb()

	if load_dict['install_acrn_deb'] == 'true':
		print('start install_acrn_deb')
		install_acrn_deb()


	if load_dict['install_acrn_kernel_deb'] == 'true':
		print('start install install_acrn_kernel_deb')
		install_acrn_kernel_deb()

	if load_dict['auto_reboot'] == 'true':
		print('start reboot')
		os.system("sudo reboot")

if __name__ == "__main__":
		install_process()
