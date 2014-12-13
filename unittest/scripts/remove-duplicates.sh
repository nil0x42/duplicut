#!/bin/bash

ROOTDIR="$(git rev-parse --show-toplevel)"
source "$ROOTDIR/unittest/scripts/context-loader.sh"

# This script removes duplicates the same way duplicut does.
#
# It is intended to serve as a duclicate remover reference in
# order to ensure that duplicut works properly in the test suite.

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <wordlist>"
    exit 1;
fi

cat "$1" | awk ' !x[$0]++' \
    | perl -ne 'print unless length() > 24 || m/^\0/'
