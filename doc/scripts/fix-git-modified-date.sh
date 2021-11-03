#!/bin/bash
# Copyright (C) 2021 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause
#
# Run this script in the _build/html folder after generating the HTML content.
#
# Scan through HTML files look for the corresponding .rst file (in doc or misc
# for ACRN project). Replace the "Last updated" date in the footer with the last
# commit date for the corresponding .rst file (if we can find it).  Tweak
# wording to mention Last modified and published dates.

cd _build/html

find -type f -name "*.html" | \
    while read filename;
       do
           f=${filename%.*}.rst
           [[ -f "../../$f" ]] && prefix="../.."
           [[ -f "../../../$f" ]] && prefix="../../.."
           d="$(git log -1 --format="%ad" --date=format:"%b %d, %Y" -- "$prefix/$f")";
           [[ ! -z "$d" ]] && sed -i "s/\(^ *<span class=\"lastupdated\">\)Last updated on \(.*\)\.$/\1Last modified: $d. Published: \2./" $filename;
       done
