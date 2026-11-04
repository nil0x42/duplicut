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
