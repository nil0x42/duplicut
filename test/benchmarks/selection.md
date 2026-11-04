# Partition selection

This audit describes `6107d5f`. The later
[configuration integration](integration.md) shares sampling with adaptive table
and chunk sizing, while retaining the selection thresholds described here.

The selection audit replaces the absolute 256 MiB gate introduced with the
[initial partition measurements](partitions.md). A fixed input size does not
describe the cost of deduplication on machines with different memory budgets.

Selection uses three independent quantities:

- Input size and estimated chunk count, using the actual configured hash table.
  This reflects `-m` and the memory estimate taken during configuration.
- Current system memory availability, without applying `-m`, with space reserved
  for the hash table. Limiting allocations does not make a cached file cold.
- Average accepted line length from four distributed windows, reading at most
  64 KiB. Filters and partial boundary lines use the existing parsing rules.

## Rules

At least 16 estimated chunks are required. For inputs that fit alongside the
hash table in available memory, long lines require eight times this margin:
128 chunks. Short lines can use partitions at 16 chunks. Inputs that do not fit
alongside the table also use the 16-chunk threshold, regardless of line length.
There is no absolute input-size threshold.

The 16-chunk threshold compares repeated source traversal with partitioning's
fixed number of full scans and extra I/O. The eightfold cached-long margin is
conservative: cached scans are cheap, while partitioning still copies and
hashes complete contents. It also leaves room for the low end of the target
range: at 10 percent unique lines, many `mmap-chunks` tables can become empty early.
Neither value predicts time in seconds on a given CPU.

The length boundary is twice `sizeof(t_line)`, currently 16 bytes. With half
of a worker's memory assigned to its table, a half-full table can retain
`memory / (4 * sizeof(t_line))` entries. The other half holds contents and
becomes the limiting allocation above this average length. This explains the
storage tradeoff; it is not a proof of a universal timing crossover.

## Audit of the other limits

| Choice | Decision | Reason |
|---|---|---|
| 256 MiB minimum | Removed | A smaller file can require many passes with little memory. |
| Input versus available memory | Reserve the table too | The `mmap-chunks` table can otherwise evict pages assumed to fit. |
| Always reject cached long lines | Removed | Many passes can outweigh copying even without disk pressure. |
| 16 estimated chunks | Retained | A ratio to effective memory, independent of absolute RAM size. |
| Four 16 KiB sample windows | Retained | Bounded estimation work, without a preliminary full scan. |
| 4096 partitions maximum | Removed | Metadata and scatter buffers already have a budget-derived bound. |
| Power-of-two partition count | Retained | Allows masked hash routing and bounded overflow passes. |
| 1 MiB minimum admission | Removed | Small budgets can still support one useful worker. |
| 1 MiB worker granularity | Retained as 64 I/O buffers | Limits scratch overhead when sharing memory among workers. |
| Workers chosen before bitmap | Corrected | Worker count is capped again after exact bitmap and metadata sizes. |
| Bitmap at most half the engine budget | Retained | Leaves memory for private tables, contents and scratch space. |
| Scatter buffers at most half the engine budget | Retained | Bounds allocations regardless of file size or CPU count. |
| Disk preflight and fallback | Retained | Optional partitioning must not consume RAM through a temporary filesystem. |

For a single worker below 1 MiB, usable memory must be at least four times
scratch space: two I/O buffers plus the maximum line length. The same check
is repeated after subtracting metadata and the bitmap. This is about 144 KiB
with current buffer and line limits; it is an overhead bound, not a minimum
wordlist size. When enough usable memory remains, each worker receives at least one
64-buffer granule; smaller usable budgets run one worker.
`-t` remains a maximum. The former global table budget is reused; `-m` still
does not cap mapped pages or total RSS.

## Validation

The normal production binary is tested on an 8 MiB input with several `-m`
budgets: a few chunks keep the `mmap-chunks` engine, many chunks enable partitioning,
and insufficient scratch space keeps the `mmap-chunks` engine. A complete partition
run below the former 1 MiB admission limit verifies output correctness.

Linux tests inject available-memory values from 4 MiB to 128 GiB. They check
table headroom, cached long lines, many-pass cached inputs and the absence of
the former absolute size gate. The existing 10/50/90 percent functional tests,
collisions, bounded overflow, filters, I/O failures and output order still run.

The performance comparison uses release binaries with the same GCC flags and
options: `63cfc02` versus the selector finalized in `6107d5f`. Each case has one warm-up and
three measured repetitions, with mixed order and matching output SHA-256.
These runs look for substantial selection errors; they do not establish small
percentage changes on fast jobs.

| Source | Bytes/line | Unique | `-m` | Before (s) | Revised (s) | Time saved |
|---|---:|---:|---|---:|---:|---:|
| 128 MiB | 8 | 50% | 8M | 2.637 | 1.407 | 46.6% |
| 128 MiB | 8 | 90% | 8M | 4.433 | 1.963 | 55.7% |
| 1 GiB | 128 | 10% | 32M | 1.532 | 1.598 | -4.3% |
| 1 GiB | 128 | 50% | 32M | 5.092 | 1.652 | 67.6% |
| 1 GiB | 128 | 90% | 32M | 7.999 | 2.091 | 73.9% |
| 4 GiB | 128 | 10% | 128M | 14.747 | 9.712 | 34.1% |

All rows use `-t 4`. The small 1 GiB / 10-percent difference is not treated
as a performance conclusion. The 4 GiB case has substantial variability:
the reference spans 9.12 to 16.39 seconds, the candidate 8.57 to 18.46 seconds.
Its median is indicative, not evidence of a consistent gain. The revised
selector is supported primarily by the substantial improvements on the
50/90-percent workloads and the production dispatch tests.

The measured development build used a 64-chunk cached-long threshold. The
final policy conservatively raises this to 128 chunks to leave more margin
for 10-percent-unique inputs. Every timed long-line input has 128 estimated
chunks, so its selected engine and allocation parameters remain the same.

Raw trials: [128 MiB](selection/128mib.jsonl),
[1 GiB long lines](selection/1024mib-long.jsonl),
[4 GiB long lines](selection/4096mib-long.jsonl).

The earlier 20 GiB case remains on the same external path with the same
partition count, worker count and private memory sizes. It is not rerun
or presented as a new measurement in this audit.

## Limits

Memory availability is a changing estimate, not a measurement of the working
file's residency. The existing macOS estimate reports free pages and is more
conservative than Linux `MemAvailable`. The policy uses the same portable
decisions; it does not make the two estimates identical.

RAM size alone cannot identify CPU speed, memory bandwidth, storage latency,
writeback pressure or global uniqueness from a small sample. Distributed
windows reduce prefix bias, but cannot describe every heterogeneous input.
No guessed global duplicate percentage, CPU-count speedup model or runtime
calibration is added. Overflow passes and fallbacks preserve correctness when
the estimate is poor.

Available-memory injection checks selection, not the performance of physical
4 GiB or 128 GiB hosts. Timings remain Linux/NVMe measurements on the original
machine; macOS and rotating storage have not been benchmarked. A resource-based
policy is portable, but does not guarantee the fastest engine on every host.
