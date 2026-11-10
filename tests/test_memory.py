import os
import re
import subprocess
import sys
import time
from pathlib import Path

import pytest


THREADS = min(4, os.cpu_count() or 1)


def check(program, directory, size, length, memory, engine, threads=THREADS, output=None, env=None):
    source = directory / 'input'
    words = b''.join(('Ab' + '%0*x' % (length - 2, i) + '\n').encode()
                     for i in range(4096))
    block = words + b'\0ignored\n\nbad\xff\n'
    repeats = max(1, (size << 20) // len(block))
    with source.open('wb') as stream:
        for unused in range(repeats):
            stream.write(block)
    redirected = output is not None
    output = output or directory / 'output'
    log = directory / 'log'
    command = [str(program), str(source), '-cp', '-t', str(threads)]
    if memory:
        command += ['-m', memory]
    if not redirected:
        command += ['-o', str(output)]
    peak = 0
    start = time.monotonic()
    with log.open('wb') as stderr, output.open('wb') as stdout:
        process = subprocess.Popen(command, stderr=stderr,
                                   stdout=stdout if redirected else subprocess.DEVNULL, env=env)
        while process.poll() is None:
            if time.monotonic() - start > 30:
                process.kill()
                process.wait()
                raise AssertionError('working memory test timed out')
            if sys.platform == 'linux':
                try:
                    status = Path('/proc/%d/status' % process.pid).read_text()
                    match = re.search(r'VmHWM:\s+(\d+)', status)
                    if match:
                        peak = max(peak, int(match[1]))
                except FileNotFoundError:
                    pass
            time.sleep(.01)
    text = log.read_text()
    assert process.returncode == 0, text
    assert output.read_bytes() == words.lower()
    assert '%d duplicates and %d filtered lines' % ((repeats - 1) * 4096, repeats * 3) in text
    assert 'dedupe engine: ' + engine in text, text
    if size >= 32 and sys.platform == 'linux':
        assert 0 < peak < 16 * 1024, (peak, text)
    print('[+] %s, %s, %d KiB peak RSS' % (engine, memory or 'available memory', peak))
    return text


def test_partition_memory(debug_program, directory):
    program = debug_program
    check(program, directory, 32, 128, '4M', 'disk-partitions')


def test_bitmap_budget(debug_program, directory):
    program = debug_program
    text = check(program, directory, 96, 8, '4M', 'disk-partitions')
    assert 'large bitmap estimate:' in text


def test_large_bitmap_fallback(debug_program, directory):
    program = debug_program
    text = check(program, directory, 128, 8, '4M', 'mmap-chunks')
    assert 'insufficient private storage for a large bitmap' in text


def test_bounded_worker_storage(debug_program, directory):
    program = debug_program
    if THREADS < 2:
        pytest.skip('multiple CPUs required')
    text = check(program, directory, 2, 8, None, 'disk-partitions')
    per_worker = int(re.search(r'(\d+) bytes per worker', text)[1])
    assert per_worker < 1024 * 1024


def test_serial_memory(debug_program, directory):
    program = debug_program
    check(program, directory, 2, 8, None, 'mmap-chunks', threads=1)


def test_ram_backed_output(debug_program, faults, directory, system_directory):
    program = debug_program
    mounts = [line.split() for line in Path('/proc/mounts').read_text().splitlines()]
    if not any(fields[1] == '/tmp' and fields[2] in ('tmpfs', 'ramfs') for fields in mounts):
        pytest.skip('/tmp is not RAM-backed')
    text = check(program, directory, 32, 128, '4M', 'disk-partitions',
                 output=system_directory / 'output')
    assert 'RAM-backed output: using a disk-backed working file' in text
    assert 'g_file->name:         temporary file' in text
    print('[+] RAM-backed stdout receives only the final result')
    library = faults
    env = {**os.environ, 'LD_PRELOAD': str(library),
           'DUPLICUT_TEST_AVAILABLE': '8192'}
    text = check(program, directory, 32, 128, None, 'mmap-chunks',
                 output=system_directory / 'output', env=env)
    assert 'RAM-backed output: using a disk-backed working file' in text
    print('[+] available memory also bounds RAM-backed working files')
