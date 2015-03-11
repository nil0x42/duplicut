#!/bin/bash

# compare output with python duplicate remover and common wordlists

DUPLICUT="./duplicut"
COMPARATOR="./test/scripts/remove-duplicates.py"

WORDLIST_DIR="./test/wordlists"

function test_wordlist ()
{
    file="$WORDLIST_DIR/$1"
    rm -f nonreg_*.out
    $DUPLICUT $file -o nonreg_duplicut.out
    $COMPARATOR $file 24 nonreg_comparator.out
    if ! diff nonreg_*.out; then
        echo "[-] Different result on '$file'" 2>&1
        echo "[-] Run vimdiff nonreg_*.out to see differences" 2>&1
        exit 1
    fi
}

WORDLISTS=$(find "$WORDLIST_DIR" -maxdepth 1 -type f  \
    -printf "%f\n")

for wordlist in $WORDLISTS; do
    test_wordlist "$wordlist"
done

rm -f nonreg_*.out
