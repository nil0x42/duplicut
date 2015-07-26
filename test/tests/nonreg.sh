#!/bin/bash

# compare output with python duplicate remover and common wordlists

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
    max_line_size="$2"
    filter_printable="$3"
    rm -f nonreg_*.out

    if [ $filter_printable -eq 0 ]; then
        $DUPLICUT $file -o nonreg_duplicut.out
        $COMPARATOR $file $max_line_size 0 nonreg_comparator.out
    else
        $DUPLICUT $file --printable -o nonreg_duplicut.out
        $COMPARATOR $file $max_line_size 1 nonreg_comparator.out
    fi

    if ! diff -q nonreg_*.out 2> /dev/null; then
        print_bad "Different result on '$file'"
        print_bad "Run vimdiff nonreg_*.out to see differences"
        exit 1
    else
        print_good "wordlist $file passed !"
    fi
}

WORDLISTS=$(find "$WORDLIST_DIR" -maxdepth 1 -type f  \
    -printf "%f\n")


print_info "testing wordlists without special filters"
for wordlist in $WORDLISTS; do
    test_wordlist "$wordlist" 14 0
done
print_info "testing wordlists with --printable filter"
for wordlist in $WORDLISTS; do
    test_wordlist "$wordlist" 14 1
done

rm -f nonreg_*.out
