#!/bin/bash
# Create a reST :ref: list of changed documents for the release notes
# comparing the specified tag with master branch
#
#

if [ -z $1 ]; then
   echo
   echo Create a reST :ref: list of change documents for the release notes
   echo comparing the specified tag with the master branch
   echo
   echo Usage:
   echo \ \ changed-docs.sh upstream/release_3.0 [changed amount]
   echo
   echo \ \ where the optional [changed amount] \(default 10\) is the number
   echo \ \ of lines added/modified/deleted before showing up in this report.
   echo
elif [ "$(basename $(pwd))" != "acrn-hypervisor" ]; then
   echo
   echo Script must be run in the acrn-hypervisor directory and not $(basename $(pwd))
else
    dir=`dirname $0`

    git diff --stat `git rev-parse $1` `git rev-parse master` | \
       grep \.rst | \
       awk -v changes=$2 -f $dir/changed-docs.awk
fi
