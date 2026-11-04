# Tests

Run from the repository root:

```sh
make test
TEST=20-differential.sh make test  # run one test script
```

The automated suite checks builds and exercises the CLI and file aliases
through black-box system tests. Differential regression tests compare outputs
with a Python reference implementation across wordlists and options.
Each duplicut run in the differential tests has a five-second timeout to
catch hangs; this is not a benchmark. The suite builds with coverage enabled.

The differential tests use one Python process to run all cases sequentially.
Unique lines are compared byte for byte; duplicate lines are compared without
ordering, keeping their occurrence counts. Nonzero exits, missing output files
and timeouts fail the test. On failure, `differential-*.out` files in the
repository root contain the expected and actual outputs for inspection.
