# Documentation

Duplicut keeps the first occurrence of each accepted line in its original order.
Start with the [quick start and option table](../README.md#quick-start), then
choose the detail you need below.

## Use Duplicut

- [Usage](usage.md): line handling, normalization, output files, pipes and append.
- [Memory and disk](resources.md): budgets, mapped pages and temporary storage.
- [Troubleshooting](troubleshooting.md): progress, engine fallback, errors and logs.

## Understand the engines

Read the [shared workflow and example](architecture.md) first. Then compare:

- [mmap-chunks](engines/mmap-chunks.md): index one chunk, search later chunks.
- [disk-partitions](engines/disk-partitions.md): group records, deduplicate locally.
- [Engine selection](engine-selection.md): how automatic mode chooses and checks a plan.
- [Performance](performance.md): observed results, tradeoffs and measurement context.

## Develop and verify

- [Development](development.md): building, debug builds and release binaries.
- [Testing](testing.md): checks, reference behavior and benchmark procedure.

Each topic has one reference page. Links inside the explanations lead to the
relevant details rather than repeating them.
