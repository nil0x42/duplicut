#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <file-path> <max-line-size>"
    exit 1;
fi

cat "$1" | awk ' !x[$0]++' | awk 'length<='"$2"
