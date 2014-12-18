#!/bin/bash

# check user interface

set -ve

./duplicut > /dev/null
./duplicut --help > /dev/null

./duplicut --unexistent-argument 2>&1 | grep -q unrecognized

! ./duplicut --version > /dev/null
./duplicut --version | grep -Eq "duplicut.+github"
