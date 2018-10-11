#!/bin/bash

in=$1
out=$2
config=$3

cp $in $out
grep -v "^#" ${config} | while read line; do
    IFS='=' read -ra arr <<<"$line"
    field=${arr[0]}
    value=${arr[1]}
    sed -i "s/\b$field\b/$value/g" $out
done
