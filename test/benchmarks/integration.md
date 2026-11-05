# Configuration and engine integration

Later changes add [mapped-memory budgeting](memory.md) and a
[central engine selector with manual forcing](engines.md). This page describes
the preceding integration and its measurements.

This follow-up integrates configuration, sampled sizing, engine selection,
execution and diagnostics. Earlier partition, selection and saturation results
remain in their respective reports; their binaries predate this change.

## Configuration

`config()` takes one platform memory snapshot, chooses threads, reads one bounded
line sample, sizes the global table and chunks, then prepares an initial engine
plan. The partition module checks exact runtime requirements before committing
to partition I/O. No generic engine interface or additional public option is
needed. The names used in configuration, progress and diagnostics are
`mmap-chunks` and `disk-partitions`.

Let `A` be system available memory and `E = min(A, -m)`. The shared deduplication
allocation budget is `B = 0.5 * E`, independent of the eventual global table
allocation. A file-bound table must not inadvertently shrink the private-table
engine's budget. The table-plus-current-chunk target is `1.5 * B`.
As before, the nearest-count rounding balances chunks and may exceed this
target: the nominal sum stays within `1.75 * B` (plus byte rounding), and
actual boundaries extend to the next newline.
After balancing, a final bound keeps the nominal sum within that historical
maximum even when sampled sizing leaves most of the table budget unused.
These are allocation and working-set estimates, not a cap on RSS, mapped pages,
thread stacks or the kernel page cache.

At most 64 KiB are sampled: a small file is read once; otherwise four 16 KiB
windows span the input. Partial boundary lines are excluded. Existing parsing
and filtering rules determine accepted occurrences. Two densities have different
purposes:

- The minimum **raw bytes per accepted occurrence** among usable windows sizes
  the global table, targeting 50 percent occupancy. This guards against denser
  regions seen in any sample, without assuming a duplicate percentage.
- Total sampled raw bytes divided by total accepted occurrences sizes chunks.
  This avoids letting one short-line region shrink every chunk of an otherwise
  long-line file. The 70-percent actual insertion limit handles estimates that
  miss denser unsampled regions.

Raw bytes include filtered input: the estimate describes how much of the mapped
file corresponds to an accepted occurrence. Average accepted payload length is
separate and remains the selector's content-storage estimate. If no sample has
an accepted line, the existing eight-byte fallback sizes the mmap path;
automatic partition selection declines without a usable sample.

File-based table capacities round upwards to a prime; memory-bound capacities
round downwards. Rounding a sufficient file-bound table down by a few slots
would otherwise turn a whole-file chunk into two chunks. The existing minimum
capacity remains subject to the allocation cap.

The [selection rules](selection.md) retain 16 estimated chunks, raised to 128
for cached long-line inputs. Cache headroom uses `A` minus the actual global
table allocation; allocation sizing uses `E`. Partition geometry uses `B`,
its own content storage and scratch requirements. Runtime checks subtract the
exact metadata and bitmap before confirming workers. There is no fixed file
size, RAM size or partition-count cutoff.

## Execution and diagnostics

Partition execution has four focused phase helpers: exact counting, buffered
scatter, independent deduplication, and source-order marking. The inner
partition loop retains local full contents, exact comparisons, atomic bitmap
updates and bounded overflow passes. Phase extraction adds no per-record
function calls to that loop.

Temporary files still use `mkstemp` and immediate `unlink`, preferring the
resolved input directory, then output directory, then working directory.
Known RAM filesystems are rejected. Space is checked on the opened volume;
subsequent write errors remain fatal. All fallback decisions precede partition
writes and source marking.

The nominal triangular mmap task count is initialized only when that engine
actually runs, including after a declined partition plan. Partition progress
uses its own phases and exact byte totals. The transition to final cleanup
works before its first byte update and reports completion without division by
zero. Extra saturation tasks retain their existing accounting.

DEBUG=1 shows samples, budgets, candidate, rejection reasons, confirmed engine,
allocations, phases and mmap saturation. DEBUG=2 adds task and partition-pass
summaries; DEBUG=3 includes private storage occupancy. Debug timestamps use
per-call local storage and source basenames work with absolute build paths.
Release builds compile these diagnostics out.

## Validation

All eight test scripts pass, including the existing 1080 differential cases.
New configuration checks cover density estimates, budget and prime bounds,
whole-file chunks, filtered and absent samples, and a deliberately misleading
sample requiring several saturation passes. Forced partition and Linux fault
builds check initial versus confirmed plans, all phases, disk preflight
fallback and exactly one available-memory query. A deterministic clock fixture
checks phase progress, the cleanup transition and fallback task initialization.

GCC release and Clang DEBUG=3 builds pass `-Wall -Wextra -Werror`.
ASan/UBSan checks sampled sizing, repeated saturation and partition correctness;
leak detection is disabled because of sandbox restrictions. The Clang serial
build exercises partition and saturation tests. No macOS runner was available;
Linux memory injection checks selection at other memory sizes without claiming
physical performance measurements on those machines.

## Measurements

The reference for this integration is `d513efb`, already containing partitioning
and the 70-percent saturation limit. Both release binaries use GCC 16.2.1,
`-std=gnu99 -O2 -DNDEBUG -mtune=native -ffast-math`, identical source ordering
and pthreads. Hardware is unchanged from the [initial campaign](partitions.md).
Trials use `-t 4` and affinity to physical performance cores 0, 2, 4 and 6.
Generation, output synchronization and SHA-256 checking are outside process
timing. Variants alternate order; warm-ups are excluded from medians.

Inputs use cyclic identifiers and LF. The line count is
`floor(source_bytes / (payload_length + 1))`; unique counts are the requested
percentage rounded down. Four-byte identifiers use base 94, ASCII 33 through
126. Longer words use hexadecimal identifiers of width `min(length, 16)`,
padded with `x`. The mixed case overwrites the first 16380 bytes with
`short\n` repeated 2730 times, without changing the file size.

An early minimum-density-for-both prototype slowed the 2 GiB heterogeneous
case from 5.567 to 6.754 seconds, **21.3 percent**. Only the first 16380 bytes
contained short lines; the rest contained 128-byte words. The retained
minimum-table / mean-chunk policy addresses that demonstrated failure.
[Prototype records](integration/chunk-models.jsonl) retain the rejected result.

A stricter ceiling-count variant was also rejected. With 1 GiB of eight-byte
words, 90 percent uniques and `-m 3584M`, it forced two chunks instead of one:
10.068 versus 8.857 seconds, **13.7 percent slower**. Peak RSS was essentially
identical at 2818 MiB. Retaining the historical nearest-count rounding avoids
that introduced pass; it does not enlarge the table or borrow uncapped system
memory. A small deterministic configuration regression now checks this boundary.
With nearest-count balancing restored, the three-trial medians are 8.618 and
8.538 seconds: effectively neutral. The final envelope guard does not change
any timed case's table, chunk, partition or worker dimensions.

| Source | Payload bytes | Unique | `-m` | Trials | Before (s) | Integrated (s) | Time saved |
|---|---:|---:|---|---:|---:|---:|---:|
| 128 MiB | 4 | 90% | 64M | 7 | 7.947 | 4.945 | **37.8%** |
| 2 GiB | 128 | 90% | 8G | 7 | 4.116 | 2.941 | **28.6%** |
| 512 MiB | 8 | 90% | 256M | 3 | 8.809 | 8.727 | 0.9% |
| 1 GiB | 8 | 90% | 3584M | 3 | 8.618 | 8.538 | 0.9% |
| 2 GiB | 128 | 90% | 200M | 3 | 7.684 | 7.534 | 2.0% |
| 1 GiB | 8 | 50% | 100M | 3 | 8.920 | 8.506 | 4.6% |
| 2 GiB, mixed prefix | 128 | 50% | 256M | 7 | 4.731 | 4.869 | -2.9% |

The 200M and 100M rows come from the ceiling-count control series: their selected
engine and all allocation/chunk dimensions also match the final policy.
The small differences on ordinary controls are treated as neutral, not as
established gains. The 1 GiB / 100M case uses disk partitions; its private
algorithm and allocation geometry are unchanged by phase extraction.

Four-byte words benefit from sizing chunks for their observed density rather
than the fixed eight-byte estimate. Median CPU time drops from 18.30 to 12.31 seconds
without a material RSS change. For the file-bound 128-byte case, the table
shrinks from approximately 4 GiB to 254 MiB, **93.8 percent**; process peak RSS
drops from 6146 to 2304 MiB, **62.5 percent**. That case is near three seconds:
the allocation reduction matters more than a small fast-job timing difference.

The heterogeneous input has pronounced wall-time variability. The first
seven-trial minimum-table / mean-chunk series gives 5.043 versus 4.920 seconds,
with median CPU time 11.83 versus 11.82 seconds. A subsequent three-trial
nearest-count check gives 5.109 versus 6.517 seconds, despite CPU times of
11.77 versus 11.72 seconds and no physical reads or major faults. This apparent
27.6-percent slowdown is retained in the raw data, rather than discarded.
The final seven-trial check on `db1765c` gives 4.731 versus 4.869 seconds
(2.9 percent slower), with median CPU times of 11.96 and 12.04 seconds.
Elapsed ranges overlap: 4.59–5.10 and 4.60–5.24 seconds. These series do not
support a repeatable speedup or a substantial implementation regression on
this control. Its table and chunk dimensions match the reference. There is
no claim that every individual run is faster.

Raw records: [initial sizing](integration/initial-sizing.jsonl),
[ceiling-count controls and rejected boundary](integration/ceiling-count.jsonl),
[nearest-count trials](integration/nearest-count.jsonl),
[final heterogeneous check](integration/final-mixed.jsonl),
[environment and binary hashes](integration/environment.json).

### Beyond physical RAM

The final 20 GiB trial exceeds this host's 15.34 GiB physical memory, with
128-byte words, 50 percent uniques, `-m 2G` and four workers. Both binaries
choose 256 disk partitions. Filesystem synchronization occurs before each
process and after its exit, outside timing. Descriptor polling every 200 ms
records temporary logical and allocated sizes.

| Resource | Before integration | Integrated |
|---|---:|---:|
| Process time (s) | 157.556 | 144.515 |
| CPU user + system (s) | 93.38 | 89.45 |
| Peak RSS (GiB) | 12.06 | 11.96 |
| Input block I/O (GiB) | 105.91 | 117.40 |
| Output block I/O (GiB) | 61.46 | 61.43 |
| Major faults | 12501 | 7555 |
| Peak allocated partition space (GiB) | 21.40 | 21.40 |

This is one measured pair: the observed **8.3 percent** reduction is not a
claim of a repeatable small optimization. Source and unique output are
20 and 10 GiB; both output SHA-256 hashes match. Total source, work file and
partition allocation peaks near 61.4 GiB. Free space returned to 197 GiB after
cleanup. Final synchronization took 0.030 and 0.035 seconds respectively.
[Raw disk records](integration/disk.json).

The original master trial took 588.08 seconds on this same generated input;
that historical reference is not rerun here. The large partitioning benefit
remains distinct from this integration's single-pair variation.
