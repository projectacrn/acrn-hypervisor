# -*- coding: utf-8 -*-
#* Copyright (c) 2020-2022 Intel Corporation.
import os,sys,copy,json
import subprocess
import datetime
import time

os.system('sudo -E apt-get install iasl')

if os.path.exists('iasl_build'):
	os.system('rm -rf iasl_build')
os.system('mkdir -p iasl_build')

cmd = "cd iasl_build" + "&&" +"wget https://acpica.org/sites/acpica/files/acpica-unix-20191018.tar.gz"
os.system(cmd)

cmd = "cd iasl_build" + "&&" +"tar zxvf acpica-unix-20191018.tar.gz"
os.system(cmd)

cmd = "cd iasl_build/acpica-unix-20191018" + "&&" +"make clean"
os.system(cmd)

cmd = "cd iasl_build/acpica-unix-20191018" + "&&" +"make iasl"
os.system(cmd)

cmd = "cd iasl_build/acpica-unix-20191018" + "&&" +"cp ./generate/unix/bin/iasl /usr/sbin/"
