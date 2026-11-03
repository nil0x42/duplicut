#!/bin/bash

# ensure conflicting files are rejected before truncation

set -ve

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

for pair in infile-outfile infile-dupfile outfile-dupfile; do
    for link in same hardlink symlink; do
        dir="$tmpdir/$pair-$link"
        mkdir "$dir"
        infile="$dir/input"
        outfile="$dir/output"
        dupfile="$dir/duplicates"
        printf 'alpha\nbeta\nalpha\n' > "$infile"

        case "$pair" in
            infile-outfile)
                target="$infile"
                alias="$outfile"
                printf 'keep duplicates\n' > "$dupfile"
                ;;
            infile-dupfile)
                target="$infile"
                alias="$dupfile"
                printf 'keep output\n' > "$outfile"
                ;;
            outfile-dupfile)
                printf 'keep output\n' > "$outfile"
                target="$outfile"
                alias="$dupfile"
                ;;
        esac

        case "$link" in
            same)
                if [[ "$pair" == "infile-outfile" ]]; then
                    outfile="$target"
                else
                    dupfile="$target"
                fi
                ;;
            hardlink) ln "$target" "$alias" ;;
            symlink) ln -s "$target" "$alias" ;;
        esac

        cp "$infile" "$dir/input.orig"
        cp "$outfile" "$dir/output.orig"
        cp "$dupfile" "$dir/duplicates.orig"
        if ./duplicut "$infile" -o "$outfile" -D "$dupfile"; then
            echo "conflicting files accepted: $pair ($link)" >&2
            exit 1
        fi
        cmp "$infile" "$dir/input.orig"
        cmp "$outfile" "$dir/output.orig"
        cmp "$dupfile" "$dir/duplicates.orig"
    done
done

# new output paths may also point to the same file
infile="$tmpdir/input"
printf 'alpha\nbeta\nalpha\n' > "$infile"
cp "$infile" "$tmpdir/input.orig"
for link in same relative symlink; do
    outfile="$tmpdir/new-$link"
    case "$link" in
        same) dupfile="$outfile" ;;
        relative) dupfile="$tmpdir/./new-$link" ;;
        symlink)
            dupfile="$tmpdir/link"
            ln -s "$outfile" "$dupfile"
            ;;
    esac
    if ./duplicut "$infile" -o "$outfile" -D "$dupfile"; then
        echo "conflicting new output files accepted: $link" >&2
        exit 1
    fi
    cmp "$infile" "$tmpdir/input.orig"
done

# failing to open dupfile must not truncate an existing outfile
printf 'keep output\n' > "$tmpdir/output"
cp "$tmpdir/output" "$tmpdir/output.orig"
if ./duplicut "$infile" -o "$tmpdir/output" -D "$tmpdir"; then
    echo "directory accepted as dupfile" >&2
    exit 1
fi
cmp "$infile" "$tmpdir/input.orig"
cmp "$tmpdir/output" "$tmpdir/output.orig"

# successful runs must still truncate previous output contents
printf 'alpha\nbeta\n' > "$tmpdir/expected"
printf 'alpha\n' > "$tmpdir/expected-dupes"
printf 'keep duplicates\n' > "$tmpdir/duplicates"
./duplicut "$infile" -o "$tmpdir/output" -D "$tmpdir/duplicates"
cmp "$tmpdir/output" "$tmpdir/expected"
cmp "$tmpdir/duplicates" "$tmpdir/expected-dupes"

: > "$tmpdir/empty"
./duplicut "$tmpdir/empty" -o "$tmpdir/output" -D "$tmpdir/duplicates"
test ! -s "$tmpdir/output"
test ! -s "$tmpdir/duplicates"
