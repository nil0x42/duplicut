#!/usr/bin/env python3

import os
import runpy
import shlex
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path
from build import build


reference = runpy.run_path(str(Path(__file__).with_name('reference-dedupe.py')))
dedupe = reference['dedupe']
line_count = reference['line_count']


def main():
    root = Path.cwd().resolve()
    with tempfile.TemporaryDirectory(prefix='duplicut-engines-', dir=root) as folder:
        directory = Path(folder)
        program = (Path(sys.argv[1]).resolve() if len(sys.argv) > 1
                   else build(root, directory, 'debug', '-DDEBUG=1'))
        source, output, duplicates = (directory / name for name in ('input', 'output', 'duplicates'))
        content = (b'Alpha\r\nalpha\n\0hidden\nA\xff\nB\0C\nB\0C\nsmall\nSMALL\n'
                   + b'z' * 4095 + b'\n') * 64 + b'last'
        source.write_bytes(content)
        cases = (((), False, False, False), (('-c',), False, True, False),
                 (('-C',), False, False, True), (('-p',), True, False, False),
                 (('-cp',), True, True, False), (('-Cp',), True, False, True))
        for engine in (None, 'auto', 'mmap-chunks', 'disk-partitions'):
            for threads in sorted({1, min(4, os.cpu_count() or 1)}):
                for flags, printable, lowercase, uppercase in cases:
                    expected, removed = dedupe(content, 4095, printable, lowercase, uppercase)
                    command = [str(program), str(source), '-o', str(output), '-D', str(duplicates),
                               '-m', '8M', '-l', '4095', '-t', str(threads), *flags]
                    if engine:
                        command += ['--engine', engine]
                    result = subprocess.run(command, stderr=subprocess.PIPE, timeout=10)
                    assert result.returncode == 0, (engine, flags, result.stderr)
                    assert output.read_bytes() == expected, (engine, flags)
                    assert Counter(duplicates.read_bytes().split(b'\n')) == Counter(removed.split(b'\n'))
                    counts = b'%d duplicates and %d filtered lines' % (
                        line_count(removed), line_count(content) - line_count(expected) - line_count(removed))
                    assert counts in result.stderr, result.stderr
                    if engine in ('mmap-chunks', 'disk-partitions'):
                        assert ('dedupe engine: ' + engine).encode() in result.stderr
            print('[+] %s, thread limits, filters, case conversion and counters' % (engine or 'default'))
        for engine in ('mmap-chunks', 'disk-partitions'):
            for content in (b'', b'\0ignored\n', b'x' * 5000, b'Alpha\nalpha'):
                result = subprocess.run([str(program), '-e', engine, '-m', '8M', '-cp'],
                                        input=content, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, timeout=10)
                assert result.returncode == 0, result.stderr
                assert result.stdout == dedupe(content, 1024, True, True)[0]
        print('[+] short option, pipes, empty input, filtered input and final lines')
        for value in ('', 'legacy', 'AUTO', 'unknown'):
            result = subprocess.run([str(program), '--engine=' + value], stderr=subprocess.PIPE)
            assert result.returncode != 0 and b"for '--engine'" in result.stderr
        result = subprocess.run([str(program), '-e'], stderr=subprocess.PIPE)
        assert result.returncode != 0
        if (os.cpu_count() or 1) > 1:
            words = b''.join(('%08x\n' % i).encode() for i in range(4096))
            source.write_bytes(words * 8)
            command = [str(program), str(source), '-o', str(output), '-m', '640K',
                       '-t', str(min(4, os.cpu_count() or 1))]
            result = subprocess.run(command, stderr=subprocess.PIPE, timeout=10)
            assert result.returncode == 0 and output.read_bytes() == words, result.stderr
            assert b'dedupe engine: mmap-chunks' in result.stderr, result.stderr
            result = subprocess.run([*command, '-e', 'disk-partitions'],
                                    stderr=subprocess.PIPE, timeout=10)
            assert result.returncode == 0 and output.read_bytes() == words, result.stderr
            assert b'1 workers' in result.stderr, result.stderr
            print('[+] parallel admission uses workers supported by the memory budget')
        result = subprocess.run([str(program), str(source), '-o', str(output),
                                 '-e', 'disk-partitions', '-m', '64K'], stderr=subprocess.PIPE)
        assert result.returncode != 0 and b'disk-partitions engine unavailable' in result.stderr
        if sys.platform == 'linux':
            library = directory / 'faults.so'
            compiler = shlex.split(os.environ.get('CC', 'cc'))
            subprocess.run([*compiler, '-shared', '-fPIC', '-Wall', '-Wextra',
                            str(root / 'test/scripts/partition-faults.c'), '-ldl',
                            '-o', str(library)], check=True)
            env = {**os.environ, 'LD_PRELOAD': str(library), 'DUPLICUT_TEST_FAULT': 'space'}
            command = [str(program), str(source), '-o', str(output), '-m', '8M']
            result = subprocess.run([*command, '-e', 'disk-partitions'], env=env, stderr=subprocess.PIPE)
            assert result.returncode != 0 and b'disk-partitions engine unavailable' in result.stderr
            result = subprocess.run([*command, '-e', 'mmap-chunks'], env=env, stderr=subprocess.PIPE)
            assert result.returncode == 0, result.stderr
            for available in (4 * 1024, 4 * 1024 * 1024, 128 * 1024 * 1024):
                for threads in sorted({1, min(4, os.cpu_count() or 1)}):
                    env = {**os.environ, 'LD_PRELOAD': str(library),
                           'DUPLICUT_TEST_AVAILABLE': str(available)}
                    result = subprocess.run([str(program), str(source), '-o', str(output),
                                             '-t', str(threads)], env=env,
                                            stderr=subprocess.PIPE, timeout=10)
                    assert result.returncode == 0, result.stderr
                    assert output.read_bytes() == dedupe(source.read_bytes(), 1024)[0]
            print('[+] automatic selection across available memory and thread counts')
        print('[+] invalid options and explicit forced-engine resource failures')


if __name__ == '__main__':
    main()
