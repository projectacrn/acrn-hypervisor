#! /bin/bash
# Copyright (C) 2020 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

MAX_ARRAY=6

started="started"
stopped="stopped"
SOS="sos"
UOS="uos"
SHUTDOWN="shutdown"
ACKED="acked"

#config
IP_ADDR="127.0.0.1"
SOCKET_PORT="8193"

#send message by socket
send_message()
{
	message="$1"
	echo "Sending: $message"
	echo -ne "$message" >&6 &
}

#read message by socket
read_message()
{
	read -r -d $'\0' ret_msg <&6
}

#send message by tty
send_pre_message()
{
	message="$1"
	echo "Sending: $message"
	echo -ne "$message" > /dev/ttyS1
}

#read message by tty
read_pre_message()
{
	read -r -d $'\0' ret_msg < /dev/ttyS1
}

power_off_post_vms() {
	vm_list=$(acrnctl list)
	echo $vm_list

	array=($vm_list)
	num=${#array[@]}
	echo "Number of VMs: " $num

	if [ $num -gt $MAX_ARRAY ]; then
		echo "There are no VMs running or acrnctl encountered an internal error."
		return 0
	fi

	#shut down post-launched VMs
	for ((i=0; i<$num; i+=2))
	do
		if [ ${array[$i+1]} == $started ]; then
			echo "Shutting down: " ${array[$i]}
			acrnctl stop ${array[$i]}
			sleep 5s
		fi
	done

	return 1
}

check_post_vms_status() {
	#check post-launched VM status for some time
	check_times=5
	while [[ $check_times > 0 ]]
	do
		vm_list=$(acrnctl list)
		array=($vm_list)
		num=${#array[@]}
		echo "VM status: " $vm_list

		if [ $num -gt $MAX_ARRAY ]; then
			sleep 5s
			let check_times--
			echo "Check #" $check_times
			continue;
		fi

		flag=0

		for ((i=0; i<$num; i+=2))
		do
			if [ ${array[$i+1]} != $stopped ]; then
				flag=1
				break;
			fi
		done

		if [ $flag -eq 1 ]; then
			sleep 5s
			let check_times--
			echo "Check #" $check_times
		else
			echo "VM status: " $vm_list
			break;
		fi

		if [ $check_times -eq 0 ]; then
			echo "Timed out waiting for VMs..."
			break;
		fi
	done

	if [ $check_times -gt 0 ]; then
		return 1;
	else
		return 0;
	fi
}

check_post_vms_alive() {
	#check if there is any post-launched VM alive, and return if there is
	vm_list=$(acrnctl list)
	array=($vm_list)
	num=${#array[@]}

	for ((i=0; i<$num; i+=2))
	do
		if [ ${array[$i+1]} == $started ]; then
			echo $vm_list " VM alive!"
			return 1
		fi
	done

	echo "No VM alive: " $vm_list
	return 0
}

if [ "$1" = "$SOS" ]; then
	try_times=2

	#shut down post-launched VMs
	while [[ $try_times -gt 0 ]]
	do
		echo "Checking whether post-launched VMs are alive..."
		check_post_vms_alive
		if [ $? -eq 0 ]; then
			try_times=1
			break
		fi

		echo "Powering off VMs..."
		power_off_post_vms
		if [ $? -eq 0 ]; then
			break
		fi

		echo "Checking the status of post-launched VMs..."
		check_post_vms_status
		if [ $? -eq 1 ]; then
			break
		fi

		let try_times--
	done

	if [ $try_times -eq 0 ]; then
		echo "S5 failed!"
		exit
	fi

	echo $(acrnctl list)

	#send shutdown message to the pre-launched VM
	send_pre_message $SHUTDOWN
	#read ack message from the pre-launched VM
	read_pre_message
	echo "ret_msg: $ret_msg"

	#check the ack message
	if [ "$ret_msg" = "$ACKED" ]; then
		echo "Received ACK message"
	else
		echo "Could not receive ACK message from the pre-launched User VM"
		exit
	fi

	#all pre-launched and post-launched VMs have shut down
	echo "Shutting down the Service VM itself..."
	sleep 3s
	poweroff

elif [ "$1" = "$UOS" ]; then
	echo "Trying to open socket..."
	if ! exec 6<>/dev/tcp/$IP_ADDR/$SOCKET_PORT
	then
		echo "Failed to open socket"
		exit 1
	fi
	echo "Socket opened"

	#send shutdown message
	send_message $SHUTDOWN
	#read ack message
	read_message
	echo "ret_msg: $ret_msg"

	#check the ack message
	if [ "$ret_msg" = "$ACKED" ]; then
		echo "Received ACK message"
	else
		echo "Could not receive ACK message"
	fi
else
	echo "Error! Please use: ./s5_trigger.sh sos  OR ./s5_trigger.sh uos"
fi
