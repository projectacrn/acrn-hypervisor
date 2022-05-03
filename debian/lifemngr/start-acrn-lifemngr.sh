#!/bin/sh
# helper to start ACRN lifemnr according to VM type

# must coincide with fixed coding in
# misc/services/life_mngr/config.h
LIFE_MNGR_CONFIG_PATH="/etc/life_mngr/life_mngr.conf"
# distinguish service/user VM
LIFEMNGR_VM=${LIFEMNGR_VM:-$(if [ -c /dev/acrn_hsm ]; then echo service_vm; else echo user_vm; fi)}

# eventually install default configuration
if [ ! -f ${LIFE_MNGR_CONFIG_PATH} ]; then
    mkdir -p $(dirname ${LIFE_MNGR_CONFIG_PATH})
    cp /usr/share/acrn-lifemngr/life_mngr.conf.${LIFEMNGR_VM} ${LIFE_MNGR_CONFIG_PATH}
fi

exec /usr/bin/acrn-lifemngr
