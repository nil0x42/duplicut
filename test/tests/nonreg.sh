#!/bin/bash

# compare output with python duplicate remover and common wordlists

set -v

DUPLICUT="./duplicut"
COMPARATOR="./test/scripts/remove-duplicates.py"

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
    rm -f nonreg_*.out
    p="[CMP] duplicut $args < $file:"

    timeout 1 $DUPLICUT -o nonreg_duplicut.out $args < $file
    retval="$?"
    $COMPARATOR $file -o nonreg_comparator.out $args

    if [[ $retval -eq 124 ]]; then
        print_bad "$p timeout"
        exit 1
    elif ! diff -q nonreg_*.out 2>&1 > /dev/null; then
        print_bad "$p different result"
        diff -y <(cat -te nonreg_comparator.out) <(cat -te nonreg_duplicut.out)
        print_bad "Run \`diff nonreg_*.out\` to see differences"
        exit 1
    else
        print_good "$p OK !"
    fi
}

WORDLISTS=$(find "$WORDLIST_DIR" -maxdepth 1 -type f  \
    -name '*.txt' -printf "%f\n" | sort)

for wordlist in $WORDLISTS; do
    for size in 1 5 14 15 40 64 65 128 255; do
        test_wordlist "$wordlist" -l $size
        test_wordlist "$wordlist" -l $size -p
    done
done

rm -f nonreg_*.out
