# Observed performance

[Documentation](README.md) · [Engine selection](engine-selection.md) · [Benchmark procedure](testing.md#benchmarking)

The observations below were recorded on November 4–5, 2026, on a laptop with
16 GB RAM: an Intel Core i5-12450H, 12 logical CPUs, about 15.34 GiB visible to
Linux, and a WD SN740 NVMe SSD using ext4 over LUKS. Power settings were
`powersave` with `balance_performance`.

These results explain the README examples and engine tradeoffs. They are
workload observations, not a prediction for every machine or duplicate ratio.
Builds, options and repetition counts matter; results from different campaigns
should not be treated as one same-binary comparison.

## Real wordlist

The private input contains 2,770,048,320 bytes (2.58 GiB) and 285,193,896 lines.
With `-cp`, the result has 30,618,150 unique lines: 254,356,632 duplicates and
219,114 filtered lines are removed. This is about 10.7% unique, not an all-unique
throughput test. The wordlist itself is not distributed.

Measurements of the working-memory implementation at `de5ba3c` use default
threads and redirect stdout to a private file on RAM-backed `/tmp`:

| Memory option | Median elapsed | Peak RSS, median |
|---|---:|---:|
| Default | 35.726 s | 2,771.84 MiB |
| `-m 100M` | 36.637 s | 47.55 MiB |
| `-m 300M` | 37.340 s | 101.01 MiB |

Each row has three repetitions in rotating order. Output is 310,830,864 bytes,
with SHA-256 `beea1f7cba60864dc7f90492a269cc6bb123dbb10533ecb7d5c0e0cb8a2db64b`.
Limited budgets use a disk working file before restoring the result to stdout.
The final RAM-backed result still occupies about 296 MiB outside this simple
process RSS figure.

[Raw observations](../tests/benchmarks/memory/bigwl.jsonl) also contain earlier
single-run controls. Those controls are not three-run medians. Polled output
sizes may miss the final copy and must not be read as bounds below the final
result size.

## Beyond physical RAM

A generated 20 GiB input with 128-byte words, LF separators and 50% unique
contents completes in **140.093 s** using `-m 2G` and four workers pinned to
physical performance cores. It is about 1.3 times this laptop's physical RAM
and ten times the requested memory budget, not ten times physical RAM.

This is one run of the retained working-memory implementation, not a repeated
median. The approximately 10 GiB output matches the earlier reference hash:
`e8bd5036418d80492046912173d9182fd12ef071466d76a1b42bd674f9c3d8f1`.

| Resource | Observation |
|---|---:|
| Peak process RSS | 674.35 MiB |
| Peak allocated partition file | 21.40 GiB |
| Peak working/output file | 20.00 GiB |
| Output synchronization after timing | 0.0186 s |

The source, working copy and partitions need about 61.4 GiB of disk space at
peak. The earlier master reference took 588.08 s on this generated
input; it is historical, not a fresh forced-mmap measurement of the current
binary. Later selector and UI changes retain this workload's engine and
execution dimensions; the figure is not presented as a rerun of those changes.

[Retained disk observation](../tests/benchmarks/memory/disk.json) ·
[Earlier disk comparison](../tests/benchmarks/partitions/disk.json).

## Engine tradeoffs

Partitions tend to benefit many-pass jobs, particularly when input cannot stay
cached. Mmap avoids partition writes and can be efficient with few chunks,
cached long lines, or many duplicates eliminated early. The
[selection page](engine-selection.md) owns the rules; these measurements show
why the choice is a tradeoff.

The selector comparison below uses a generated 1 GiB input, 128 bytes per word,
`-cp -m 128M`, matching compiler flags, shuffled order, a warm-up and three measured
repetitions. Both the previous and revised selectors use the same underlying
engine algorithms and dimensions for these cases.

| Workers | Unique | Previous auto: mmap | Revised auto: partitions |
|---:|---:|---:|---:|
| 1 | 10% | 5.126 s | 5.630 s |
| 1 | 50% | 17.367 s | 6.531 s |
| 1 | 90% | 27.089 s | 7.083 s |
| 4 | 10% | 2.371 s | 5.075 s |
| 4 | 50% | 7.829 s | 5.976 s |
| 4 | 90% | 10.896 s | 6.181 s |

The low-uniqueness losses are real; they are not hidden in an average percentage.
A larger bitmap also creates counterexamples: at 128 MiB of eight-byte words,
90% unique and `-m 4M`, forced partitions take about 28 s. Mmap takes about 38 s
with one worker but 12 s with four. The conservative volume guard retains mmap
in both cases: useful protection for four workers, a missed opportunity for one.

The exploratory forced-engine matrix has one run per mode in each of 39
combinations. It identifies large errors, not statistically established winners
for close results. No macOS or rotating-disk performance measurement is claimed.
Later UI timings taken alongside other Duplicut jobs are not used as overhead
evidence.

## Memory observations

Mapped scan windows reduce residency, but `-m` is an estimate. A constrained
128 MiB short-word test with `-m 4M` still reaches about 21.5 MiB RSS on the mmap
fallback path, while anonymous allocations stay near 2.3 MiB. The real-wordlist
and larger-than-RAM rows above show different residency behavior.

Removing the large-bitmap guard unconditionally slowed that same constrained
four-worker case from about 12.7 to 27.9 s. Feasible allocations alone do not
establish that partitioning will be faster.

## Data and provenance

Raw records remain under `tests/benchmarks`. JSONL holds one observation per line;
`repeat: -1` denotes a warm-up. Parameters, commands and hashes are in the rows;
environment files add compiler flags, CPU affinity, build IDs and binary hashes.

| Dataset | Scope and interpretation |
|---|---|
| [Partitions](../tests/benchmarks/partitions/) · [environment](../tests/benchmarks/partitions/environment.json) | Initial comparison with `a4a5d80`; typically one warm-up and seven measured trials, plus single disk trials and thread-scaling controls. Earlier activation rules are no longer current. |
| [Saturation](../tests/benchmarks/saturation/) · [environment](../tests/benchmarks/saturation/environment.json) | `ab958d2` versus the bounded-insertion change `991b765`; three measured trials, then seven pinned trials for the variable 2 GiB long-line control. |
| [Selection](../tests/benchmarks/selection/) · [environment](../tests/benchmarks/selection/environment.json) | `63cfc02` versus selection finalized in `6107d5f`; three trials after warm-up. Timed long-line inputs had 128 chunks; the measured 64-chunk gate and final 128-chunk gate chose the same engine there. |
| [Integration](../tests/benchmarks/integration/) · [environment](../tests/benchmarks/integration/environment.json) | Sampled sizing, nearest-count controls and heterogeneous inputs around `db1765c`; `ceiling-count` includes a rejected prototype. Disk data precedes the later mapped-memory changes. |
| [Working memory](../tests/benchmarks/memory/) | `de5ba3c` observations used above. `bitmap-controls` includes unconditional admission; `bounded` checks retained admission. Real-wordlist controls are single runs; retained rows have three repeats. |
| [Engines](../tests/benchmarks/engines/) · [environment](../tests/benchmarks/engines/environment.json) | Frozen optimized DEBUG=1 builds around `7183428` and `2a7af6b`. `forced` is exploratory; confirmation files record the repeated selector comparison. |

Standard generated workloads use cyclic hexadecimal identifiers, padded with
`x` and followed by LF. Line count is the byte target divided by word length
plus one, rounded down; the chosen unique fraction sets the cycle length.
Four-byte integration and four/five-byte saturation inputs instead use base-94
identifiers from printable ASCII. The mixed integration case replaces its first
16,380 bytes with repeated `short` lines, leaving total size unchanged.

Single-worker rows in the selector table come from the 16-batch confirmation
build; their dispatch, dimensions and processing are identical under the retained
eight-batch rule. Four-worker rows use the final confirmation. The initial
4 GiB selection trials and mixed-prefix integration controls have substantial
wall-time variability; their raw medians do not establish small repeatable gains
or regressions on their own.

The recorded `unique` external-tool attempts did not finish within their limits
and do not provide comparable timings. No `rling` measurement is recorded.
Procedures for new comparisons are in [Testing](testing.md#benchmarking).
