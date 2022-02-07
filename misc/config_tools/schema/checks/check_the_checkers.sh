#!/bin/bash

# Copyright (C) 2022 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

checks_dir=$(dirname $0)
for checker in ${checks_dir}/*.xsd; do
    xmllint --noout --schema ${checks_dir}/schema_of_checks/main.xsd ${checker}
done
