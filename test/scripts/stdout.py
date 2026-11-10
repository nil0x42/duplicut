#!/usr/bin/env python3

import errno
import os
import select
import signal
import subprocess
import sys
import time
import tty
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from reference import dedupe


def run(command, success=True, **kwargs):
    result = subprocess.run(command, stderr=subprocess.PIPE, timeout=5, **kwargs)
    assert (result.returncode == 0) == success, (command, result.returncode, result.stderr)
    assert not list(Path('.').glob('duplicut_tmpfile.*'))
    return result


def test_outputs(program, options):
    contents = (b'', b'alpha\nbeta\nalpha\n', b'alpha\nbeta',
                b'Alpha\r\nalpha\n\0hidden\nA\xff\nB\x00C\nB\x00C\n',
                b'alpha\nbeta\nalpha\n' * 16384)
    flags = ((), ('-c',), ('-C',), ('-p',), ('-p', '-c'), ('-p', '-C'), ('-l', '1'))
    for content in contents:
        Path('input').write_bytes(content)
        for args in flags:
            size = 1 if '-l' in args else 1024
            expected, duplicates = dedupe(content, size, '-p' in args,
                                          '-c' in args, '-C' in args)
            command = [program, '-t', '1', '-m', '64K', '-D', 'duplicates', *options, *args]
            for mode in ('file', 'append', 'pipe'):
                if mode == 'pipe':
                    result = run(command, input=content, stdout=subprocess.PIPE)
                    actual = result.stdout
                else:
                    Path('output').write_bytes(b'prefix' if mode == 'append' else b'')
                    with open('output', 'ab' if mode == 'append' else 'wb') as output:
                        run([*command, 'input'], stdout=output)
                    actual = Path('output').read_bytes()
                assert actual == (b'prefix' if mode == 'append' else b'') + expected, (args, mode)
                actual_duplicates = Path('duplicates').read_bytes().split(b'\n')
                assert sorted(actual_duplicates) == sorted(duplicates.split(b'\n'))


def test_descriptors(command, expected):
    # inherited O_RDWR, nonzero positions and existing tails
    for content, offset, append in ((b'', 0, False), (b'', 7, False),
                                    (b'header:old output with a tail', 7, False),
                                    (b'old output with a tail', 0, False),
                                    (b'prefix', 0, True)):
        Path('output').write_bytes(content)
        fd = os.open('output', os.O_RDWR | (os.O_APPEND if append else 0))
        try:
            os.lseek(fd, offset, os.SEEK_SET)
            run(command, stdout=fd)
            position = (len(content) if append else offset) + len(expected)
            assert os.lseek(fd, 0, os.SEEK_CUR) == position
        finally:
            os.close(fd)
        if append:
            actual = content + expected
        else:
            prefix = content[:offset].ljust(offset, b'\0')
            actual = prefix + expected + content[offset + len(expected):]
        assert Path('output').read_bytes() == actual

    # read/write stdout aliases are rejected without truncation
    original = Path('input').read_bytes()
    for path, args in (('input', []), ('output', ['-D', 'output'])):
        if path == 'output':
            Path(path).write_bytes(b'keep output')
        saved = Path(path).read_bytes()
        with open(path, 'r+b') as output:
            result = run([*command, *args], success=False, stdout=output)
        assert b'must be different' in result.stderr
        assert Path(path).read_bytes() == saved
        assert Path('input').read_bytes() == original

    # reopening a write-only destination must fall back to a temporary file
    Path('output').write_bytes(b'')
    Path('output').chmod(0o200)
    restricted = True
    try:
        try:
            fd = os.open('output', os.O_RDWR)
        except PermissionError:
            with open('output', 'wb') as output:
                run(command, stdout=output)
        else:
            os.close(fd)
            restricted = False
            print('[*] Skipping write-only permissions test: read access is not restricted')
    finally:
        Path('output').chmod(0o600)
    if restricted:
        assert Path('output').read_bytes() == expected


def test_streams(program, options, expected):
    command = [program, '-t', '1', 'input', *options]

    # FIFO with a bounded reader
    os.mkfifo('fifo')
    try:
        with ThreadPoolExecutor(max_workers=1) as pool:
            reader = pool.submit(subprocess.run, ['cat', 'fifo'], stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, timeout=5)
            deadline = time.monotonic() + 5
            while True:
                try:
                    output = os.open('fifo', os.O_WRONLY | os.O_NONBLOCK)
                    break
                except OSError as error:
                    if error.errno != errno.ENXIO:
                        raise
                    assert not reader.done(), reader.result()
                    assert time.monotonic() < deadline, 'FIFO open timeout'
                    time.sleep(0.01)
            try:
                os.set_blocking(output, True)
                run(command, stdout=output)
            finally:
                os.close(output)
            result = reader.result(timeout=5)
            assert result.returncode == 0, result.stderr
            assert result.stdout == expected
    finally:
        Path('fifo').unlink()

    # raw terminal output must contain only the deduped bytes
    master, slave = os.openpty()
    try:
        tty.setraw(slave)
        run(command, stdout=slave)
        actual = b''
        deadline = time.monotonic() + 5
        while len(actual) < len(expected):
            ready, _, _ = select.select([master], [], [], max(0, deadline - time.monotonic()))
            assert ready, 'terminal read timeout'
            actual += os.read(master, 4096)
        assert actual == expected
        assert not select.select([master], [], [], 0)[0], 'extra terminal output'
    finally:
        os.close(slave)
        os.close(master)

    # SIGPIPE must not leave a named temporary file
    reader, writer = os.pipe()
    os.close(reader)
    try:
        result = run(command, success=False, stdout=writer)
        assert result.returncode == -signal.SIGPIPE, result.stderr
    finally:
        os.close(writer)

    # interrupt a process blocked while copying input into its temporary file
    with subprocess.Popen([program, '-t', '1', *options], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0) as process:
        try:
            os.set_blocking(process.stdin.fileno(), False)
            content = b'alpha\n' * 524288
            offset = 0
            deadline = time.monotonic() + 5
            while offset < len(content):
                timeout = max(0, deadline - time.monotonic())
                _, ready, _ = select.select([], [process.stdin], [], timeout)
                assert ready, 'stdin write timeout'
                try:
                    offset += os.write(process.stdin.fileno(), content[offset:])
                except BlockingIOError:
                    pass
            assert process.poll() is None
            assert not list(Path('.').glob('duplicut_tmpfile.*'))
        finally:
            process.terminate()
            process.communicate(timeout=5)
        assert process.returncode == -signal.SIGTERM


def main():
    program = sys.argv[1]
    for options in ([], ['-o', '/dev/stdout']):
        test_outputs(program, options)
        Path('input').write_bytes(b'alpha\nbeta\nalpha\n')
        command = [program, '-t', '1', 'input', *options]
        expected = b'alpha\nbeta\n'
        test_descriptors(command, expected)
        test_streams(program, options, expected)
    print('[+] stdout regression tests succeeded')
    return 0


if __name__ == '__main__':
    sys.exit(main())
