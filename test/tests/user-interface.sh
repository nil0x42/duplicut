#!/bin/bash

# check user interface

set -ve

./duplicut > /dev/null
./duplicut --help > /dev/null

./duplicut --unexistent-argument 2>&1 | grep -q "unrecognized"

! ./duplicut --version > /dev/null
./duplicut --version | grep -Eq "duplicut.+github"

# -l max value is 255
./duplicut /etc/passwd -o /tmp/x -l 255 &> /dev/null
! ./duplicut /etc/passwd -o /tmp/x -l 256 &> /dev/null
./duplicut /etc/passwd -o /tmp/x -l 256 2>&1 | grep -q "max value is 255"

# -c can't be used together with -C (lowere/upper case)
./duplicut /etc/passwd -o /tmp/x -c &> /dev/null
./duplicut /etc/passwd -o /tmp/x -C &> /dev/null
! ./duplicut /etc/passwd -o /tmp/x -c -C &> /dev/null
./duplicut /etc/passwd -o /tmp/x -c -C 2>&1 | grep -q "cannot use '--lowercase' and '--uppercase' together"
