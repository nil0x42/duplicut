# Testing

[Documentation](README.md) · [Development](development.md) · [Measurements](performance.md)

Run from the repository root with a C toolchain, `make`, Python 3 and the shell
utilities used by the test suite. Python tests use the standard library. The
suite builds instrumented and targeted variants as needed.

## Run the checks

```sh
make test
TEST=20-differential.sh make test
TEST=28-progress.sh make test
```

The first command runs the suite; the others select one script. CI checks GCC
and Clang on Linux and macOS, with separate ARM macOS coverage. Compiler and
coverage tools are configured by CI rather than required for ordinary use.

## What is verified

| Family | Coverage |
|---|---|
| Build and CLI | Make targets, option validation and executable behavior. |
| File aliases and stdout | Redirection, append, positions, pipes, FIFOs, terminals, write failures and cleanup. |
| Differential | Unique bytes, duplicate occurrence counts, filtering and case conversion against a Python reference. |
| Saturation | Short lines, collisions, partial chunks and bounded continuation. |
| Configuration and resources | Shared samples, density estimates, memory bounds, scan windows and misleading samples. |
| Partitions and engines | Forced and automatic modes, overflow, bitmap updates, exact counters, disk preflight and I/O failures. |
| Progress | Phase history, deterministic durations, narrow terminals, colors, restoration, long paths and fallback without backward progress. |

The reference compares unique output byte for byte. Duplicate output is checked
as a multiset because its order is not guaranteed. Duplicate and filtered counts
are checked too. Linux-specific fault injection exercises partial I/O, EINTR,
EOF and disk-full conditions; unsupported platform cases are skipped explicitly.

Subprocess timeouts catch hangs, not slow benchmarks. Differential and saturation
runs generally use five seconds, progress tests ten seconds, and configuration
and partition tests thirty seconds. Failure output identifies the case and,
where applicable, leaves expected/actual files for inspection.

## Control test concurrency

Differential cases run concurrently, using half the available logical CPUs
rounded up, including Linux affinity limits. Each Duplicut process has its own
worker limit. Set `TEST_JOBS=1` for sequential execution, or a positive integer
for a chosen job count. On failure, pending cases are cancelled and active ones
finish; the first reported failure can vary with scheduling.

## Benchmarking

Benchmark separately from tests, builds and other heavy jobs. Use release builds
with matching flags and options, and a disk-backed directory with space for
input, output and partitions. Compare output hashes before interpreting timings.

The Linux helper generates cyclic identifiers with 10/50/90% unique lines. It
runs one warm-up and seven measured repetitions per variant by default, shuffles
order, and records CPU, RSS, block I/O, faults and checksums. Generation and
output synchronization/checksumming occur outside process timing.

```sh
python3 test/scripts/benchmark-partitions.py /path/to/baseline /path/to/candidate \
    --size-mib 1024 --memory 100M --results /tmp/partitions.jsonl
```

To compare engines in the same executable, supply it as both positional arguments
and add `--baseline-engine mmap-chunks --candidate-engine disk-partitions`.
Omitting these engine flags also works with older binaries.

Use `--lengths`, `--unique`, `--threads` and `--repetitions` to focus a run, and
`--directory` to choose storage. A small `-m` exercises repeated passes but does
not reproduce a file larger than physical RAM. Include such a workload when
that is the behavior being evaluated. The [performance page](performance.md)
separates these situations and indexes the recorded data.
