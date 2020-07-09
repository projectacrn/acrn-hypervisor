.. _ubuntu_sos:

ubuntu_sos
##########

Description
***********
this scripts show how to build acrn-hypervisor and acrn-kernel deb package

1. setup native ubuntu 18.04
=============================
Base on GEO; config your proxy

   .. code-block:: none

  sudo apt install python3

  sudo su


2. config release.json file
============================
below is explanation for items in release.json,please change item accordingly,if do not change means go with default value

 .. code-block:: none

  {

	"//":"release ubuntu as sos verion",

	"install_package":"true",---------->this 1st time should set to true if you did not install acrn related compile package, if already install,set to false

	"gcc_version":"7.3.0",---------->gcc version should higher than 7.3.0

	"binutils":"2.27",---------->binutils version should higher than 2.27

	"//":"acrn-hypervisor config",

	"build_acrn":"true",---------->if you need build acrn-hypervisor set to true

	"sync_acrn_code":"true",---------->if you need sync acrn-hypervisor code set to true

	"acrn_repo":"https://github.com/projectacrn/acrn-hypervisor.git",---------->acrn repo

	"release_version":"remotes/origin/release_2.0",---------->acrn release branch

	"acrn_deb_package":"true",---------->if you need create acrn-hypervison deb set to true

	"install_acrn_deb":"false",---------->if you need install acrn-hypervisor deb set to true

	"build_cmd":---------->acrn-hypervisor build command must include scenario and board info and release type

	{

		"scenario":

		{

			"industry":"true",

			"hybrid":"true",

			"logical_partition":"true"


		},

		"board":

		{

			"nuc7i7dnb":"true",

			"whl-ipc-i5":"true"

		},

		"release":"0"---------->0 means debug version,1 means release version

	},

	"//":"kernel config",

	"build_acrn_kernel":"true",---------->if you need build acrn-kernel set to true

	"sync_acrn_kernel_code":"true",---------->if you need sync acrn-kernel code set to true

	"kernel_release_version":"remotes/origin/release_2.0",---------->acrn kernel release branch


	"sos_kernel_repo":"https://github.com/projectacrn/acrn-kernel.git",---------->acrn kernel repo


	"acrn_kernel_deb_package":"true",---------->if you need create acrn-kernel deb set to true

	"install_acrn_kernel_deb":"false",---------->if you need install acrn-kernel deb set to true

	"//":"misc",

	"auto_reboot":"false"---------->if you need reboot set to true

 }

3.python3 install_uSoS.py
=========================
after finished , will get below two item

acrn_deb_package.deb

acrn_kernel_deb_package.deb


install command

sudo dpkg -i acrn_deb_package.deb

sudo dpkg -i acrn_kernel_deb_package.deb


uninstall

sudo dpkg -r acrn-package

sudo dpkg -r acrn-kernel-package

4.python3 compile_iasl.py
=========================
this scriptrs is help compile iasl and cp to /usr/sbin
