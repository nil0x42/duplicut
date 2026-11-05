# Engine selection audit

The engine preference rules live in `src/engine.c`. Configuration owns the
shared memory snapshot, sampling and mmap dimensions. The partition module owns
its buffer layout and exact resource checks. Selection consumes those usable
capacities instead of duplicating their arithmetic. Execution, cleanup, option
semantics and stable output order remain shared.

`-e auto` is the default. `-e mmap-chunks` skips partition selection.
`-e disk-partitions` bypasses performance preferences but retains memory,
filesystem and space checks. Unavailable forced partitioning fails explicitly;
automatic selection can fall back before partition writes start. I/O failures
after that point remain errors, rather than silent retries with another engine.

## Inputs to the decision

The memory snapshot provides available system memory `A`. The working budget
is `E = min(A, -m)` and the deduplication allocation budget is `B = E / 2`.
At most 64 KiB are sampled in four distributed windows. Parsing, case conversion
and filters use the existing line reader.

The smallest sampled raw bytes per accepted line sizes the global table at a
50% target load. The aggregate raw bytes per accepted line sizes chunks, with
the historical nearest-count rounding. The table cannot exceed `B`; the nominal
table plus chunk cannot exceed `1.75 * B`. A 70% insertion limit handles denser
unsampled regions by continuing in another pass. The input size divided by the
nominal chunk size estimates the number of chunks; it is not an exact count of
the passes that saturation or duplicate elimination will actually require.

Average accepted content length serves a different purpose: private tables
also retain full line contents. Available system memory is kept separate from
`-m` when estimating whether the working file can remain cached beside the
global table. A smaller allocation limit does not itself make file pages cold.

## Initial preference

The normal threshold is 16 estimated chunks. This is a ratio to the configured
capacity, not an absolute file-size or RAM threshold.

An exception allows parallel insertion for short lines when all these
conditions hold:

- Multithreading is compiled in and at least two partition workers are usable.
- The estimated chunk count is no greater than that usable worker count.
- The input contains at least one sample's worth of bytes per usable worker.
- Mean accepted contents are no longer than twice `sizeof(t_line)`, currently
  16 bytes.

The exception uses workers supported by the memory budget and partition count,
not simply `-t`. A requested 64 workers with memory for one must not qualify.
It targets the serial insertion and idle lookup workers of whole-file or
few-chunk mmap processing.

Cached long lines require at least eight lookup batches. A batch is the number
of estimated chunks divided by available mmap lookup workers, rounded up; a
serial build uses one worker. This applies when mean contents exceed twice
`sizeof(t_line)` and the input plus global table fit in `A`.
Repeated cached traversal is cheap relative to writing and copying full
contents, and lookup chunks can run concurrently. The fixed partition scans
and writes do not gain the same parallelism.

This replaces the former fixed 128-chunk rule. For example, 32 estimated chunks
make 32 batches with one worker, eight with four workers and three with twelve.
The first two qualify; twelve workers retain mmap processing. More cores therefore
do not automatically mean partitions: the existing engine also searches
subsequent chunks in parallel. This counts parallel work opportunities; it does
not predict a linear timing speedup or measure disk latency. Eight batches leave
a margin over fixed source scans, temporary writes and temporary reads. Both
thresholds are work ratios, independent of absolute input size, available RAM,
clock speed or an OS name.

An unusable accepted-line sample retains mmap chunks. A forced partition engine
does not need a useful sample to honor the request, but still needs a feasible
layout.

## Feasibility and exact admission

The partition module prepares a layout before the initial preference is tested.
This performs arithmetic only, without reading the complete input or creating
a temporary file.

- Minimum working space is four times worker scratch overhead: two 16 KiB I/O
  buffers plus the maximum line length, currently 4095 bytes.
- Maximum workers are bounded by requested threads, the compiled threading
  mode, partition count and usable memory. A worker granule is 64 I/O buffers
  (1 MiB). Smaller feasible budgets use one worker; this is not a 1 MiB
  admission floor.
- The power-of-two partition count targets approximately `4 * workers²` ranges
  and scales further with input size versus budget. Scatter buffers
  and metadata remain within their shared memory allowance. Input volume also
  limits the count so tiny ranges do not multiply buffers unnecessarily.

The counting phase then obtains exact accepted occurrence count `N`, contents
size `L` and temporary size `D = 10*N + L`. It reserves a bitmap of
`ceil(N/8)` bytes and partition metadata. Remaining memory must still support
minimum worker storage. Worker count is capped again after these reservations;
worker allocation is also bounded by the largest serialized range, so abundant
RAM does not produce needlessly huge tables.

Automatic admission retains the existing fast path when the bitmap occupies at
most half of `B`. Above that fraction, it estimates private passes from the
actual usable entry count and content capacity:

```
private_passes = ceil(max(N / (partitions * entries_per_worker),
                          L / (partitions * content_bytes_per_worker)))
partition_work = 3 * input_bytes + (private_passes + 1) * D
mmap_work      = input_bytes * (estimated_chunks + 1) / 2
```

Partitioning is rejected when `partition_work >= mmap_work`. Every occurrence
is treated as distinct for capacity estimation; no global uniqueness percentage
is guessed from the sample. Hash imbalance and saturation can still add work.
These quantities compare logical traversal volumes, not seconds. Worker count
affects partition count and per-worker capacities; no fictitious linear CPU
speedup is applied to serial scans or disk traffic.

Before writing, the temporary directory is selected from the input's resolved
directory, the output directory, then the current directory. Known RAM-backed
filesystems are rejected. Temporary files use `mkstemp` and immediate unlink;
available space is checked before partitioning. These are resource constraints,
not guesses about whether the physical medium is HDD or SSD.

Mapped scan windows and disk working files for large RAM-backed output are
described in the [working memory report](memory.md). `-m` remains an estimate
rather than a hard RSS cap.

## Validation and interpretation

Functional tests compare automatic and forced engines against the same Python
reference, including filters, case conversion, duplicate output, stable order,
final lines and report counters. Resource failures remain explicit when an
engine is forced. Linux memory injection covers 4 MiB through 128 GiB with one
or several threads; it checks configuration and correctness, not hypothetical
machine performance.

Performance checks compare both forced engines and automatic selection on the
same executable, with 1, 4 and 12 requested workers, 10/50/90% unique lines,
short and long lines, whole-file and constrained memory layouts. Outputs are
hashed and synchronization takes place outside timing. Exploratory single runs
identify large errors; close results and very short jobs are not optimization
claims. The real input larger than physical RAM is covered separately in the
working memory report.

The model intentionally does not infer storage latency, CPU frequency, memory
bandwidth, or global uniqueness from core count and RAM. Those inputs cannot
guarantee the fastest engine on every machine. The policy uses portable resource
ratios and conservative margins; forced engines make exceptional workloads
testable without a private build.

## Measurements

The exploratory matrix has 39 combinations, with three runs per combination:
forced mmap, forced partitions and automatic selection. The retained rules point
to the faster measured forced engine in 32 combinations, including 31 of 35
where even the faster engine takes at least three seconds. These are descriptive
single-run counts, not statistically established winners for close results.

Whole-file short inputs favor mmap insertion with one worker and partitions
with several workers. Whole-file long inputs retain mmap processing throughout
the 10/50/90% range. Constrained short inputs generally favor partitions: at
257 MiB, 90% unique and `-m 25M`, the exploratory single-worker comparison is
55.56 seconds for mmap versus 6.83 seconds for partitions. Automatic selection
already chooses partitions there.

The revised cached rule fixes substantial single-worker errors in the previous
selector. Confirmation uses identical compiler flags, mixed order, one warm-up
and three measured repetitions per variant. The following inputs are 1 GiB,
128 bytes per word, `-cp` and `-m 128M`; medians include copy and final cleanup.

| Workers | Unique | Previous auto (s) | Revised auto (s) | Time saved |
|---:|---:|---:|---:|---:|
| 1 | 10% | 5.126 | 5.630 | -9.8% |
| 1 | 50% | 17.367 | 6.531 | 62.4% |
| 1 | 90% | 27.089 | 7.083 | 73.9% |
| 4 | 10% | 2.371 | 5.075 | -114.1% |
| 4 | 50% | 7.829 | 5.976 | 23.7% |
| 4 | 90% | 10.896 | 6.181 | 43.3% |

The low-uniqueness regressions are real. At 10%, mmap quickly disables most
later occurrences and avoids many of the worst-case scans. A bounded distributed
sample cannot reliably infer that global uniqueness, whereas longer 50/90%
jobs benefit substantially from partitions. The four-worker 10% control loses
2.704 seconds from an originally short job. The one-worker case loses 0.504
seconds. There is no claim of universal timing improvement or an average
percentage that hides these cases.

A 16-batch prototype kept mmap at four workers and 90% unique: median 10.689
seconds versus 6.533 for forced partitions. The retained eight-batch margin
corrects that choice. Single-worker confirmation was measured with the earlier
prototype; its selected engine, dimensions, resource checks and execution path
are identical under the retained margin. The unchanged 257 MiB short-line
control remains near 3.5 seconds; its small difference is not a performance
conclusion.

## Remaining estimation limits

The choice remains conservative for very large bitmaps. On 128 MiB of eight-byte
words, 90% unique and `-m 4M`, forced partitions take 28.14 seconds with one
worker versus 38.07 for forced mmap. Automatic selection retains mmap because
the pessimistic private-pass volume is larger. With four workers that decision
protects a much larger loss: mmap takes 12.30 seconds versus 28.34 for partitions.
At twelve workers the corresponding observations are 8.27 and 29.40 seconds.
The traversal model does not know relative cache latency and CPU costs.

At twelve workers, the 1 GiB long-line 90% case also keeps mmap, although forced
partitions win the exploratory trial by about 2.5 seconds. Raising a worker
count exposes parallel tasks; it cannot predict shared memory-bandwidth limits.
These residual errors are why the engine option is useful and why the selector
is presented as a heuristic, rather than a guaranteed timing oracle.

Raw data: [forced-engine matrix](engines/forced.jsonl),
[first confirmation](engines/confirmation-16-batches.jsonl),
[final confirmation](engines/confirmation.jsonl),
[environment and binary hashes](engines/environment.json).

Generated inputs remain locally under `.benchmarks/inputs`, including
`1024mib-128bytes-50pct.txt`, `1024mib-128bytes-90pct.txt` and
`257mib-8bytes-90pct.txt`. The cache ignores itself and is not committed.
For a manual comparison, use the same input, options and output filesystem,
changing only `-e mmap-chunks`, `-e disk-partitions` or `-e auto`.

The 20 GiB external run and actual `bigwl` results in the memory report retain
the same engine and execution dimensions under this selector. Their measurements
are reused as such, rather than presented as new runs of the selector.

Validation includes all ten normal test scripts, strict GCC and Clang builds,
AddressSanitizer/UndefinedBehaviorSanitizer configuration, saturation and engine
tests, and forced engines in a build with multithreading disabled. Working
memory and partition tests also run with one, two and three requested workers,
including the former multiworker assumptions in production-dispatch fixtures.
The benchmark helper's public engine options have a separate functional smoke
test. No macOS or HDD execution is claimed.
