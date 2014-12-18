#!/bin/bash


_PROGRAM="./duplicut"
_MAIN_DIR="$(dirname $0)"
_SELF_NAME="$(basename $0)"
_TESTS_DIR="$(dirname $0)/tests"

_DEFAULT_ARCH="x86 x64"

_DEFAULT_TEST=$(find "$_TESTS_DIR" -maxdepth 1 -type f  \
                -executable -printf "%f\n")


# print an error message and exit
function die ()
{
    local errmsg="$1"

    echo "[*] CC=\"$CC\""
    echo "[*] FLAGS=\"$FLAGS\""
    echo "[*] ARCH=\"$_arch\""
    echo "[*] TEST=\"$_test\""
    echo "[!] $errmsg" > /dev/stderr
    exit 1
}


function build_program ()
{
    local _arch="$1"
    local buildscript="$_MAIN_DIR/build.sh"

    case "$_arch" in
        "x86")
            export FLAGS="-m32"
            $buildscript \
                || die "Failed to compile"

            fileinfo="$(file -b "$_PROGRAM" 2>&1)"
            grep -qv "x86-64" <<< "$fileinfo" \
                || die "Bad filetype: $_PROGRAM: $fileinfo"

            ;;
        "x64")
            export FLAGS=""
            $buildscript \
                || die "Failed to compile"

            fileinfo="$(file -b "$_PROGRAM" 2>&1)"
            grep -q "x86-64" <<< "$fileinfo" \
                || die "Bad filetype: $_PROGRAM: $fileinfo"

            ;;
        *)
            die "Invalid architecture: $_arch"
            ;;
    esac
}


# example: run_single_test "x86" "build.sh"
function run_single_test ()
{
    local testscript="$_TESTS_DIR/$1"

    "$testscript" \
        || die "Test script failed: $testscript"
}


[ -z "$ARCH" ] && ARCH="$_DEFAULT_ARCH"
[ -z "$TEST" ] && TEST="$_DEFAULT_TEST"

if [[ "$TEST" == "all" ]]; then
    TEST="$(find . -maxdepth 1 -type f -executable)"
fi

for _arch in $ARCH; do
    build_program "$_arch"
    for _test in $TEST; do
        run_single_test "$_test"
    done
done
