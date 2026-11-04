# Tests

Run from the repository root:

```sh
make test
TEST=20-differential.sh make test  # run one test script
```

The automated suite checks builds and exercises the CLI, file aliases and stdout
through black-box system tests. Differential regression tests compare outputs
with a Python reference implementation across wordlists and options.
Each duplicut run in the differential tests has a five-second timeout to
catch hangs; this is not a benchmark. The suite builds with coverage enabled.

The stdout tests cover shell redirections, append mode, inherited file positions,
pipes, FIFOs, terminals, write errors and temporary file cleanup. Targeted cases
also compare stdout and duplicate lines with the Python reference. Subprocesses
and blocking operations in the Python tests have five-second timeouts. The
write-only permissions case is skipped explicitly if read access is not
restricted (for example, when running as root).

The differential tests use one Python process to run cases concurrently.
The default is half the available logical CPUs, rounded up (including Linux
affinity limits). Each duplicut process also has its own threads.
The selected job count is printed at startup. Set `TEST_JOBS=1` to run
sequentially, or use any positive integer to choose the number of concurrent
cases. Each case uses its own temporary directory and keeps duplicut's default
threads.
Unique lines are compared byte for byte; duplicate lines are compared without
ordering, keeping their occurrence counts. Nonzero exits, missing output files
and timeouts fail the test. On failure, `differential-*.out` files in the
repository root contain the expected and actual outputs for inspection.

On failure, pending cases are cancelled and running processes are allowed to
finish. Only the first failure received is reported; it may vary between runs.

Saturation tests exercise short words with a small table, forced collisions,
remaining lines in a saturated chunk, filters and final lines. They compare
stable output and duplicate counts with the Python reference, using one or
several threads and a five-second subprocess timeout.

Configuration tests check bounded shared samples, minimum window density,
allocation budgets, chunk sizing and repeated saturation after a misleading
sample. Debug builds exercise both engine plans, confirmed allocations and
disk preflight fallback. Linux tests also verify that configuration reads
available memory once. Their subprocess timeout is thirty seconds.


The partition tests build a small-input variant to exercise bounded memory,
collisions, filters, output order and concurrent bitmap updates. Linux tests
also inject partial I/O, interruptions, EOF and disk-full errors, and check
volume selection, immediate unlink and the production activation boundary.
These tests use temporary directories under the repository so they can run
on a disk-backed filesystem. Their subprocess timeout is thirty seconds.

To compare two release binaries on Linux:

```sh
python3 test/scripts/benchmark-partitions.py /path/to/baseline /path/to/candidate \
    --size-mib 1024 --memory 100M --results /tmp/partitions.jsonl
```

The benchmark generates deterministic inputs with 10, 50 and 90 percent unique
lines, checks output hashes and records raw timings and resource use. It runs
one warm-up and seven measured repetitions per variant. Output synchronization
runs outside process timing, before the next trial. Run benchmarks separately
from tests and builds; use a disk-backed `--directory` with space for inputs,
outputs and partitions. `--lengths`, `--threads` and `--repetitions` narrow a run.
Recorded measurements and selection rules are in the
[partition benchmarks](benchmarks/partitions.md).
The [selection audit](benchmarks/selection.md) explains the resource-based rules
and their validation across available-memory estimates.
