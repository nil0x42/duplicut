# Hash table saturation

These measurements describe `991b765`, before the
[sampled sizing and engine integration](integration.md).

The regression input contains 4096 distinct three-byte hexadecimal words,
repeated twice. With `-m 64K`, the original `mmap-chunks` engine hangs while searching
a full hash table. The same failure was reproduced in `a4a5d80` and `ab958d2`.

The former insertion loop limited probes, not occupancy. Exhausting its
unsigned countdown wrapped the counter and bypassed the error check; reaching
an empty slot on the last allowed probe could instead report an error.
Subsequent lookups relied on finding an empty entry and could loop forever.

## Change

Count actual inserted representatives and stop accepting new ones at 70 percent
occupancy. The target load used for chunk sizing remains 50 percent, the memory
budget and eight-byte line estimate remain unchanged. This is an insertion
safety limit, independent of the line-length sampler.

When a new representative cannot be inserted, leave it at the start of the
unindexed remainder. Compare that remainder and all following chunks with the
current table, wait for workers, then populate the next table from the remaining
lines. Nominal chunk boundaries stay unchanged. Each pass makes progress and
keeps the earliest occurrence; there is no table growth or extra data structure.

Parallel lookup code in `cleanout_chunk()` is unchanged. The extra scheduling
and progress accounting run only when the insertion limit is reached. Ordinary
insertions count entries; the former per-probe countdown is removed.

## Correctness

The suite has seven scripts, including 1080 existing differential cases.
Saturation cases check unique output byte for byte and duplicate occurrence
counts against the Python reference with one and several threads. They cover
two- and three-byte words, 10/50/90 percent uniques, filters, case conversion,
CRLF, valid and filtered final lines without LF, and piped input/output.

A temporary stress build used a constant hash and an oversized line-size
estimate. The current test lowers its insertion limit instead, since sampled
sizing supersedes that estimate. Both require several passes within one
nominal chunk and check collision handling beyond the old probe limit.
This does not change production limits.

## Performance

Release builds use the same GCC 16.2.1 flags:
`-std=gnu99 -O2 -DNDEBUG -mtune=native -ffast-math`, pthreads, and identical
source ordering. The reference is `ab958d2`. Hardware and timing conventions
match the [partition campaign](partitions.md). All cases use the `mmap-chunks` engine
and `-t 4`, without duplicate-file output.

Inputs contain cyclic identifiers padded to the requested length, with LF.
Four- and five-byte identifiers use base 94, alphabet ASCII 33 through 126;
eight- and 128-byte identifiers use hexadecimal, width `min(length, 16)`.
The line count is `floor(source_bytes / (length + 1))`; the number of distinct
identifiers is `floor(line_count * unique_percent / 100)`. Generation occurs
outside timing. All output SHA-256 hashes match the reference.

Each case has one warm-up and three measured repetitions in mixed order.
Output synchronization happens outside process timing, before the next trial.
These measurements check for meaningful regressions and the benefit of lower
occupancy; they are not evidence for small percentage improvements.

| Input | Bytes/line | Unique | `-m` | Before (s) | After (s) | Time saved |
|---|---:|---:|---|---:|---:|---:|
| 128 MiB | 4 | 50% | 64M | 6.442 | 5.017 | 22.1% |
| 128 MiB | 4 | 90% | 64M | 11.492 | 8.333 | 27.5% |
| 128 MiB | 5 | 90% | 64M | 5.822 | 5.770 | 0.9% |
| 512 MiB | 8 | 50% | 256M | 6.142 | 6.153 | -0.2% |
| 512 MiB | 8 | 90% | 256M | 9.306 | 9.274 | 0.3% |
| 2 GiB | 128 | 50% | 200M | 6.241 | 6.154 | 1.4% |
| 2 GiB | 128 | 90% | 200M | 7.982 | 9.247 | -15.8% |

The 4-byte inputs exceed the insertion ceiling and benefit from lower probe
costs despite the additional passes. The 5- and 8-byte controls are neutral.

The first 128-byte / 90-percent series has variable wall times: the reference
ranges from 7.90 to 10.21 seconds and the candidate from 8.26 to 10.46 seconds.
Its elapsed median alone does not establish an implementation regression.
Median CPU time is 22.41 versus 22.57 seconds.

That case was repeated with seven measured trials and both binaries pinned
to physical performance cores 0, 2, 4 and 6, still using `-t 4`:

| Controlled 2 GiB / 128-byte / 90% case | Before | After |
|---|---:|---:|
| Median process time (s) | 8.278 | 8.051 |
| Median CPU user + system (s) | 22.70 | 22.74 |

This control does not show the elapsed slowdown from the initial three-trial
series, and CPU time remains essentially unchanged. No small speedup is claimed.

Raw records: [initial trials](saturation/trials.jsonl),
[controlled trials](saturation/pinned.jsonl),
[build metadata](saturation/environment.json).

The normal suite, targeted saturation tests, ASan/UBSan and a Clang serial
build pass. Release and DEBUG=3 builds also pass with `-Wall -Wextra -Werror`.
ASan functional tests run with leak detection disabled in the sandbox.
