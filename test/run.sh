#!/bin/bash

cd "$(git rev-parse --show-toplevel)" || exit 1

_PROGRAM="./duplicut"
_TESTS_DIR="./test/tests"

BANNER="==============================================================================="

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
    echo "[!] $errmsg" >&2
    exit 1
}


function build_program ()
{
    print_info "$BANNER"
    print_info "BUILDING PROGRAM FOR HOST ..."
    print_info "$BANNER"
    make coverage || die "Could not compile"

    check_program
}


function check_program ()
{
    fileinfo="$(file -b "$_PROGRAM" 2>&1)"
    grep -q "64" <<< "$fileinfo" \
        || die "Expected a 64-bit binary: $_PROGRAM: $fileinfo"
}


tests=()
if [[ -z "$TEST" || "$TEST" == "all" ]]; then
    tests=("$_TESTS_DIR"/*.sh)
else
    for _test in $TEST; do
        tests+=("$_TESTS_DIR/$_test")
    done
fi

[[ ${#tests[@]} -gt 0 ]] || die "No tests selected"
for _test in "${tests[@]}"; do
    [[ -f "$_test" && -x "$_test" ]] || die "Unknown or non-executable test: $_test"
done

DUPLICUT_TEST_BUILD_DIR=$(mktemp -d "$(pwd)/duplicut-tests.XXXXXX") \
    || die "Cannot create test build directory"
export DUPLICUT_TEST_BUILD_DIR
trap 'rm -rf "$DUPLICUT_TEST_BUILD_DIR"' EXIT

export FLAGS=""
scripts=0
for _test in "${tests[@]}"; do
    if [[ $scripts -eq 0 && ! "$_test" -ef "$_TESTS_DIR/00-build.sh" ]]; then
        build_program
    fi
    print_info "$BANNER"
    print_info "RUNNING $_test ..."
    print_info "$BANNER"
    started=$SECONDS
    if ! "$_test"; then
        print_bad "$_test failed !"
        exit 1
    fi
    if [[ "$_test" -ef "$_TESTS_DIR/00-build.sh" ]]; then
        check_program
    fi
    print_good "$_test succeeded ($((SECONDS - started))s)"
    echo -e "\n"
    (( ++scripts ))
done

print_good "$scripts/$scripts test scripts succeeded"
