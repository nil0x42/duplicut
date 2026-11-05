# Usage

[Documentation](README.md) · [Options](../README.md#options) · [Troubleshooting](troubleshooting.md)

Duplicut accepts one input file, or reads stdin when no filename is supplied.
The [README examples](../README.md#options) cover the common commands; this page
explains their behavior.

## Line handling

A line is a byte sequence separated by LF. CRLF is also recognized, with the CR
removed from the line's contents. The final line is accepted without a newline.
Line length is measured in bytes, not Unicode characters; an overlong line is
filtered out rather than truncated. The [option table](../README.md#options)
gives the default and maximum lengths.

Empty lines and lines beginning with a NUL byte are ignored. An embedded NUL is
not itself a delimiter. `-p` accepts only bytes from ASCII space through `~`
(0x20–0x7e); it excludes tabs, control bytes and non-ASCII text.

Output uses LF separators. Newline conventions are not preserved byte for byte:
a missing final LF may be added during compaction.

## Equality and case

By default, comparison is case-sensitive. `Alpha` and `alpha` are different words.
`-c` or `-C` converts case before comparison and writes the converted form. The
two options cannot be combined. These are byte-oriented operations intended for
wordlists, not Unicode case folding.

After conversion and filtering, the earliest occurrence is kept. Later matching
occurrences are duplicates. Filtered lines are counted separately. Both engines
follow these same rules; see [exact matching](architecture.md#hashmaps-and-exact-matches).

## Output files and streams

| Destination | Behavior |
|---|---|
| `-o FILE` | Replace the named file with the result. |
| stdout redirected to a new file | Use the file directly as working storage when possible. |
| stdout in append mode or at a nonzero position | Keep its existing contents and position; append/write the result of this run. |
| stdout connected to a pipe, FIFO or terminal | Process through a temporary working file, then write the result to stdout. |

`-o` takes precedence over stdout redirection. Input, unique output and duplicate
output must refer to different files; aliases through symlinks or hard links are
also checked. Use different input and output names with shell redirections too:
the shell can truncate a destination before Duplicut starts.

Append mode does not deduplicate against the destination's existing contents.
To deduplicate several lists together, concatenate them as input in priority
order, as shown in the README.

The input is copied before processing, so Duplicut does not mark duplicates in
the source. The [shared workflow](architecture.md#working-file-and-output)
explains when the output itself serves as this copy.

## Duplicate output

`-D` writes each removed duplicate occurrence, not one entry per duplicated word.
If a word occurs four times, it contributes three lines to this file. Filtered
lines are not included. Duplicate output follows the selected case conversion.

Unique output order is stable. Duplicate-file order is not guaranteed: mmap
lookup workers may report occurrences concurrently.

## Resource and engine options

Memory values accept integer byte counts and binary suffixes, for example
`100M` and `2G`; units are powers of 1024. Nonempty input requires at least 64 KiB
of effective memory. See [resource accounting](resources.md#working-memory-budget)
for allocation and residency details.

`-t` sets a maximum, not a promise that all workers will stay busy. Values above
the number of online CPUs are rejected. Actual parallelism also depends on the
engine, memory and available work.

Leave `-e auto` for normal use. Forcing an engine is useful for diagnosis and
comparisons; it does not bypass its resource checks. The [selection guide](engine-selection.md)
explains the difference between an initial candidate and a confirmed engine.

## Diagnostics and exit status

Results use stdout or `-o`; diagnostic messages use stderr. Redirecting stderr
keeps a fixed phase history without ANSI sequences. The final report separates
duplicate occurrences from filtered lines. An empty input succeeds with zero
counts and only the phases actually executed.

Success returns zero. Errors return a nonzero status; signals and downstream
pipe failures can also terminate execution. See [progress and error diagnosis](troubleshooting.md).
