import os
import subprocess
from collections import Counter
from pathlib import Path

import pytest

from .reference import dedupe, line_count
from .build import build


THREADS = str(min(4, os.cpu_count() or 1))


def run(command, success=True, **kwargs):
    result = subprocess.run(command, stderr=subprocess.PIPE, timeout=30, **kwargs)
    assert (result.returncode == 0) == success, (command, result.returncode, result.stderr)
    return result


@pytest.fixture(scope='module')
def option_content():
    content = (b'Alpha\r\nalpha\n\0hidden\nA\xff\nB\x00C\nB\x00C\na\nb\n'
               + b'z' * 4095 + b'\n' + b'w' * 512 + b'\n') * 4000 + b'last'
    return content


def test_order_and_unique_ratios(partition_program, directory):
    program = partition_program
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


def check_table_overflow(program, directory):
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


def test_filters_and_streams(partition_program, directory, option_content):
    program = partition_program
    content = option_content
    source = directory / 'input'
    output = directory / 'output'
    duplicates = directory / 'duplicates'
    source.write_bytes(content)
    for flags in ((), ('-c',), ('-C',), ('-p',), ('-p', '-c'), ('-p', '-C'),
                  ('-l', '1'), ('-l', '4095')):
        size = int(flags[-1]) if '-l' in flags else 1024
        expected, expected_duplicates = dedupe(content, size, '-p' in flags,
                                              '-c' in flags, '-C' in flags)
        result = run([program, str(source), '-o', str(output), '-D', str(duplicates),
             '-m', '4M', '-t', THREADS, *flags])
        assert output.read_bytes() == expected, flags
        assert Counter(duplicates.read_bytes().splitlines()) == Counter(expected_duplicates.splitlines())
        counts = b'%d duplicates and %d filtered lines' % (
            line_count(expected_duplicates),
            line_count(content) - line_count(expected) - line_count(expected_duplicates))
        assert counts in result.stderr, (flags, result.stderr)
    expected = dedupe(content, 1024)[0]
    result = run([program, str(source), '-m', '4M'], stdout=subprocess.PIPE)
    assert result.stdout == expected
    with output.open('wb') as stream:
        stream.write(b'prefix')
    with output.open('ab') as stream:
        run([program, str(source), '-m', '4M'], stdout=stream)
    assert output.read_bytes() == b'prefix' + expected


def test_record_headers(records_program):
    run([records_program])


def test_io_faults_and_temporary_paths(partition_program, records_program, faults, directory, option_content):
    program = partition_program
    records = records_program
    content = option_content
    expected = dedupe(content, 1024)[0]
    library = faults
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
    run([records], env={**environment, 'DUPLICUT_TEST_FAULT': 'partial'})
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
                            ('boundary-read', b'cannot read chunk boundary'),
                            ('read', b'cannot read partitions')):
        fault_command = command
        if fault == 'boundary-read':
            fault_command = [program, str(source), '-o', str(output),
                             '-m', '64K', '-t', THREADS, '-e', 'mmap-chunks']
        result = run(fault_command, success=False,
                     env={**environment, 'DUPLICUT_TEST_FAULT': fault})
        assert result.returncode > 0 and message in result.stderr, (fault, result.stderr)
    duplicate_file = output_dir / 'duplicates'
    run([*command, '-D', str(duplicate_file)],
        env={**environment, 'DUPLICUT_TEST_FAULT': 'stream'})
    assert output.read_bytes() == expected
    assert Counter(duplicate_file.read_bytes().splitlines()) == Counter(dedupe(content, 1024)[1].splitlines())
    chunks = [program, str(source), '-o', str(output), '-D', str(duplicate_file),
              '-m', '64K', '-t', THREADS]
    run(chunks, env={**environment, 'DUPLICUT_TEST_FAULT': 'stream'})
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


def test_production_engine_selection(program, faults, directory):
    source, output = directory / 'input', directory / 'output'
    log = directory / 'paths'
    environment = {**os.environ, 'LD_PRELOAD': faults}
    word = b'abcdefghijklmno\n'
    with source.open('wb') as stream:
        block = (word + b'\0' * (4096 - len(word) - 1) + b'\n') * 256
        for unused in range(256):
            stream.write(block)
    production = [program, str(source), '-o', str(output),
                  '-m', '8M', '-l', '4095', '-t', THREADS]
    result = run(production, success=False,
                 env={**environment, 'DUPLICUT_TEST_FAULT': 'zero'})
    assert b'cannot write partitions' in result.stderr, result.stderr
    run(production)
    assert output.read_bytes() == word
    with source.open('r+b') as stream:
        stream.truncate(128 * 1024 * 1024)
    result = run(production, success=False,
                 env={**environment, 'DUPLICUT_TEST_FAULT': 'zero'})
    assert b'cannot write partitions' in result.stderr, result.stderr
    with source.open('wb') as stream:
        block = (b'x' * 4095 + b'\n') * 256
        for unused in range(256):
            stream.write(block)
    log.unlink(missing_ok=True)
    run([*production, '-m', '16M'], env={**environment, 'DUPLICUT_TEST_FAULT': 'cached',
                         'DUPLICUT_TEST_LOG': str(log)})
    assert output.read_bytes() == b'x' * 4095 + b'\n'
    assert log.exists() == (int(THREADS) < 4)
    run([*production, '-m', '16M'], env={**environment, 'DUPLICUT_TEST_FAULT': 'uncached',
                         'DUPLICUT_TEST_LOG': str(log)})
    assert output.read_bytes() == b'x' * 4095 + b'\n'
    assert log.read_text().splitlines()
    with source.open('r+b') as stream:
        stream.write(word * (65536 // len(word)))
    log.unlink()
    run([*production, '-m', '64M'], env={**environment, 'DUPLICUT_TEST_FAULT': 'cached',
                         'DUPLICUT_TEST_LOG': str(log)})
    assert output.read_bytes() == word + b'x' * 4095 + b'\n'
    assert log.exists() == (int(THREADS) <= 2)
    source.write_bytes((word + b'\0' * (4096 - len(word) - 1) + b'\n') * 2048)
    for memory, partitioned in (('8M', int(THREADS) > 1), ('1M', False),
                                ('768K', True), ('512K', True), ('256K', False)):
        result = run([*production, '-m', memory], success=not partitioned,
                     env={**environment, 'DUPLICUT_TEST_FAULT': 'zero'})
        if partitioned:
            assert b'cannot write partitions' in result.stderr, (memory, result.stderr)
        else:
            assert output.read_bytes() == word
    run([*production, '-m', '512K'])
    assert output.read_bytes() == word
    source.write_bytes((b'x' * 4095 + b'\n') * 2048)
    for available, partitioned in ((4096, True), (8256, True), (8448, False), (9216, False),
                                    (4 * 1024 * 1024, False), (128 * 1024 * 1024, False)):
        partitioned = partitioned or int(THREADS) <= 2
        env = {**environment, 'DUPLICUT_TEST_FAULT': 'zero',
               'DUPLICUT_TEST_AVAILABLE': str(available)}
        result = run([*production, '-m', '768K'], success=not partitioned, env=env)
        if partitioned:
            assert b'cannot write partitions' in result.stderr, (available, result.stderr)
        else:
            assert output.read_bytes() == b'x' * 4095 + b'\n'
    env = {**environment, 'DUPLICUT_TEST_FAULT': 'zero',
           'DUPLICUT_TEST_AVAILABLE': str(128 * 1024 * 1024)}
    result = run([*production, '-m', '512K'], success=int(THREADS) >= 4, env=env)
    if int(THREADS) < 4:
        assert b'cannot write partitions' in result.stderr, result.stderr
    else:
        assert output.read_bytes() == b'x' * 4095 + b'\n'
    with source.open('ab') as stream:
        stream.write((b'x' * 4095 + b'\n') * 6144)
    result = run([*production, '-m', '512K'], success=False, env=env)
    assert b'cannot write partitions' in result.stderr, result.stderr


def test_collision_overflow(directory, root):
    include = directory / 'include'
    include.mkdir()
    (include / 'hash.h').write_text('#pragma once\n#include <stdint.h>\n'
                                  '#define HASH(line, size) ((uint64_t)0)\n')
    program = str(build(root, directory, 'collisions', '-DNDEBUG',
                        '-DPARTITION_MIN_CHUNKS=0', '-I' + str(include)))
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
    (include / 'hash.h').write_text('#pragma once\n#include <stdint.h>\n'
                                  '#define XXH_INLINE_ALL\n#include "xxhash.h"\n'
                                  '#define HASH(line, size) (XXH3_64bits(line, size) & ~UINT64_C(4095))\n')
    build(root, directory, 'collisions', '-DNDEBUG',
          '-DPARTITION_MIN_CHUNKS=0', '-I' + str(include))
    check_table_overflow(program, directory)
