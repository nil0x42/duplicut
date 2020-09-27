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
    max_line_size="$2"
    filter_printable="$3"
    rm -f nonreg_*.out

    if [ $filter_printable -eq 0 ]; then
        timeout 1 $DUPLICUT -l $max_line_size -o nonreg_duplicut.out < $file
        retval="$?"
        $COMPARATOR $file $max_line_size 0 nonreg_comparator.out
    else
        # timeout --foreground 1 $DUPLICUT $file --printable -o nonreg_duplicut.out
        timeout 1 $DUPLICUT -l $max_line_size --printable -o nonreg_duplicut.out < $file
        retval="$?"
        $COMPARATOR $file $max_line_size 1 nonreg_comparator.out
    fi

    if [[ $retval -eq 124 ]]; then
        print_bad "duplicut timed-out on '$file'"
        exit 1
    elif ! diff -q nonreg_*.out 2>&1 > /dev/null; then
        print_bad "Different result on '$file'"
        diff -y <(cat -te nonreg_comparator.out) <(cat -te nonreg_duplicut.out)
        print_bad "Run \`diff nonreg_*.out\` to see differences"
        exit 1
    else
        print_good "wordlist $file passed !"
    fi
}

WORDLISTS=$(find "$WORDLIST_DIR" -maxdepth 1 -type f  \
    -name '*.txt' -printf "%f\n" | sort)


print_info "testing wordlists without special filters"
for wordlist in $WORDLISTS; do
    test_wordlist "$wordlist" 14 0
done
print_info "testing wordlists with --printable filter"
for wordlist in $WORDLISTS; do
    test_wordlist "$wordlist" 14 1
done

rm -f nonreg_*.out
