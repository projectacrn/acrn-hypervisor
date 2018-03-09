#!/bin/bash

# Instructions to create systemd-networkd configuration files:

if [ ! -e /etc/systemd/network ]; then
    mkdir -p /etc/systemd/network

    # /etc/systemd/network/acrn.network
    cat <<EOF>/etc/systemd/network/acrn.network
[Match]
Name=e*

[Network]
Bridge=acrn-br0
EOF


    # /etc/systemd/network/acrn.netdev
    cat <<EOF>/etc/systemd/network/acrn.netdev
[NetDev]
Name=acrn-br0
Kind=bridge
EOF

    # /etc/systemd/network/eth.network
    cat <<EOF>/etc/systemd/network/eth.network
[Match]
Name=acrn-br0

[Network]
DHCP=ipv4
EOF

    # need to mask 80-dhcp.network and 80-virtual.network
    ln -s /dev/null /etc/systemd/network/80-dhcp.network
    ln -s /dev/null /etc/systemd/network/80-virtual.network

    # should get specifc list of taps
    # /etc/systemd/network/acrn_tap0.netdev
    cat <<EOF>/etc/systemd/network/acrn_tap0.netdev
[NetDev]
Name=acrn_tap0
Kind=tap
EOF

    # restart systemd-network to create the devices
    # and bind network address to the bridge
    systemctl restart systemd-networkd
fi

# add tap device under the bridge
ifconfig acrn_tap0 up
brctl addif acrn-br0 acrn_tap0
