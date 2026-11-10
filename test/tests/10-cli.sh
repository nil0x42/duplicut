#!/bin/bash

# black-box system tests for command-line options

# faketty COMMAND
faketty() {
    if command -v unbuffer > /dev/null; then
        # for macos (brew install expect)
        unbuffer -p $@
    else
        script -eqfc "$(printf "%q " "$@")" /dev/null
    fi
}

set -ve

tmpdir=$(mktemp -d "/tmp/duplicut-cli.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT

# -o is optional, including with empty stdin
< /dev/null ./duplicut > "$tmpdir/output"
test ! -s "$tmpdir/output"

faketty ./duplicut > /dev/null
./duplicut --help > /dev/null

./duplicut --unexistent-argument 2>&1 | grep -q "unrecognized"

! ./duplicut --version > /dev/null
./duplicut --version | grep -Eq "duplicut.+github"

# -l max value is 4095
./duplicut /etc/passwd -o "$tmpdir/output" -l 4095 &> /dev/null
! ./duplicut /etc/passwd -o "$tmpdir/output" -l 4096 &> /dev/null
./duplicut /etc/passwd -o "$tmpdir/output" -l 4096 2>&1 | grep -q "max value is 4095"

# -c can't be used together with -C (lowere/upper case)
./duplicut /etc/passwd -o "$tmpdir/output" -c &> /dev/null
./duplicut /etc/passwd -o "$tmpdir/output" -C &> /dev/null
! ./duplicut /etc/passwd -o "$tmpdir/output" -c -C &> /dev/null
./duplicut /etc/passwd -o "$tmpdir/output" -c -C 2>&1 | grep -q "cannot use '--lowercase' and '--uppercase' together"

# -D is optional
./duplicut /etc/passwd -o "$tmpdir/output"
./duplicut /etc/passwd -o "$tmpdir/output" -D "$tmpdir/duplicates"
! ./duplicut /etc/passwd -o "$tmpdir/unique" -D /
