#!/bin/bash

cd `git rev-parse --show-toplevel`

_PROGRAM="./duplicut"
_MAIN_DIR="./test"
_TESTS_DIR="$_MAIN_DIR/tests"

_DEFAULT_TEST=$(find "$_TESTS_DIR" -maxdepth 1 -type f  \
                -name '*.sh' -exec basename {} ';' | sort)

BANNER=$(perl -E 'print "="x79 . "\r\t\t"')

function print_info () {
    echo -e "\033[1;34m[*]\033[0;36m $1\033[0m"
}
function print_good () {
    echo -e "\033[1;32m[+]\033[0;32m $1\033[0m"
}
function print_bad () {
    echo -e "\033[1;31m[-]\033[0;31m $1\033[0m"
}


# print an error message and exit
function die ()
{
    local errmsg="$1"

    echo "[*] CC=\"$CC\""
    echo "[*] FLAGS=\"$FLAGS\""
    echo "[*] TEST=\"$_test\""
    echo "[!] $errmsg" > /dev/stderr
    exit 1
}


function build_program ()
{
    export FLAGS=""
    make coverage || die "Could not compile"

    fileinfo="$(file -b "$_PROGRAM" 2>&1)"
    grep -q "64" <<< "$fileinfo" \
        || die "Expected a 64-bit binary: $_PROGRAM: $fileinfo"
}


scripts=0
errors=0
function execute_scripts () {
    if [ -d "$1" ]; then
        for i in "$1"/*; do
            execute_scripts "$i"
        done
    elif [ -f "$1" ] && [ -x "$1" ]; then
        print_info "$BANNER"
        print_info "RUNNING $1 ..."
        print_info "$BANNER"
        if "$1"; then
            print_good "$1 succeeded"
        else
            print_bad "$1 failed !"
            exit 1
            (( ++errors ))
        fi
        echo -e "\n"
        (( ++scripts ))
    fi
}

[ -z "$TEST" ] && TEST="$_DEFAULT_TEST"

if [[ "$TEST" == "all" ]]; then
    TEST="$(find . -maxdepth 1 -type f -executable)"
fi

print_info "$BANNER"
print_info "BUILDING PROGRAM FOR HOST ..."
print_info "$BANNER"
build_program
for _test in $TEST; do
    _test="$_TESTS_DIR/$_test"
    execute_scripts "$_test"
done

if [[ $errors -eq 0 ]]; then
    print_good "$scripts/$scripts test scripts succeeded"
    exit 0
else
    print_bad "$errors/$scripts test scripts failed"
    exit 1
fi
