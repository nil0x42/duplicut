#!/bin/bash

# differential regression tests against a Python reference implementation

set -v

# `gtimeout` fallback for old macos/xcode
timeout=timeout
which $timeout &>/dev/null || timeout=gtimeout

DUPLICUT="./duplicut"
REFERENCE="./test/scripts/reference-dedupe.py"

WORDLIST_DIR="./test/wordlists"

function print_info () {
    echo -e "\033[1;34m[*]\033[0;36m $1\033[0m"
}
function print_good () {
    echo -e "\033[1;32m[+]\033[0;32m $1\033[0m"
}
function print_bad () {
    echo -e "\033[1;31m[-]\033[0;31m $1\033[0m"
}

function test_wordlist ()
{
    file="$WORDLIST_DIR/$1"
    shift 1
    args="$@"
    rm -f differential-unique-*.out
    rm -f differential-duplicates-*.out
    p="[DIFFERENTIAL CASE] duplicut $args < $file:"

    $timeout 5 $DUPLICUT -o differential-unique-duplicut.out -D differential-duplicates-duplicut.out -m 64K $args < $file
    retval="$?"
    $REFERENCE $file -o differential-unique-reference.out -D differential-duplicates-reference.out $args
    if [[ -f differential-duplicates-duplicut.out ]]; then
        sort -o differential-duplicates-duplicut.out differential-duplicates-duplicut.out
        sort -o differential-duplicates-reference.out differential-duplicates-reference.out
    fi

    if [[ $retval -eq 124 ]]; then
        print_bad "$p timeout"
        exit 1
    elif ! diff -q differential-unique-*.out 2>&1 > /dev/null; then
        print_bad "$p different unique lines"
        diff -y <(cat -te differential-unique-reference.out) <(cat -te differential-unique-duplicut.out)
        print_bad "Run \`diff differential-unique-*.out\` to see differences"
        exit 1
    elif ! diff -q differential-duplicates-*.out 2>&1 > /dev/null; then
        print_bad "$p different duplicate lines"
        diff -y <(cat -te differential-duplicates-reference.out) <(cat -te differential-duplicates-duplicut.out)
        print_bad "Run \`diff differential-duplicates-*.out\` to see differences"
        exit 1
    else
        print_good "$p OK !"
    fi
}

WORDLISTS=$(find "$WORDLIST_DIR" -maxdepth 1 -type f  \
    -name '*.txt' -exec basename {} ';' | sort)

for wordlist in $WORDLISTS; do
    for size in 1 5 14 15 40 65 128 2000 4095; do
        test_wordlist "$wordlist" -l $size
        test_wordlist "$wordlist" -l $size -p
        test_wordlist "$wordlist" -l $size -c
        test_wordlist "$wordlist" -l $size -p -c
        test_wordlist "$wordlist" -l $size -C
        test_wordlist "$wordlist" -l $size -p -C
    done
done

rm -f differential-unique-*.out
