#!/bin/bash
# Copyright (C) 2022 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

set -e

usage() {
  echo "Usage: $0 [--board_list ACRN_BOARDLIST] [--scenario_list ACRN_SCENARIOLIST] [--config_path CONFIGDIRS] [--release n|y] [acrn | board_inspector | clean]"
  echo "Optional arguments:"
  echo "  -h, --help           show this help message and exit"
  echo "  -b, --board_list     list the boards to build, seperated by blank; build all scanned boards in the config path if specified as \"\"; build the default boards in debian rules if not specified"
  echo "  -s, --scenario_list  list the scenarios to build, seperated by blank; build all scanned scenarios in the config path if specified as \"\"; build the default scenarios in debian rules if not specified"
  echo "  -c, --config_path    specify the config path for the board and scenario configuration files, default use misc/config_tools/data if not specified"
  echo "  -r, --release        build debug version with n, release version with y; default defined in debian rules if not specified"
  echo "  acrn|board_inspector|clean    specify the build target, default value is acrn if not specified"
  echo "Examples: "
  echo "  $0"
  echo "  $0 -b nuc11tnbi5 -s shared"
  echo "  $0 -b \"nuc11tnbi5 tgl-vecow-spc-7100-Corei7\" -s \"shared hybrid\" -c misc/config_tools/data -r y"
  echo "  $0 -b \"\" -s shared"
  echo "  $0 board_inspector"
}

invalid() {
  echo "ERROR: Unrecognized argument: $1" >&2
  usage
  exit 1
}

verify_cmd() {
  command -v $@ >/dev/null 2>&1 || { echo >&2 "ERROR: $@ is not installed which is required for running this script. Aborting."; exit 1; }
}

verify_cmd readlink
verify_cmd debuild
verify_cmd "gbp dch"

POSITIONAL_ARGS=()

board_list="default"
scenario_list="default"
config_path="misc/config_tools/data"
release="default"
while [[ $# -gt 0 ]]; do
  case $1 in
    -b|--board_list)
      board_list="$2"
      shift 2
      ;;
    -s|--scenario_list)
      scenario_list="$2"
      shift 2
      ;;
    -c|--config_path)
      config_path="$2"
      shift 2
      ;;
    -r|--release)
      release="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -*|--*)
      invalid $1
      ;;
    *)
      POSITIONAL_ARGS+=("$1")
      shift
      ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}"

cmd="debuild"
if [ "$board_list" != "default" ]; then
  echo "ACRN_BOARDLIST    = ${board_list@Q}"
  cmd="$cmd -eACRN_BOARDLIST=${board_list@Q}"
fi
if [ "$scenario_list" != "default" ]; then
  echo "ACRN_SCENARIOLIST = ${scenario_list@Q}"
  cmd="$cmd -eACRN_SCENARIOLIST=${scenario_list@Q}"
fi
cmd="$cmd -eCONFIGDIRS=${config_path@Q}"
echo "CONFIGDIRS        = ${config_path@Q}"
if [ "$release" != "default" ]; then
  echo "RELEASE           = ${release@Q}"
  if [ "$release" != "n" ] && [ "$release" != "y" ]; then
    echo "ERROR: the release argument can only be n or y."
    exit 1
  fi
  cmd="$cmd -eRELEASE=${release@Q}"
fi
if [ -z $1 ] || [ "$1" == "acrn" ]; then
  cmd="$cmd -- binary"
elif [ "$1" == "board_inspector"  ]; then
  cmd="$cmd -- binary-indep"
elif [ "$1" == "clean"  ]; then
  cmd="$cmd -- clean"
fi

SCRIPT=$(readlink -f "$0")
SCRIPT_PATH=$(dirname "$SCRIPT")
cd $SCRIPT_PATH/../
source VERSION

rm -rf debian/changelog
if [ -z $EMAIL ] && [ -z $DEBEMAIL]; then
  export DEBEMAIL=$(git config --get user.email)
  if [ -z $DEBEMAIL ]; then
    export DEBEMAIL="projectacrn@gmail.com"
  fi
fi
gbp dch -S --git-log="-n 10" --id-length=10 --ignore-branch
sed -i "s/unknown/$MAJOR_VERSION.$MINOR_VERSION$EXTRA_VERSION/g" debian/changelog

echo $cmd
echo $cmd | bash -

cd -
