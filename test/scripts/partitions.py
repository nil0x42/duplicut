#!/usr/bin/env python3

import os
import runpy
import shlex
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path


THREADS = str(min(4, os.cpu_count() or 1))


dedupe = runpy.run_path(str(Path(__file__).with_name('reference-dedupe.py')))['dedupe']


def run(command, success=True, **kwargs):
    result = subprocess.run(command, stderr=subprocess.PIPE, timeout=30, **kwargs)
    assert (result.returncode == 0) == success, (command, result.returncode, result.stderr)
    return result


def test_outputs(program, directory):
    source = directory / 'input'
    output = directory / 'output'
    duplicates = directory / 'duplicates'
    count = 300000
    for percent in (10, 50, 90):
        unique = count * percent // 100
        words = [('%08x' % i + 'x' * 120).encode() for i in range(unique)]
        with source.open('wb') as stream:
            for i in range(count):
                index = i if i < unique else (count - i - 1) % unique
                stream.write(words[index] + (b'\r\n' if i % 2 else b'\n'))
        expected = b''.join(word + b'\n' for word in words)
        expected_duplicates = Counter(words[(count - i - 1) % unique] for i in range(unique, count))
        for threads in sorted({1, int(THREADS)}):
            command = [program, str(source), '-o', str(output), '-D', str(duplicates),
                       '-m', '8M', '-t', str(threads)]
            run(command)
            assert output.read_bytes() == expected, (percent, threads)
            assert Counter(duplicates.read_bytes().splitlines()) == expected_duplicates
            assert not list(directory.rglob('duplicut_tmpfile.*'))
            print('[+] %d%% unique lines, at most %d threads' % (percent, threads))


def test_table_overflow(program, directory):
    source = directory / 'input'
    output = directory / 'output'
    duplicates = directory / 'duplicates'
    count = 100000
    unique = 90000
    with source.open('wb') as stream:
        for i in range(count):
            if i % 16 == 0:
                stream.write(b'\0' * 4095 + b'\n')
            index = i if i < unique else count - i - 1
            stream.write(('%08x\n' % index).encode())
    result = run([program, str(source), '-o', str(output), '-D', str(duplicates),
                  '-m', '4M', '-l', '8', '-t', THREADS])
    assert output.read_bytes() == b''.join(('%08x\n' % i).encode() for i in range(unique))
    assert Counter(duplicates.read_bytes().splitlines()) == Counter(
        ('%08x' % i).encode() for i in range(count - unique))
    assert b'10000 duplicates' in result.stderr
    assert b'6250 filtered lines' in result.stderr
    print('[+] short records, table saturation and single-count statistics')


def test_options(program, directory):
    content = (b'Alpha\r\nalpha\n\0hidden\nA\xff\nB\x00C\nB\x00C\na\nb\n'
               + b'z' * 4095 + b'\n' + b'w' * 512 + b'\n') * 4000 + b'last'
    source = directory / 'input'
    output = directory / 'output'
    duplicates = directory / 'duplicates'
    source.write_bytes(content)
    for flags in ((), ('-c',), ('-C',), ('-p',), ('-p', '-c'), ('-p', '-C'),
                  ('-l', '1'), ('-l', '4095')):
        size = int(flags[-1]) if '-l' in flags else 1024
        expected, expected_duplicates = dedupe(content, size, '-p' in flags,
                                              '-c' in flags, '-C' in flags)
        run([program, str(source), '-o', str(output), '-D', str(duplicates),
             '-m', '4M', '-t', THREADS, *flags])
        assert output.read_bytes() == expected, flags
        assert Counter(duplicates.read_bytes().splitlines()) == Counter(expected_duplicates.splitlines())
    expected = dedupe(content, 1024)[0]
    result = run([program, str(source), '-m', '4M'], stdout=subprocess.PIPE)
    assert result.stdout == expected
    with output.open('wb') as stream:
        stream.write(b'prefix')
    with output.open('ab') as stream:
        run([program, str(source), '-m', '4M'], stdout=stream)
    assert output.read_bytes() == b'prefix' + expected
    print('[+] filters, case, binary lines, final line, pipe and append')
    return content, expected


def test_faults(program, directory, root, content, expected):
    if sys.platform != 'linux':
        print('[*] Linux I/O fault injection skipped on this platform')
        return
    library = directory / 'faults.so'
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    subprocess.run([*compiler, '-shared', '-fPIC', '-Wall', '-Wextra',
                    str(root / 'test/scripts/partition-faults.c'), '-ldl',
                    '-o', str(library)], check=True)
    source_dir = directory / 'source'
    output_dir = directory / 'destination'
    source_dir.mkdir()
    output_dir.mkdir()
    source = source_dir / 'input'
    source.write_bytes(content)
    output = output_dir / 'output'
    log = directory / 'paths'
    command = [program, str(source), '-o', str(output), '-m', '4M', '-t', THREADS]
    environment = {**os.environ, 'LD_PRELOAD': str(library)}
    for fault in ('partial', 'ram', 'space'):
        run(command, env={**environment, 'DUPLICUT_TEST_FAULT': fault})
        assert output.read_bytes() == expected, fault
    for fault in ('ram', 'space'):
        result = run([program, '-m', '4M'], input=content, stdout=subprocess.PIPE,
                     success=False, env={**environment, 'DUPLICUT_TEST_FAULT': fault})
        assert b'cannot create temporary file' in result.stderr, result.stderr
    for fault, message in (('stream-read', b'copy_file() -> read()'),
                            ('stream-zero', b'copy_file() -> write()'),
                            ('write', b'cannot write partitions'),
                            ('zero', b'cannot write partitions'),
                            ('read', b'cannot read partitions')):
        result = run(command, success=False, env={**environment, 'DUPLICUT_TEST_FAULT': fault})
        assert result.returncode > 0 and message in result.stderr, (fault, result.stderr)
    duplicate_file = output_dir / 'duplicates'
    run([*command, '-D', str(duplicate_file)],
        env={**environment, 'DUPLICUT_TEST_FAULT': 'stream'})
    assert output.read_bytes() == expected
    assert Counter(duplicate_file.read_bytes().splitlines()) == Counter(dedupe(content, 1024)[1].splitlines())
    legacy = [program, str(source), '-o', str(output), '-D', str(duplicate_file),
              '-m', '64K', '-t', THREADS]
    run(legacy, env={**environment, 'DUPLICUT_TEST_FAULT': 'stream'})
    assert output.read_bytes() == expected
    assert Counter(duplicate_file.read_bytes().splitlines()) == Counter(dedupe(content, 1024)[1].splitlines())
    for deny, parent in ((None, source_dir), (str(source_dir) + '/', output_dir)):
        log.unlink(missing_ok=True)
        env = {**environment, 'DUPLICUT_TEST_FAULT': 'partial', 'DUPLICUT_TEST_LOG': str(log)}
        if deny:
            env['DUPLICUT_TEST_DENY'] = deny
        run(command, env=env, cwd='/tmp')
        assert output.read_bytes() == expected
        assert all(Path(path).parent == parent for path in log.read_text().splitlines())
    link_dir = directory / 'links'
    link_dir.mkdir()
    link = link_dir / 'input'
    link.symlink_to(source)
    log.unlink()
    env = {**environment, 'DUPLICUT_TEST_FAULT': 'partial', 'DUPLICUT_TEST_LOG': str(log)}
    run([program, str(link), '-o', str(output), '-m', '4M'], env=env, cwd='/tmp')
    assert all(Path(path).parent == source_dir for path in log.read_text().splitlines())
    log.unlink()
    run([program, '-o', str(output), '-m', '4M'], input=content, env=env, cwd='/tmp')
    assert output.read_bytes() == expected
    assert all(Path(path).parent == output_dir for path in log.read_text().splitlines())
    assert not list(directory.rglob('duplicut_tmpfile.*'))
    print('[+] partial I/O, EINTR, EOF, ENOSPC, volumes, paths and immediate unlink')
    word = b'abcdefghijklmno\n'
    with source.open('wb') as stream:
        block = (word + b'\0' * (4096 - len(word) - 1) + b'\n') * 256
        for unused in range(256):
            stream.write(block)
    production = [str(root / 'duplicut'), str(source), '-o', str(output),
                  '-m', '8M', '-l', '4095', '-t', THREADS]
    result = run(production, success=False,
                 env={**environment, 'DUPLICUT_TEST_FAULT': 'zero'})
    assert b'cannot write partitions' in result.stderr, result.stderr
    run(production)
    assert output.read_bytes() == word
    with source.open('r+b') as stream:
        stream.truncate(128 * 1024 * 1024)
    run(production, env={**environment, 'DUPLICUT_TEST_FAULT': 'zero'})
    assert output.read_bytes() == word
    print('[+] production dispatch at 128 and 256 MiB')
    with source.open('wb') as stream:
        block = (b'x' * 4095 + b'\n') * 256
        for unused in range(256):
            stream.write(block)
    log.unlink(missing_ok=True)
    run(production, env={**environment, 'DUPLICUT_TEST_FAULT': 'cached',
                         'DUPLICUT_TEST_LOG': str(log)})
    assert output.read_bytes() == b'x' * 4095 + b'\n'
    assert not log.exists()
    run(production, env={**environment, 'DUPLICUT_TEST_FAULT': 'uncached',
                         'DUPLICUT_TEST_LOG': str(log)})
    assert output.read_bytes() == b'x' * 4095 + b'\n'
    assert log.read_text().splitlines()
    print('[+] cached and uncached long-line selection')



def test_collisions(directory, root):
    include = directory / 'include'
    include.mkdir()
    (include / 'hash.h').write_text('#pragma once\n#include <stdint.h>\n'
                                  '#define HASH(line, size) ((uint64_t)0)\n')
    program = str(directory / 'collisions')
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    subprocess.run([*compiler, '-std=gnu99', '-O2', '-DNDEBUG',
                    '-DPARTITION_MIN_BYTES=0', '-I' + str(include),
                    '-I' + str(root / 'include'),
                    *map(str, sorted((root / 'src').glob('*.c'))),
                    '-lm', '-pthread', '-o', program], check=True)
    words = [('%08x' % i + 'x' * (4095 - 8)).encode() for i in range(2000)]
    source = directory / 'input'
    output = directory / 'output'
    duplicates = directory / 'duplicates'
    source.write_bytes(b''.join(word + b'\n' for word in words)
                       + b''.join(word + b'\n' for word in reversed(words)) * 3)
    run([program, str(source), '-o', str(output), '-D', str(duplicates),
         '-m', '4M', '-l', '4095', '-t', THREADS])
    assert output.read_bytes() == b''.join(word + b'\n' for word in words)
    assert Counter(duplicates.read_bytes().splitlines()) == Counter(words * 3)
    print('[+] forced hash collisions and bounded overflow passes')
    (include / 'hash.h').write_text('#pragma once\n#include <stdint.h>\n'
                                  '#define XXH_INLINE_ALL\n#include "xxhash.h"\n'
                                  '#define HASH(line, size) (XXH3_64bits(line, size) & ~UINT64_C(4095))\n')
    subprocess.run([*compiler, '-std=gnu99', '-O2', '-DNDEBUG',
                    '-DPARTITION_MIN_BYTES=0', '-I' + str(include),
                    '-I' + str(root / 'include'),
                    *map(str, sorted((root / 'src').glob('*.c'))),
                    '-lm', '-pthread', '-o', program], check=True)
    test_table_overflow(program, directory)


def main():
    root = Path.cwd().resolve()
    with tempfile.TemporaryDirectory(prefix='duplicut-partitions-', dir=root) as folder:
        directory = Path(folder)
        if len(sys.argv) > 1:
            program = str(Path(sys.argv[1]).resolve())
        else:
            program = str(directory / 'duplicut')
            compiler = shlex.split(os.environ.get('CC', 'cc'))
            subprocess.run([*compiler, '-std=gnu99', '-O2', '-DNDEBUG', '-Wall', '-Wextra',
                            '-DPARTITION_MIN_BYTES=0', '-I' + str(root / 'include'),
                            *map(str, sorted((root / 'src').glob('*.c'))),
                            '-lm', '-pthread', '-o', program], check=True)
        test_outputs(program, directory)
        content, expected = test_options(program, directory)
        if len(sys.argv) == 1:
            test_faults(program, directory, root, content, expected)
            test_collisions(directory, root)


if __name__ == '__main__':
    main()
