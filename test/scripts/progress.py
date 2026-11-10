#!/usr/bin/env python3

import errno
import fcntl
import os
import re
import struct
import subprocess
import sys
import tempfile
import termios
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from reference import dedupe
from build import build, build_faults


ANSI = re.compile(rb'\x1b\[[0-?]*[ -/]*[@-~]')


def terminal_reader(fd):
    chunks = []
    while True:
        try:
            block = os.read(fd, 65536)
        except OSError as error:
            if error.errno == errno.EIO:
                break
            raise
        if not block:
            break
        chunks.append(block)
    return b''.join(chunks)


def terminal_session(command):
    os.setsid()
    fcntl.ioctl(2, termios.TIOCSCTTY, 0)
    before = termios.tcgetattr(2)
    result = subprocess.run(command, timeout=10)
    # Keep the controlling session alive until its settings have been checked.
    after = termios.tcgetattr(2)
    # macOS sets PENDIN internally when canonical input is restored.
    before[3] &= ~termios.PENDIN
    after[3] &= ~termios.PENDIN
    assert after == before, ('terminal settings were not restored', before, after)
    sys.exit(result.returncode if result.returncode >= 0 else 128 - result.returncode)


def terminal(command, content, columns=120, delay=0):
    master, slave = os.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 24, columns, 0, 0))
    try:
        process = subprocess.Popen([sys.executable, __file__, '--terminal', *command],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=slave)
        with ThreadPoolExecutor(max_workers=1) as pool:
            reader = pool.submit(terminal_reader, master)
            try:
                if delay:
                    time.sleep(delay)
                output, unused = process.communicate(content, timeout=12)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
                os.close(slave)
            log = reader.result(timeout=5)
        return process.returncode, output, log
    finally:
        os.close(master)


def completed(log):
    text = ANSI.sub(b'', log).decode()
    rows = [line for line in text.splitlines() if line.startswith('time:') and 'done: took' in line]
    assert all('ETA:' not in row for row in rows), rows
    groups = [int(re.search(r'\[(\d)/5\]', row)[1]) for row in rows]
    assert groups == sorted(groups), rows
    percents = [float(re.search(r'([\d.]+)%', row)[1]) for row in rows]
    assert percents == sorted(percents), rows
    assert percents[-1] == 100 and 100 not in percents[:-1], rows
    return text, rows


def close_diagnostics():
    os.close(0)
    os.close(2)


def main():
    root = Path.cwd().resolve()
    with tempfile.TemporaryDirectory(prefix='duplicut-progress-', dir=root) as folder:
        directory = Path(folder)
        program = build(root, directory, 'release', '-DNDEBUG', '-Werror')
        content = (b'Alpha\r\nalpha\nBeta\n\0hidden\nA\xff\nlast\n' * 400) + b'final'
        expected = dedupe(content, 1024, True, True)[0]
        for engine, phases in (('mmap-chunks', 5), ('disk-partitions', 8)):
            command = [str(program), '-cp', '-m', '8M', '-t', '1', '-e', engine]
            result = subprocess.run(command, input=content, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=10)
            assert result.returncode == 0 and result.stdout == expected, result.stderr
            assert b'\x1b' not in result.stderr and b'\r' not in result.stderr
            text, rows = completed(result.stderr)
            assert len(rows) == phases, rows
            assert 'writing output' in rows[-1]
            assert ('task ' in text) == (engine == 'mmap-chunks')
            for width in (20, 80, 160):
                code, output, log = terminal(command, content, width,
                                             delay=.35 if width == 80 else 0)
                assert code == 0 and output == expected, log
                text, rows = completed(log)
                assert len(rows) == phases, rows
                assert b'\x1b[32mdone' in log and b'\x1b[2K' in log, log
                if width >= 80:
                    assert re.search(r'ETA:\s+--', text), text
                frames = ANSI.sub(b'', log).decode().split('\r')
                for frame in frames:
                    line = frame.split('\n')[0]
                    if line.startswith('time:') and 'done: took' not in line:
                        assert len(line) < width, (width, line)
            print('[+] %s: fixed history, live display, widths, colors and terminal restoration' % engine)
        for content in (b'', b'\0ignored\n', b'x' * 5000):
            result = subprocess.run([str(program), '-cp', '-e', 'disk-partitions', '-m', '8M'],
                                    input=content, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=10)
            assert result.returncode == 0 and not result.stdout, result.stderr
            text, rows = completed(result.stderr)
            assert len(rows) == (2 if not content else 5), rows
            assert b'0 duplicates' in result.stderr
        print('[+] empty input and skipped partition phases')
        source, output = directory / 'input', directory / 'output'
        source.write_bytes(b'Alpha\nAlpha\n')
        result = subprocess.run([str(program), str(source), '-o', str(output)],
                                stderr=subprocess.PIPE, timeout=10)
        assert result.returncode == 0 and output.read_bytes() == b'Alpha\n'
        assert 'finalizing output' in completed(result.stderr)[1][-1]
        for engine in ('mmap-chunks', 'disk-partitions'):
            result = subprocess.run([str(program), str(source), '-o', str(output),
                                     '-e', engine, '-m', '8M'], stdin=subprocess.DEVNULL,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    preexec_fn=close_diagnostics, timeout=10)
            assert result.returncode == 0 and output.read_bytes() == b'Alpha\n'
            assert not result.stdout and not result.stderr
        print('[+] closed stderr cannot write diagnostics into reused file descriptors')
        command = [str(program), '-e', 'disk-partitions', '-m', '64K']
        code, unused, log = terminal(command, b'a\na\n')
        text = ANSI.sub(b'', log).decode()
        assert code != 0 and '\nerror:' in text.replace('\r', '')
        assert '100.0%' not in text and 'successfully' not in text
        assert not any('preparing deduplication' in line and 'done: took' in line
                       for line in text.splitlines())
        print('[+] direct output and fatal errors without a false completion')
        parent = directory
        limit = os.pathconf(directory, 'PC_PATH_MAX')
        while len(os.fsencode(parent)) < min(1300, limit - 250):
            parent /= 'wordlist path ' + 'x' * 80
        parent.mkdir(parents=True)
        source = parent / 'input'
        source.write_bytes(b'x\nx\n')
        result = subprocess.run([str(program), str(source), '-o', str(output)],
                                stderr=subprocess.PIPE, timeout=10)
        assert result.returncode == 0 and os.fsencode(source) in result.stderr
        print('[+] complete long paths in the technical summary')
        if sys.platform == 'linux':
            program = build(root, directory, 'candidate', '-DNDEBUG', '-Werror',
                            '-DPARTITION_MIN_CHUNKS=0')
            library = build_faults(root, directory)
            env = {**os.environ, 'LD_PRELOAD': str(library), 'DUPLICUT_TEST_FAULT': 'space'}
            result = subprocess.run([str(program), str(source), '-o', str(output), '-m', '8M'],
                                    env=env, stderr=subprocess.PIPE, timeout=10)
            assert result.returncode == 0 and output.read_bytes() == b'x\n', result.stderr
            text, rows = completed(result.stderr)
            assert 'fallback:' in text and 'checking disk-partitions' in text
            assert 'mmap-chunks: chunk' in text and len(rows) == 6, rows
            assert 'writing partitions' not in text
            print('[+] explicit engine fallback with nondecreasing progress')


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--terminal':
        terminal_session(sys.argv[2:])
    else:
        main()
