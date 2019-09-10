#!/bin/bash

# run the filter-known-issues.py script to remove "expected" warning
# messages from the output of the document build process and write
# the filtered output to stdout
#
# Only argument is the name of the log file saved by the build.

KI_SCRIPT=scripts/filter-known-issues.py
CONFIG_DIR=.known-issues/doc

LOG_FILE=$1
BUILDDIR=$(dirname $LOG_FILE)

if [ -z "${LOG_FILE}" ]; then
        echo "Error in $0: missing input parameter <logfile>"
        exit 1
fi

# When running in background, detached from terminal jobs, tput will
# fail; we usually can tell because there is no TERM env variable.
if [ -z "${TERM:-}" -o "${TERM:-}" = dumb ]; then
    TPUT="true"
    red=''
    green=''
else
    TPUT="tput"
    red='\E[31m'
    green='\e[32m'
fi

if [ -s "${LOG_FILE}" ]; then
   $KI_SCRIPT --config-dir ${CONFIG_DIR} ${LOG_FILE} > ${BUILDDIR}/doc.warnings 2>&1
   if [ -s doc.warnings ]; then
	   echo
	   echo -e "${red}New errors/warnings found, please fix them:"
	   echo -e "=============================================="
	   $TPUT sgr0
	   echo
	   cat doc.warnings
	   echo
	   exit 1
   else
	   echo -e "${green}No new errors/warnings."
	   $TPUT sgr0
   fi

else
   echo "Error in $0: logfile \"${LOG_FILE}\" not found."
   exit 1
fi
