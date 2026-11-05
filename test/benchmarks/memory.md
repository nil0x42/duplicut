# Working memory and bitmap admission

These measurements cover `de5ba3c`, following the counter correction in
`bf94d23`. The older reports describe earlier allocation and selection rules.
All reported outputs have matching SHA-256 and duplicate/filter counts.

## Actual input

The local `bigwl` contains 2,770,048,320 bytes. With `-cp`, both engines remove
254,356,632 duplicates and 219,114 filtered lines. The stable output contains
30,618,150 lines and 310,830,864 bytes. Its SHA-256 is
`beea1f7cba60864dc7f90492a269cc6bb123dbb10533ecb7d5c0e0cb8a2db64b`.

These runs redirect stdout to a private file on `/tmp`, a RAM-backed filesystem.
The control is the state after the counter fix; each control was measured once.
The candidate has three repetitions per budget, in rotating order. The table
reports candidate medians; modest timing differences need this qualification.

| Memory option | Control seconds | Candidate seconds | Time saved | Control peak RSS MiB | Candidate peak RSS MiB |
|---|---:|---:|---:|---:|---:|
| Default | 45.949 | 35.726 | 22.2% | 7,391.56 | 2,771.84 |
| `-m 100M` | 40.387 | 36.637 | 9.3% | 2,694.10 | 47.55 |
| `-m 300M` | 34.931 | 37.340 | -6.9% | 2,746.91 | 101.01 |

The limited-memory path now uses a disk working file before copying the final
result to RAM-backed stdout. This avoids expanding that output to the full
2.58 GiB input, at the cost of additional disk I/O. The 300M case shows that
cost; these changes do not make every invocation faster. The final RAM-backed
output still occupies about 296 MiB independently of the process memory budget.
Polling output size every 100 ms can miss the final copy; the raw sampled peaks
must not be interpreted as a bound below the final output size.

Default selection uses parallel partitions here, and is faster than the limited
cases in these observations. CPU time at 100M drops from 291.26 to a median
58.61 seconds because the useful large bitmap is no longer rejected blindly.

## Bitmap tradeoff

Removing the former half-budget bitmap limit unconditionally causes a serious
counterexample. Eight-byte words, 128 MiB input, 90% unique and `-m 4M` leave
only about 226 KiB for private workers after the bitmap. Repeated private
overflow then costs more than scanning mmap chunks.

The retained rule checks actual usable table and content capacities when the
bitmap exceeds half the engine budget. It compares worst-case private passes
with estimated mmap scan work, without guessing the global duplicate ratio.
The following medians have three repetitions after a warm-up, with four workers
pinned to physical cores, `-cp` and `-m 4M`.

| Input MiB | Unique | Original half limit (s) | Unconditional admission (s) | Bounded admission (s) | Bounded peak RSS MiB |
|---|---:|---:|---:|---:|---:|
| 96 | 50% | 4.145 | 3.863 | 3.791 | 3.84 |
| 96 | 90% | 7.005 | 5.848 | 5.738 | 3.14 |
| 128 | 90% | 12.686 | 27.875 | 12.486 | 21.48 |

The last row falls back to mmap chunks. Mapped scan windows markedly reduce
residency, but this deliberately tiny budget still exceeds `-m`: resident file
pages reach roughly 20 MiB while anonymous allocations remain near 2.3 MiB.
The memory option is a working-set estimate, not a hard RSS limit. Thread
stacks, runtime overhead, page boundaries and kernel mapping behavior also
prevent an exact cap. Larger real workloads below show much closer budgets.

## Input larger than physical RAM

A 20 GiB input exceeds this host's 15.34 GiB physical RAM. With 128-byte words,
50% unique, `-m 2G` and four physical cores, the retained implementation takes
140.093 seconds, using 674.35 MiB peak RSS. The preceding integration run took
144.515 seconds and about 11.96 GiB peak RSS. These are single runs: they
establish feasibility and memory reduction, not a reliable 3% timing gain.
The original master measurement was 588.080 seconds; it predates these fixes.

The partition file peaks at 22,973,165,568 allocated bytes, in addition to the
working output's 21,474,840,576 bytes. Output synchronization outside timing
takes 0.0186 seconds. The 10 GiB result matches the earlier output hash
`e8bd5036418d80492046912173d9182fd12ef071466d76a1b42bd674f9c3d8f1`.
Generated files were removed after this run.

Raw observations: [actual input](memory/bigwl.jsonl),
[bitmap controls](memory/bitmap-controls.jsonl),
[bounded admission](memory/bounded.jsonl), [20 GiB](memory/disk.json).

The host and storage are unchanged from the
[partition environment](partitions/environment.json). These are Linux/NVMe
measurements; no macOS or rotating-disk timing claim follows from them.
