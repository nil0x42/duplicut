# Engine selection

[Documentation](README.md) · [Resources](resources.md) · [Observed tradeoffs](performance.md#engine-tradeoffs)

`auto` is intended for normal use. It compares input characteristics and usable
resources, not a fixed file-size range in gigabytes. The same input can warrant
a different engine under a different budget or worker count.

Selection has two stages: a bounded sample identifies a candidate, then a
partition candidate receives exact counting and resource checks before writes.

## Initial candidate

The [density estimates](resources.md#table-and-chunk-sizing) provide a nominal
chunk size. Input bytes divided by that size, rounded up, estimate the number
of chunks. Saturation and duplicate elimination can change the actual work.

The following rules apply together:

1. Without a usable accepted-line sample or chunk size, retain mmap.
2. Normally, require at least **16 estimated chunks** to consider partitions.
3. Allow an exception for parallel insertion of short lines when there are at
   least two usable partition workers, chunks do not outnumber those workers,
   input bytes are at least sampled bytes times usable workers, and mean
   accepted contents are at most **16 bytes**.
4. For longer lines whose input plus global table fit in available system memory,
   also require at least **eight lookup batches**.

A lookup batch is the estimated chunk count divided by the maximum mmap lookup
workers, rounded up. A serial build uses one worker. For 32 chunks, this is
32 batches with one worker, eight with four, and three with twelve. The first
two meet the cached-long rule; the last does not.

The short-line exception uses workers supported by the partition layout and
memory budget, not just the requested thread count. Cached availability uses
system memory before applying `-m`: limiting allocations does not itself make
the file cold.

The length boundary corresponds to twice the size of a packed line reference.
The chunk and batch margins amortize partitioning's extra scans and I/O; they
do not predict elapsed time or linear scaling with cores.

## Confirmation and fallback

A candidate needs a feasible layout with room for metadata, buffers and workers.
The minimum is tied to scratch space, not a fixed wordlist size. A counting scan
then provides exact occurrence counts, content sizes and partition ranges.

The [bitmap and private storage](resources.md#partition-storage) must fit. Worker
count is reduced if necessary after those reservations. When the bitmap uses
at most half the engine allocation budget, the feasible plan is admitted.

Above that fraction, selection compares approximate traversal volumes:

```text
private passes = ceil(max(
    accepted occurrences / (partitions × usable entries per worker),
    accepted content bytes / (partitions × content capacity per worker)))

partition work = 3 × input bytes + (private passes + 1) × partition-file bytes
mmap work      = input bytes × (estimated chunks + 1) / 2
```

Automatic selection rejects partitioning when its estimated work is at least
mmap's. The capacity estimate treats occurrences as distinct and assumes balanced
ranges; it does not model every possible hash imbalance or duplicate saving.
These are logical byte-work estimates, not measurements of CPU or disk time.

A suitable temporary file and sufficient disk space must also be available.
A declined candidate falls back before partition writes, with a short reason.
An I/O failure after writing begins is an error rather than a silent retry.
The [phase history](troubleshooting.md#reading-the-progress) distinguishes
checking a candidate from actually running its engine.

## Forcing an engine

- `-e mmap-chunks` skips partition selection and counting.
- `-e disk-partitions` bypasses performance preferences but retains resource checks.
  If unavailable, it fails explicitly instead of switching engines.

Use forcing to diagnose a particular workload. Comparisons need matching flags,
budgets, destinations and system load; see [measurement procedure](testing.md#benchmarking).

## What the heuristic cannot know

A small sample does not reveal global uniqueness. At 10% unique lines, mmap
may eliminate most later work early; at 90%, many more scans can remain useful.
CPU speed, cache latency, shared memory bandwidth, storage latency and concurrent
jobs also affect which engine wins.

More workers help both engines differently. They enable private-table insertion
for partitions and concurrent later-chunk searches for mmap; serial scans and
disk traffic do not become proportionally faster. The rules aim for useful,
portable choices, not a guarantee of the fastest engine on every input.
