#!/bin/bash

# system regression tests for stdout and shell redirections

set -ve

program="$(pwd)/duplicut"
scripts="$(pwd)/test/scripts"
tmpdir=$(mktemp -d "$(pwd)/duplicut-stdout.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT
cd "$tmpdir"

printf 'alpha\nbeta\nalpha\n' > input
printf 'alpha\nbeta\n' > expected
: > empty

for mode in default stdout; do
    options=()
    if [[ "$mode" == "stdout" ]]; then
        options=(-o /dev/stdout)
    fi

    # > replaces existing contents, >> appends exact bytes
    printf 'old output\n' > output
    "$program" input "${options[@]}" > output
    cmp output expected
    "$program" input "${options[@]}" >> output
    cat expected expected > appended
    cmp output appended
    rm output
    "$program" input "${options[@]}" >> output
    cmp output expected
    printf 'prefix' > output
    printf 'prefix' > appended
    cat expected expected >> appended
    "$program" input "${options[@]}" >> output
    "$program" input "${options[@]}" >> output
    cmp output appended

    # following commands must see stdout positioned after the result
    ("$program" input "${options[@]}"; printf 'footer') > output
    cat expected > appended
    printf 'footer' >> appended
    cmp output appended
    (printf 'header'; "$program" input "${options[@]}"; printf 'footer') > output
    printf 'header' > appended
    cat expected >> appended
    printf 'footer' >> appended
    cmp output appended

    # empty results must leave append destinations unchanged
    "$program" empty "${options[@]}" >> output
    "$program" input -l 1 "${options[@]}" >> output
    cmp output appended
    "$program" empty "${options[@]}" > output
    test ! -s output

    # stdin, pipes and character devices
    "$program" "${options[@]}" < input > output
    cmp output expected
    "$program" input "${options[@]}" | cat > output
    cmp output expected
    "$program" input "${options[@]}" > /dev/null

    # stdout aliases must be rejected before truncating duplicates
    for pair in infile-outfile outfile-dupfile; do
        for link in same hardlink symlink; do
            cp input target
            printf 'keep duplicates\n' > duplicates
            case "$link" in
                same) alias=target ;;
                hardlink) alias=alias; ln target "$alias" ;;
                symlink) alias=alias; ln -s target "$alias" ;;
            esac
            if [[ "$pair" == "infile-outfile" ]]; then
                if "$program" target -D duplicates "${options[@]}" >> "$alias"; then
                    echo "conflicting stdout accepted: $pair ($link)" >&2
                    exit 1
                fi
                printf 'keep duplicates\n' > saved
                cmp duplicates saved
            else
                if "$program" input -D "$alias" "${options[@]}" >> target; then
                    echo "conflicting stdout accepted: $pair ($link)" >&2
                    exit 1
                fi
            fi
            cmp target input
            if [[ "$link" != "same" ]]; then
                rm "$alias"
            fi
        done
    done

    # invalid stdout and dupfile must fail without changing output
    if "$program" input "${options[@]}" >&-; then
        echo "closed stdout accepted" >&2
        exit 1
    fi
    if "$program" input "${options[@]}" 1< input; then
        echo "read-only stdout accepted" >&2
        exit 1
    fi
    cp input output
    if "$program" input -D . "${options[@]}" >> output; then
        echo "directory accepted as dupfile" >&2
        exit 1
    fi
    cmp output input
    if [[ -e /dev/full ]]; then
        if "$program" input "${options[@]}" > /dev/full; then
            echo "stdout write failure ignored" >&2
            exit 1
        fi
    fi
done

# -o takes precedence over stdout, even when stdout is closed
"$program" input -o output > unused
cmp output expected
test ! -s unused
"$program" input -o output >&-
cmp output expected

python3 "$scripts/stdout.py" "$program"
test -z "$(find . -name 'duplicut_tmpfile.*' -print)"
