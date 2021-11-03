#!/bin/bash
# Copyright (C) 2021 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# Create top-level website redirect to a doc (conf.py redirect script can only
# create redirects within the published folder output, e.g. latest/ or 2.6/)
#
#  publish-redirect docname.html destpath/docname.html

if [[ $# -ne 2 ]]; then
    echo "Error: $0 expects two parameters: docname.html destpath/docname.html" >&2
    exit 1
fi

cat>"$1"<<EOF
<html>
  <head>
    <title>ACRN Hypervisor documentation Redirect</title>
    <meta http-equiv="refresh" content="0; URL=$2">
    <script>
      window.location.href = "$2"
    </script>
  </head>
  <body>
    <p>Please visit the <a href="/latest/">latest ACRN documentation</a></p>
  </body>
</html>
EOF
