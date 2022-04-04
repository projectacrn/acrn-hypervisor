#!/bin/sh
# helper to start ACRN lifemnr according to VM type

LIFEMNGR_CONF=/usr/share/acrn-lifemngr/acrn-lifemngr.conf

# eventually include configuration for overriding default configuration
if [ -f ${LIFEMNGR_CONF} ]; then
    . ${LIFEMNGR_CONF}
fi

LIFEMNGR_VM=${LIFEMNGR_VM:-$(if [ -c /dev/acrn_hsm ]; then echo sos; else echo uos; fi)}
LIFEMNGR_TTY=${LIFEMNGR_TTY:-/dev/ttyS1}

/usr/bin/acrn-lifemngr ${LIFEMNGR_VM} ${LIFEMNGR_TTY}
