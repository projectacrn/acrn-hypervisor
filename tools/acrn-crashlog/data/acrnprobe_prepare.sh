#!/bin/bash
#
# Copyright (C) <2018> Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

core_pattern_conf="/proc/sys/kernel/core_pattern"
crashlog_path="/var/log/crashlog"

if [ ! -d $crashlog_path ]; then
	mkdir -p $crashlog_path
fi

# backup the default core_pattern
cat $core_pattern_conf > /var/log/crashlog/default_core_pattern

# update the content of core_pattern, passdown all the parameters
echo "|/usr/bin/usercrash-wrapper %E %P %u %g %s %t %c %h %e %p %i %I %d" > $core_pattern_conf

default_conf="/usr/share/defaults/telemetrics/telemetrics.conf"
user_conf="/etc/telemetrics/telemetrics.conf"

grep -q "record_server_delivery_enabled=false" $user_conf
if [ "$?" -eq "0" ];then
	exit;
fi

telemd_services=(
hprobe.timer
telemd-update-trigger.service
pstore-clean.service
pstore-probe.service
oops-probe.service
klogscanner.service
journal-probe.service
bert-probe.service
)

mkdir -p $(dirname $user_conf)
cp $default_conf $user_conf

for ((i=0;i<${#telemd_services[*]};i++))
do
	if [ ! -L "/etc/systemd/system/${telemd_services[$i]}" ];then
		systemctl mask ${telemd_services[$i]} --now
	fi
done

# modify the configure file

sed -i "s/server_delivery_enabled=true/server_delivery_enabled=false/g" $user_conf
sed -i "s/record_retention_enabled=false/record_retention_enabled=true/g" $user_conf

# restart telemd
sleep 3
systemctl restart telemd.service
