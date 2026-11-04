# Partition benchmarks

Measurements on 2026-11-04, comparing `a4a5d80` with `2d3f09e`.
Both use GCC 16.2.1, `-O2 -DNDEBUG -mtune=native -ffast-math`, pthreads
and the same options. The machine has an i5-12450H (12 logical CPUs),
15.34 GiB RAM and a WD SN740 NVMe SSD, with ext4 over LUKS. The CPU governor
is `powersave`, with `balance_performance`; trials have no explicit CPU affinity.

The generated inputs contain hexadecimal identifiers padded to the requested
length, followed by LF. Identifiers repeat cyclically, with 10, 50 or 90 percent
unique lines. These are reproducible workloads, not a sample of natural leaked
passwords. The large disk trial uses the same pattern with 50 percent uniques.

`benchmark-partitions.py` runs one warm-up, then seven measured repetitions,
mixing the order of the two binaries. Times include the complete process:
copying, deduplication and final compaction. Output `fsync` and SHA-256 validation
run outside timing, before the next trial. CPU time, RSS, block I/O, page faults
and synchronization time are in the accompanying raw results. The deleted
partition file is closed during the process; no descriptor is retained solely
to force its contents to stable storage.
No global cache eviction is used.

## Selection used for these measurements

The [selection audit](selection.md) describes the revised rules, including the
removal of the absolute input-size gate and fixed partition-count ceiling.
The rules below describe the version measured in this initial campaign.

Selection uses byte counts and the existing platform memory estimate, without
CPU-specific timing calibration. The same logic is built for Linux and macOS;
the existing macOS free-memory estimate is conservative compared with Linux
`MemAvailable`. All-line counting occurs only after the preliminary choice, to
size the on-disk ranges and bitmap exactly.

- At least 256 MiB of input and 16 estimated source chunks, with at least 1 MiB
  available for the partition engine. Smaller jobs keep the original engine.
- If the input fits in the current available system memory, a sample of up to
  64 KiB, spread over four windows, must have an average accepted line length
  at most 16 bytes. Partial boundary lines are excluded. Longer
  cached inputs keep the original engine. Available memory is queried without
  applying `-m`; a small allocation budget does not imply physical disk reads.
- Partition count is a power of two, targeting chunks times workers, capped at
  4096 and by the allocation budget. Scatter buffers and metadata use at most
  half that budget. They are freed before allocating the duplicate bitmap.
- The bitmap must fit in half the budget. Workers share the remaining memory,
  with private hash tables, content storage and 16 KiB input/output buffers.
  `-t` remains a maximum, also limited by the memory budget.
- If the bitmap cannot fit, or a suitable temporary volume cannot be opened,
  selection falls back before changing the working contents. An I/O failure
  after partition writing begins is an error, not a restart on another engine.

The engine budget is the former global hash table allocation, approximately
half of `-m`. Neither the old nor the new `-m` is a cap on RSS: file mappings
and the kernel page cache also consume physical memory.

The cache gate is deliberately conservative. The earlier unconditional
partition path took 1.988 s versus 1.544 s on a cached 1 GiB input with 128-byte
lines and 10 percent uniques: a 28.7 percent regression. Those measurements
are retained as rejected design data. Sampled length and available memory are
heuristics, not a guarantee for every distribution or storage device.

## Results

Tables below use median elapsed time and `100 * (1 - candidate / baseline)`.
The raw records include the warm-up as repetition `-1`, which is excluded.

| Source | Bytes/line | Unique | `-m` | Baseline (s) | Candidate (s) | Time saved |
|---|---:|---:|---|---:|---:|---:|
| 257 MiB | 8 | 10% | 25M | 2.331 | 1.599 | 31.4% |
| 257 MiB | 8 | 50% | 25M | 6.944 | 1.828 | 73.7% |
| 257 MiB | 8 | 90% | 25M | 10.212 | 2.358 | 76.9% |
| 257 MiB | 128 | 10% | 25M | 0.389 | 0.253 | 35.0% |
| 257 MiB | 128 | 50% | 25M | 0.665 | 0.621 | 6.6% |
| 257 MiB | 128 | 90% | 25M | 0.888 | 0.863 | 2.9% |
| 257 MiB | 16 | 50% | 25M | 2.896 | 1.159 | 60.0% |
| 1 GiB | 128 | 10% | 100M | 1.522 | 1.048 | 31.1% |
| 1 GiB | 128 | 50% | 100M | 2.819 | 2.693 | 4.5% |
| 1 GiB | 128 | 90% | 100M | 3.894 | 3.882 | 0.3% |
| 20 GiB | 128 | 50% | 2G | 588.08 | 160.58 | **72.7%** |

All rows use `-t 4`. The 20 GiB row is one reference trial and one final
candidate trial, not a seven-run median. The reference is reused from the earlier
disk campaign; the final candidate reran the identical generated input.
Its SHA-256 is identical, with 10 GiB output and a **3.66 times speedup**.
The source is about 1.3 times physical RAM and 10 times `-m`;
this does not claim a trial at 10 times physical RAM.

The final sampler refinement in `adee397` changes the prefix sample to four
distributed windows and estimates chunks arithmetically. These uniform timing
inputs select the same engines and partition counts. The 20 GiB path exceeds
available memory and does not invoke the length sampler.

| 20 GiB resource | Baseline | Candidate |
|---|---:|---:|
| CPU user + system (s) | 474.40 | 98.11 |
| Peak RSS (GiB) | 12.75 | 11.66 |
| Input block I/O (GiB) | 270.44 | 118.56 |
| Output block I/O (GiB) | 40.00 | 61.43 |
| Major faults | 11543112 | 13890 |

The final trial sampled open descriptors every 200 ms.
Peak allocated partition space was 21.40 GiB; the working/output file
peaked at 20.00 GiB. Including the source, these files need about
61.40 GiB of disk space.
There were 197 GiB free before and after the campaign. The final disk trial
also flushed the filesystem before timing and after exit; final synchronization
took 0.044 s, outside the process time.

| Threads (maximum) | Candidate median (s) |
|---:|---:|
| 1 | 3.629 |
| 2 | 2.882 |
| 4 | 2.117 |
| 12 | 2.016 |

Scaling uses 257 MiB, 8-byte words, 50 percent uniques and `-m 25M`, with
one warm-up and three trials per thread limit. This demonstrates private-table
parallelism; it is not a thread-count rule for every CPU.

The 8 MiB control had overlapping, noisy distributions around 50 ms. One
15-trial series favored the reference by 2.8 percent; another 30-trial mixed
series favored the candidate by 1.6 percent. No small performance claim is
based on these controls. The 1 GiB long-line 90-percent case is also neutral.

Raw results: [257 MiB](partitions/257mib.jsonl),
[16-byte boundary](partitions/257mib-16.jsonl),
[1 GiB long lines](partitions/1024mib-long.jsonl),
[disk trial](partitions/disk.json), [scaling](partitions/scaling.json),
[small control](partitions/small-control.json),
[extra control](partitions/small-control-extra.jsonl),
[rejected cache policy](partitions/before-cache-gate.jsonl),
[environment](partitions/environment.json).

## Disk space and correctness

The partition file stores a 10-byte header and the complete contents of every
accepted line. Its size relative to an LF input is `(length + 10) / (length + 1)`:
2 times for 8-byte words, about 1.07 times for 128-byte words. This is additional
to the input and existing working/output file; overflow passes compact within
the same partition ranges and do not allocate another partition file.

Temporary files use `mkstemp` followed immediately by `unlink`, preferably
beside the resolved input, then the output, then in the current directory.
Known RAM filesystems are rejected. Available space is checked on the opened
descriptor; concurrent space consumption can still make later writes fail.

The functional suite passes all six scripts, including 1080 differential
cases. Partition tests cover 10/50/90 percent uniques, exact first-occurrence
order, duplicate occurrence counts, filters, case conversion, binary contents,
CRLF, final lines, pipes, append mode, forced hash collisions, arena and table
saturation, and single-threaded versus concurrent results. Linux fault tests
exercise partial transfers, EINTR, EOF, ENOSPC, RAM volumes, source/output
fallbacks, symbolic links, space preflight and immediate unlink. Production
selection is tested at 128/256 MiB and with cached/uncached long inputs.

ASan/UBSan and a Clang `-Wall -Wextra -Werror` build are checked separately.
The portable serial build is exercised with `MULTITHREADING=0`. No macOS or
rotating-disk execution was available on this machine.

## Issue #22

The [buffering proposal](https://github.com/nil0x42/duplicut/issues/22#issuecomment-695776538)
is applied through sequential source scans, buffered partition I/O and private
tables processed by the existing pthread pool. This parallelizes insertion
without a shared concurrent table or a first-occurrence race. Full contents
remain local to each partition, avoiding scattered source-page comparisons.

A second engine that searches buffers against a frozen global table would
still need ordered insertion and local duplicate handling. It is not added:
the retained partition engine addresses the expensive disk case, while the
existing engine already searches subsequent chunks in parallel and remains
preferable for the measured cached long-line case. No OpenMP, probabilistic
equality, recursive partition tree, hugepage policy or new command-line option
is introduced.

`unique` was tried on the same ASCII pattern. The installed John multicall
binary did not finish a 1 GiB input within 600 seconds with a forced hash size,
nor a bounded 64 MiB retry within 30 seconds with automatic hash sizing and
`-buf=1`. No speed ratio or matching-output claim is made for these unfinished
runs. The [retry record](partitions/unique.json) includes the exact command.
Its memory options do not have the same accounting as duplicut's. `rling` was
not installed, so no timing is reported for it.
