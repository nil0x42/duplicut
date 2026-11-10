import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


def make(directory, target=None, success=True):
    command = ['make'] + ([target] if target else [])
    result = subprocess.run(command, cwd=directory, env={**os.environ, 'FLAGS': ''},
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    assert (result.returncode == 0) == success, result.stdout


def symbols(program):
    return subprocess.run(['nm', '--debug-syms', str(program)],
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10).stdout


def test_make_targets(root, directory):
    shutil.copy2(root / 'Makefile', directory / 'Makefile')
    for name in ('src', 'include'):
        shutil.copytree(root / name, directory / name)
    program = directory / 'duplicut'
    make(directory, 'foobar', success=False)
    make(directory, 'debug')
    assert (directory / 'objects').is_dir()
    if shutil.which('ctags'):
        assert (directory / 'tags').is_file()
    assert program.is_file() and os.access(program, os.X_OK)
    subprocess.run([str(program), '--help'], stdout=subprocess.PIPE, cwd=directory, check=True)
    if sys.platform == 'linux':
        assert re.search(rb'\s+t\s+_dlog$', symbols(program), re.MULTILINE)
        assert not (directory / 'gmon.out').exists()

    make(directory)
    make(directory, 'clean')
    assert program.is_file() and os.access(program, os.X_OK)
    for name in ('objects', 'tags', 'gmon.out'):
        assert not (directory / name).exists()
    make(directory, 'distclean')
    assert not program.exists()
    for name in ('objects', 'tags', 'gmon.out'):
        assert not (directory / name).exists()

    if sys.platform == 'linux':
        make(directory, 'distclean')
        make(directory, 'profile')
        assert not (directory / 'gmon.out').exists()
        subprocess.run([str(program), '--help'], stdout=subprocess.PIPE, cwd=directory, check=True)
        assert (directory / 'gmon.out').exists()
    make(directory)
    subprocess.run([str(program), '--help'], stdout=subprocess.PIPE, cwd=directory, check=True)
    if sys.platform == 'linux':
        assert not (directory / 'gmon.out').exists()
        assert not symbols(program).strip()
        assert Path('/proc/meminfo').is_file()


def test_coverage_build(program, root):
    if sys.platform == 'linux':
        assert not (root / 'gmon.out').exists()
        assert b'_gcov_' in symbols(program)
