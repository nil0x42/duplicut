# Development

[Documentation](README.md) · [Testing](testing.md) · [Troubleshooting](troubleshooting.md)

Use a normal release build for wordlist processing, and a debug build when
investigating behavior. Documentation and diagnostic examples assume commands
are run from the repository root.

## Building

Native builds require a C compiler and `make` on 64-bit Linux or macOS. Hashing
code is bundled; no separate hashing library is required. The build links the
system math and pthread libraries.

```sh
make
```

To choose a compiler or require warnings to be clean:

```sh
make CC=clang FLAGS=-Werror
```

Precompiled binaries are available from the
[latest release](https://github.com/nil0x42/duplicut/releases/latest). Choose the
OS and architecture, give the downloaded binary a convenient name such as
`duplicut`, and make it executable with `chmod +x duplicut`.

The executable can be run directly. `make clean` removes intermediate build
files while keeping it; `make distclean` removes it as well. On systems with a
compatible `install` command, the Makefile also provides `install` and `uninstall`
targets with `PREFIX` and `DESTDIR` support.

## Debug builds

```sh
make debug level=1
```

Levels 1–4 add progressively more detail:

| Level | Useful information |
|---:|---|
| 1 | Samples, budgets, plans, rejection reasons and phase transitions. |
| 2 | Additional hashmap and partition-pass summaries. |
| 3 | More detailed occupancy and content-storage information. |
| 4 | Verbose low-level diagnostics. |

Debug builds use different optimization settings and may emit substantial logs;
they are not performance baselines. Optional `ctags` generation helps editor
navigation but is not required to run Duplicut. The
[reporting recipe](troubleshooting.md#collect-a-useful-report) keeps output data
separate from diagnostics.

## Tests and profiling

Follow the [testing guide](testing.md) for the full suite, focused tests and
reference behavior. The Makefile also offers `coverage`, `profile`,
`profile-generate` and `profile-use`; profile generation and use need matching
builds and representative workloads. Normal operation does not require them.

Keep changes focused, follow the existing project style and update the relevant
documentation when user-visible behavior changes.

## Release binaries

`test/build-releases.sh` uses Zig and `make` to cross-compile four targets:
Linux x64/ARM64 and macOS x64/ARM64. Linux targets use static musl builds. The
macOS targets are x64 from 10.13 and ARM64 from 11.0. The script prefers
`llvm-strip` when available and names binaries using the Git description.

Run it with Bash supporting associative arrays, such as Bash 4 or newer:

```sh
bash test/build-releases.sh
```

This build process is separate from the compiler choices exercised by CI. It
generates local artifacts; it does not create or publish a GitHub release.
