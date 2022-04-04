#!/bin/sh

# helper to compare version
verlte() {
    [  "$1" = "$(echo -e "$1\n$2" | sort -V | head -n1)" ]
}

lintian_version=$(lintian --version | awk '{print $2}')

# Explicitly use --fail-on error if available (since 2.77.0)
# This circumvents a problem in bullseye lintian, where failing on error
# is not the default action any more
# Always use debian profile for lintian
if $(verlte 2.77.0 ${lintian_version}); then
    LINTIAN_ARGS="--profile debian --fail-on error"
else
    LINTIAN_ARGS="--profile debian"
fi

echo "lintian ${LINTIAN_ARGS} $1"
lintian ${LINTIAN_ARGS} $1
status=$?

if [ $status -ne 0 ]; then
    echo "+++ LINTIAN ERRORS DETECTED +++" >&2
fi

exit $status
