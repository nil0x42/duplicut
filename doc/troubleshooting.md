# Troubleshooting

[Documentation](README.md) · [Usage](usage.md) · [Resources](resources.md)

Start with the phase history and technical summary on stderr. They distinguish
work already completed, the active phase and the resources actually selected.

## Reading the progress

| Group | Meaning |
|---|---|
| `[1/5] copying input` | Prepare the working copy and apply case conversion. |
| `[2/5] preparing deduplication` | Sample input, size structures and select a candidate. |
| `[3/5]` | Run mmap searches, or check/write/dedupe/mark partitions. |
| `[4/5] compacting output` | Keep accepted unique lines in source order. |
| `[5/5] finalizing output` or `writing output` | Finish file handling or restore the result from a temporary working file. |

In a foreground terminal, the active line refreshes in place. On completion it
is retained with `(done: took ...)`, and only its ETA disappears. Durations under
one second show milliseconds. Redirected stderr keeps the fixed history without
ANSI sequences or periodic updates. Very short phases are still recorded;
phases not executed, such as deduplication of an empty input, are omitted.

Percentage is global, not a percentage of the current phase: copy occupies
0–4%, deduplication 4–94%, compaction 94–99% and finalization the last 1%.
Preparation remains at the copy boundary. Progress does not move backward on
fallback or revised task estimates. `ETA: --` means an estimate is unavailable;
unknown-size input may also show an unknown percentage.

The mmap chunk and task counters describe indexed chunks and processing tasks,
not output line counts. Extra bounded passes can revise the task total. An ETA
is an estimate of remaining work, not a deadline.

## Why did the engine change?

`checking disk-partitions` examines a candidate. Exact bitmap, private-memory or
disk requirements may reject it. The reason is displayed, followed by the
confirmed mmap engine and its parameters. This is a planned
[fallback](engine-selection.md#confirmation-and-fallback), not partial deduplication
by one engine followed by a restart of modified data.

With a forced engine, unavailable resources produce an error instead of a
switch. A failed phase is not marked done, and no success report follows it.

## Why are not all threads busy?

`-t` is a maximum. Mmap inserts into one table serially and parallelizes searches
of following chunks. Partitions use independent workers, capped by memory and
the amount of work. A confirmed partition worker count can therefore be lower
than the requested limit. See the [engine descriptions](README.md#understand-the-engines).

## Memory and disk questions

- **RSS exceeds `-m`:** it is a working-memory estimate. Read
  [what the budget covers](resources.md#working-memory-budget).
- **Temporary-file creation fails:** check the displayed directory, permissions
  and free space. Known RAM-backed filesystems are excluded; the current
  directory is only one of the [fallback locations](resources.md#temporary-files).
- **A low memory limit makes processing slower:** it can mean more scans or
  private overflow passes. More threads cannot compensate for every storage cost.
- **A redirected result is slow near the end:** a temporary working file may
  still need copying to the destination. Check the finalization phase and the
  downstream command rather than assuming deduplication is still running.

## Unexpected output

Check [line handling and case conversion](usage.md#line-handling) first. Filtered
lines are different from duplicates. Appending does not compare new input with
existing output. Unique order is stable, while the duplicate file's order is not.

## Collect a useful report

Record the exact command, `./duplicut --version`, OS, input size, memory budget,
thread limit, destination type and phase history. A small input reproducing the
problem is especially useful. Include disk-space or concurrent-load information
when relevant; do not include private wordlist contents in a public report.

For more detail, make a [debug build](development.md#debug-builds):

```sh
make debug level=1
./duplicut wordlist.txt -o clean-wordlist.txt 2> duplicut-debug.log
```

The result and log stay separate. Attach the relevant diagnostics to a
[GitHub issue](https://github.com/nil0x42/duplicut/issues), then use `make` to
return to a release build.
