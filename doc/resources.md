# Memory and disk

[Documentation](README.md) · [Engine selection](engine-selection.md) · [Troubleshooting](troubleshooting.md)

Duplicut budgets its allocations and mapped working set. Memory and disk costs
have different owners; a small allocation limit does not make the operating
system's file cache disappear.

## Working-memory budget

Configuration takes one memory snapshot. Linux uses `MemAvailable`; macOS uses
a free-page estimate, which is more conservative. The effective budget is the
smaller of this estimate and `-m`, when provided.

Half of the effective budget is reserved as the **engine allocation budget**.
It caps the global mmap hashmap or the partition structures. The other space
allows for resident file data and processing overhead. The allocation budget
is independent of a small file's actual hashmap size.

`-m` is not a hard RSS limit. Runtime state, thread stacks, page granularity and
mapped-page behavior add overhead. The kernel file cache and a RAM-backed final
output also consume system memory outside a simple process-allocation total.
The [recorded measurements](performance.md#memory-observations) illustrate both
close budgets and a small-budget residency overshoot.

## Table and chunk sizing

At most 64 KiB are sampled: the complete file when small enough, otherwise four
windows of up to 16 KiB distributed across it. Partial boundary lines are
excluded. Counts use the same parsing and filtering rules as processing.

The estimates serve different purposes:

| Estimate | Use |
|---|---|
| Smallest raw bytes per accepted occurrence among usable windows | Size the global table for the densest observed region. |
| Aggregate raw bytes per accepted occurrence | Size nominal mmap chunks. |
| Average accepted content length | Estimate the cost of retaining contents for partitions. |

Sampling measures occurrences, not global uniqueness. An unusable sample uses
an eight-byte density fallback for sizing; automatic partition selection still
requires a usable accepted-line sample.

The global table is file-bound when possible, subject to its allocation cap,
and has a prime number of slots. Chunks are balanced by rounding the estimated
count to the nearest integer, rather than always adding another chunk.

The nominal table-plus-chunk target is 1.5 times the engine allocation budget.
The rounding margin is bounded at 1.75 times that budget, before newline-boundary
extensions. The [mmap insertion limit](engines/mmap-chunks.md#bounded-insertion)
handles denser regions that the sample missed.

## Resident mapped pages

When the working file does not fit beside the planned allocations, readers use
bounded, newline-aligned scan windows. Consumed pages are released with memory
advice while their shared file contents remain intact. The indexed chunk is
explicitly released after its lookup workers finish; its mapped addresses remain
valid throughout processing.

Released pages may be faulted in again. Memory mapping is therefore not the
same as keeping all input resident, nor does releasing residency erase file data.

## Partition storage

The partition count grows with input size and usable workers, subject to memory
for metadata and scatter buffers. Worker count is capped again after exact
bitmap and metadata reservations. Below the normal per-worker granule, a feasible
plan can still use one worker. Allocations are also capped by the largest range
so excess available RAM does not create needlessly large private tables.

For `N` accepted occurrences with `L` content bytes in total:

```text
partition file bytes = 10 × N + L
bitmap bytes         = ceil(N / 8)
```

These are occurrence counts, including repeats. The [record layout](engines/disk-partitions.md#write-records)
explains the overhead. During scattering, buffers and metadata share a bounded
allowance; the buffers are freed before bitmap allocation. Later, workers share
the memory remaining after the bitmap and metadata, using private tables,
content storage and scratch buffers.

The input, working file and partition file can coexist. A temporary working file
may be needed in addition to the partitions, and copying the final result to a
different destination can require space there too. A low memory budget does
not imply low temporary disk use.

## Temporary files

Temporary-file creation tries these locations in order:

1. The resolved directory of a regular input file.
2. The directory of a suitable regular output file.
3. The current directory.

Known RAM-backed filesystems are rejected, and available space is checked against
the size known at creation. Unknown input length or later changes in free space
can still lead to an I/O error. This avoids turning large disk partitions into
RAM consumption when the current directory or `/tmp` is backed by memory.

Files are created with `mkstemp` and immediately unlinked. Their open descriptors
keep them usable until close; process termination releases them without leaving
named temporary files. The displayed directory is the location actually chosen.

For large or unknown-size input, a RAM-backed output can receive only the final
result while disk holds the working copy. The final RAM-backed result still
occupies memory: its size cannot be eliminated by `-m`.
