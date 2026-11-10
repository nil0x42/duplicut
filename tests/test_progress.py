import os
import re
import subprocess

from .reference import dedupe
from .terminal import terminal


ANSI = re.compile(rb'\x1b\[[0-?]*[ -/]*[@-~]')


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


def test_phase_history_and_widths(release_program):
    program = release_program
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


def test_empty_and_filtered_phases(release_program):
    program = release_program
    for content in (b'', b'\0ignored\n', b'x' * 5000):
        result = subprocess.run([str(program), '-cp', '-e', 'disk-partitions', '-m', '8M'],
                                input=content, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=10)
        assert result.returncode == 0 and not result.stdout, result.stderr
        text, rows = completed(result.stderr)
        assert len(rows) == (2 if not content else 5), rows
        assert b'0 duplicates' in result.stderr


def test_direct_output(release_program, directory):
    program = release_program
    source, output = directory / 'input', directory / 'output'
    source.write_bytes(b'Alpha\nAlpha\n')
    result = subprocess.run([str(program), str(source), '-o', str(output)],
                            stderr=subprocess.PIPE, timeout=10)
    assert result.returncode == 0 and output.read_bytes() == b'Alpha\n'
    assert 'finalizing output' in completed(result.stderr)[1][-1]


def test_closed_diagnostics(release_program, directory):
    program = release_program
    source, output = directory / 'input', directory / 'output'
    source.write_bytes(b'Alpha\nAlpha\n')
    for engine in ('mmap-chunks', 'disk-partitions'):
        result = subprocess.run([str(program), str(source), '-o', str(output),
                                 '-e', engine, '-m', '8M'], stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                preexec_fn=close_diagnostics, timeout=10)
        assert result.returncode == 0 and output.read_bytes() == b'Alpha\n'
        assert not result.stdout and not result.stderr


def test_fatal_error_display(release_program):
    program = release_program
    command = [str(program), '-e', 'disk-partitions', '-m', '64K']
    code, unused, log = terminal(command, b'a\na\n')
    text = ANSI.sub(b'', log).decode()
    assert code != 0 and '\nerror:' in text.replace('\r', '')
    assert '100.0%' not in text and 'successfully' not in text
    assert not any('preparing deduplication' in line and 'done: took' in line
                   for line in text.splitlines())


def test_long_paths(release_program, directory):
    program = release_program
    output = directory / 'output'
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


def test_fallback_progress(fallback_program, faults, directory):
    program = fallback_program
    source, output = directory / 'input', directory / 'output'
    source.write_bytes(b'x\nx\n')
    library = faults
    env = {**os.environ, 'LD_PRELOAD': str(library), 'DUPLICUT_TEST_FAULT': 'space'}
    result = subprocess.run([str(program), str(source), '-o', str(output), '-m', '8M'],
                            env=env, stderr=subprocess.PIPE, timeout=10)
    assert result.returncode == 0 and output.read_bytes() == b'x\n', result.stderr
    text, rows = completed(result.stderr)
    assert 'fallback:' in text and 'checking disk-partitions' in text
    assert 'mmap-chunks: chunk' in text and len(rows) == 6, rows
    assert 'writing partitions' not in text
