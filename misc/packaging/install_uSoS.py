# -*- coding: utf-8 -*-
#* Copyright (c) 2020 Intel Corporation
import os,sys,copy,json
import subprocess
import datetime
import time

#parse json file
with open("release.json","r") as load_f:
	load_dict = json.load(load_f)
load_f.close()

with open("deb.json","r") as load_fdeb:
	load_dictdeb = json.load(load_fdeb)
load_fdeb.close()


# install compile package
def install_compile_package():
	#check compile env
	os.system('apt install gcc \
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
	os.system('apt install python-pip -y')
	os.system('pip3 install kconfiglib')
	os.system('apt-get install bison -y')
	os.system('apt-get install flex -y')
	os.system('apt install liblz4-tool -y')


# build acrn
def build_acrn():
	if os.path.exists('acrn_release_img'):
		os.system('rm -rf acrn_release_img')
	os.system('mkdir -p acrn_release_img')

	if load_dict['sync_acrn_code'] == 'true':
		if os.path.exists('acrn-hypervisor'):
			os.system('rm -rf acrn-hypervisor')
		cmd = 'git clone %s' % load_dict['acrn_repo']
		os.system(cmd)
		cmd = 'cd acrn-hypervisor' + "&&" +'git checkout -b mybranch %s'% load_dict['release_version']
		os.system(cmd)


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
					make_cmd = 'make all BOARD_FILE=misc/acrn-config/xmls/board-xmls/%s.xml SCENARIO_FILE=misc/acrn-config/xmls/config-xmls/%s/%s.xml RELEASE=%s'%(j,j,i,release)
					make_cmd_list.append(make_cmd)

	for i in range(len(make_cmd_list)):
		cmd = 'cd acrn-hypervisor' + "&&" +'make clean'
		os.system(cmd)

		cmd = 'cd acrn-hypervisor' + "&&" +'%s'% make_cmd_list[i]
		os.system(cmd)

		bin_name ='acrn.%s.%s.bin' % (info_list[i][0],info_list[i][1])
		out_name ='acrn.%s.%s.32.out' % (info_list[i][0],info_list[i][1])
		efi_name ='acrn.%s.%s.efi' % (info_list[i][0],info_list[i][1])

		cmd = 'cp %s acrn_release_img/%s' %(load_dictdeb['acrn.bin']['source'],bin_name)
		os.system(cmd)

		cmd = 'cp %s acrn_release_img/%s' %(load_dictdeb['acrn.32.out']['source'],out_name)
		os.system(cmd)

		if os.path.exists(load_dictdeb['acrn.efi']['source']):
				cmd = 'cp %s acrn_release_img/%s' %(load_dictdeb['acrn.efi']['source'],efi_name)
				os.system(cmd)

def create_acrn_kernel_deb():

	if os.path.exists('acrn_kernel_deb'):
		os.system('rm -rf acrn_kernel_deb')

	os.system('mkdir -p acrn_kernel_deb')

	cmd = "cd acrn_kernel_deb" + "&&" +"mkdir DEBIAN"
	os.system(cmd)

	cmd = "cd acrn_kernel_deb" + "&&" +"touch DEBIAN/control"
	os.system(cmd)

	#control file description

	listcontrol=['Package: acrn-kernel-package\n','version: %s \n'% datetime.date.today(),'Section: free \n','Priority: optional \n','Architecture: amd64 \n','Installed-Size: 66666 \n','Maintainer: Intel\n','Description: sos_kernel \n','\n']


	with open('acrn_kernel_deb/DEBIAN/control','w',encoding='utf-8') as fr:
			fr.writelines(listcontrol)

	cmd = 'cd acrn-kernel' + '&&' + 'ls *.gz'
	filename = os.popen(cmd).read().replace('\n', '').replace('\r', '')
	cmd = 'cp acrn-kernel/%s acrn_kernel_deb/' % filename
	os.system(cmd)

	cmd = 'cd acrn_kernel_deb' + '&&' + 'tar -zvxf %s' % filename
	os.system(cmd)


	cmd = 'cd acrn_kernel_deb/boot' + '&&' + 'ls vmlinuz*'
	version = os.popen(cmd)

	f = open("acrn_kernel_deb/boot/version.txt",'w')
	f.write(version.read())
	f.close()

	os.system('cp acrn-kernel.postinst acrn_kernel_deb/DEBIAN/postinst' )

	os.system('chmod +x acrn_kernel_deb/DEBIAN/postinst')

	os.system('sed -i \'s/\r//\' acrn_kernel_deb/DEBIAN/postinst')

	os.system('rm acrn_kernel_deb/%s' % filename)

	os.system('dpkg -b acrn_kernel_deb acrn_kernel_deb_package.deb ')


def build_acrn_kernel(acrn_repo,acrn_version):
	if load_dict['sync_acrn_kernel_code'] == 'true':
		if os.path.exists('acrn-kernel'):
			os.system('rm -rf acrn-kernel')

		os.system('git clone %s' % acrn_repo)

		cmd = 'cd acrn-kernel' + "&&" +'git checkout %s'% acrn_version
		os.system(cmd)

	cmd = 'cd acrn-kernel' + "&&" +'make clean'
	os.system(cmd)
	# build kernel
	cmd = 'cd acrn-kernel' + "&&" +'cp kernel_config_uefi_sos .config'
	os.system(cmd)

	cmd = 'cd acrn-kernel' + "&&" +'make olddefconfig'
	os.system(cmd)

	cmd = 'cd acrn-kernel' + "&&" +'make targz-pkg -j4'
	os.system(cmd)



#install acrn
def install_acrn_deb():
	os.system('dpkg -r acrn-package')
	os.system('dpkg -i acrn_deb_package.deb ')


#install kernel
def install_acrn_kernel_deb():
	os.system('dpkg -r acrn-kernel-package ')
	os.system('dpkg -i acrn_kernel_deb_package.deb ')


def create_acrn_deb():

	path = 'acrn_release_deb'
	if os.path.exists(path):
		os.system('rm -rf acrn_release_deb')
	os.system('mkdir -p acrn_release_deb')
	cmd = "cd acrn_release_deb" + "&&" +"mkdir DEBIAN"
	os.system(cmd)

	cmd = "cd acrn_release_deb" + "&&" +"touch DEBIAN/control"
	os.system(cmd)

	#control file description
	acrn_info = load_dict['release_version']

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
			os.system('cp %s %s' % (source,target))
		else:
			os.system('mkdir -p %s' % target)
			os.system('cp %s %s' % (source,target))

	os.system('cp -r usr acrn_release_deb')
	os.system('rm -rf usr')

	os.system('mkdir -p acrn_release_deb/boot')
	cmd = "cp acrn_release_img/acrn.* acrn_release_deb/boot"
	os.system(cmd)


	os.system('cp acrn-hypervisor.postinst acrn_release_deb/DEBIAN/postinst' )
	os.system('chmod +x acrn_release_deb/DEBIAN/postinst')
	os.system('sed -i \'s/\r//\' acrn_release_deb/DEBIAN/postinst')

	os.system('dpkg -b acrn_release_deb acrn_deb_package.deb ')



def install_process():

	if load_dict['install_package'] == 'true':
		print('start install package')
		install_compile_package()

	if load_dict['build_acrn'] == 'true':
		print('start build acrn')
		build_acrn()

	if load_dict['build_acrn_kernel'] == 'true':
		print('start build_acrn_kernel')
		build_acrn_kernel(load_dict['sos_kernel_repo'],load_dict['kernel_release_version'])

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

if __name__ == "__main__":
		install_process()
