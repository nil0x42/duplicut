import os
import subprocess


def run(program, *args, **kwargs):
    return subprocess.run([program, *args], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=5, **kwargs)


def test_help_and_version(program):
    result = run(program, '--help')
    assert result.returncode == 0 and b'Usage:' in result.stdout
    result = run(program, '--version')
    assert result.returncode == 0 and b'duplicut' in result.stdout and b'github' in result.stdout


def test_interactive_help(program):
    master, slave = os.openpty()
    try:
        result = run(program, stdin=slave)
        assert result.returncode == 0 and b'Usage:' in result.stdout
    finally:
        os.close(slave)
        os.close(master)


def test_optional_output_and_empty_stdin(program, system_directory):
    result = run(program, input=b'')
    assert result.returncode == 0 and not result.stdout
    output = system_directory / 'output'
    with open(os.devnull, 'rb') as stdin, output.open('wb') as stdout:
        result = subprocess.run([program], stdin=stdin, stdout=stdout,
                                stderr=subprocess.PIPE, timeout=5)
    assert result.returncode == 0 and not output.read_bytes(), result.stderr


def test_invalid_option(program):
    result = run(program, '--unexistent-argument')
    assert result.returncode != 0 and b'unrecognized' in result.stderr


def test_line_length_limits(program, tmp_path):
    output = tmp_path / 'output'
    result = run(program, '/etc/passwd', '-o', str(output), '-l', '4095')
    assert result.returncode == 0, result.stderr
    result = run(program, '/etc/passwd', '-o', str(output), '-l', '4096')
    assert result.returncode != 0 and b'max value is 4095' in result.stderr


def test_case_options(program, tmp_path):
    output = tmp_path / 'output'
    for flag in ('-c', '-C'):
        result = run(program, '/etc/passwd', '-o', str(output), flag)
        assert result.returncode == 0, result.stderr
    result = run(program, '/etc/passwd', '-o', str(output), '-c', '-C')
    assert result.returncode != 0
    assert b"cannot use '--lowercase' and '--uppercase' together" in result.stderr


def test_optional_duplicate_output(program, tmp_path):
    output, duplicates = tmp_path / 'output', tmp_path / 'duplicates'
    for flags in ((), ('-D', str(duplicates))):
        result = run(program, '/etc/passwd', '-o', str(output), *flags)
        assert result.returncode == 0, result.stderr
    result = run(program, '/etc/passwd', '-o', str(output), '-D', '/')
    assert result.returncode != 0
