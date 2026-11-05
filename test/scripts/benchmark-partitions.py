#!/usr/bin/env python3

import argparse
import hashlib
import json
import os
import random
import statistics
import subprocess
import tempfile
import time
from pathlib import Path


def generate(path, size, length, percent):
    count = size // (length + 1)
    unique = max(1, count * percent // 100)
    width = min(length, 16)
    assert unique <= 16 ** width
    with path.open('wb', buffering=1024 * 1024) as stream:
        for start in range(0, count, 65536):
            stream.write(b''.join(('%0*x' % (width, i % unique)
                                  + 'x' * (length - width) + '\n').encode()
                                 for i in range(start, min(start + 65536, count))))
    with path.open('rb') as stream:
        os.fsync(stream.fileno())
    return count, unique


def run(program, source, output, memory, threads, usage, engine=None):
    command = ['/usr/bin/time', '-f', '%U %S %M %I %O %F %R', '-o', str(usage),
               str(program), str(source), '-o', str(output), '-m', memory,
               '-t', str(threads)]
    if engine:
        command += ['--engine', engine]
    start = time.monotonic()
    result = subprocess.run(command, stderr=subprocess.PIPE, timeout=3600)
    elapsed = time.monotonic() - start
    assert result.returncode == 0, (command, result.returncode, result.stderr)
    sync_start = time.monotonic()
    with output.open('rb') as stream:
        os.fsync(stream.fileno())
        sync_seconds = time.monotonic() - sync_start
        digest = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    user, system, rss, inputs, outputs, major, minor = usage.read_text().split()
    return dict(seconds=elapsed, sync_seconds=sync_seconds, sha256=digest.hexdigest(),
                user=float(user), system=float(system), rss_kib=int(rss),
                input_blocks=int(inputs), output_blocks=int(outputs),
                major_faults=int(major), minor_faults=int(minor), command=command)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    engines = ('auto', 'mmap-chunks', 'disk-partitions')
    parser.add_argument('--baseline-engine', choices=engines)
    parser.add_argument('--candidate-engine', choices=engines)
    parser.add_argument('--size-mib', type=int, default=1024)
    parser.add_argument('--lengths', nargs='+', type=int, default=[16, 128])
    parser.add_argument('--unique', nargs='+', type=int, default=[10, 50, 90])
    parser.add_argument('--memory', default='100M')
    parser.add_argument('--threads', type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument('--repetitions', type=int, default=7)
    parser.add_argument('--directory', type=Path, default=Path('.'))
    parser.add_argument('--results', type=Path, required=True)
    args = parser.parse_args()
    assert args.size_mib > 0 and args.repetitions > 0 and args.threads > 0
    assert all(1 <= length <= 4095 for length in args.lengths)
    assert all(1 <= percent <= 100 for percent in args.unique)
    programs = dict(baseline=args.baseline.resolve(), candidate=args.candidate.resolve())
    engines = dict(baseline=args.baseline_engine, candidate=args.candidate_engine)
    rng = random.Random(22)
    with tempfile.TemporaryDirectory(prefix='duplicut-benchmark-', dir=args.directory) as folder:
        folder = Path(folder).resolve()
        source, output, usage = (folder / name for name in ('input', 'output', 'usage'))
        for length in args.lengths:
            for percent in args.unique:
                count, unique = generate(source, args.size_mib * 1024 * 1024, length, percent)
                times = {name: [] for name in programs}
                expected = None
                for repeat in range(-1, args.repetitions):
                    names = list(programs)
                    rng.shuffle(names)
                    for name in names:
                        row = run(programs[name], source, output, args.memory,
                                  args.threads, usage, engines[name])
                        if expected is None:
                            expected = row['sha256']
                        assert row['sha256'] == expected, (name, length, percent)
                        row.update(variant=name, repeat=repeat, length=length,
                                   unique_percent=percent, source_bytes=source.stat().st_size,
                                   lines=count, unique_lines=unique,
                                   memory=args.memory, threads=args.threads)
                        with args.results.open('a') as stream:
                            stream.write(json.dumps(row) + '\n')
                        if repeat >= 0:
                            times[name].append(row['seconds'])
                        print(name, length, percent, repeat, round(row['seconds'], 3), flush=True)
                baseline = statistics.median(times['baseline'])
                candidate = statistics.median(times['candidate'])
                print('gain: %.2f%%' % (100 * (1 - candidate / baseline)), flush=True)


if __name__ == '__main__':
    main()
