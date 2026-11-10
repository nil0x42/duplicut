import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

import pytest

from .build import build, build_faults


@pytest.fixture(scope='session')
def root():
    return Path(__file__).resolve().parent.parent


@pytest.fixture(scope='session')
def build_dir(root):
    with tempfile.TemporaryDirectory(prefix='duplicut-tests-build-', dir=root) as folder:
        yield Path(folder)


@pytest.fixture
def directory(root):
    # Partition and memory tests need disk storage rather than a RAM-backed /tmp.
    with tempfile.TemporaryDirectory(prefix='duplicut-tests-', dir=root) as folder:
        yield Path(folder)


@pytest.fixture
def system_directory():
    with tempfile.TemporaryDirectory(prefix='duplicut-tests-', dir='/tmp') as folder:
        yield Path(folder)


@pytest.fixture(scope='session')
def program(root):
    subprocess.run(['make', 'coverage'], cwd=root,
                   env={**os.environ, 'FLAGS': ''}, check=True)
    program = root / 'duplicut'
    result = subprocess.run(['file', '-b', str(program)], stdout=subprocess.PIPE, check=True)
    assert b'64' in result.stdout, result.stdout
    return str(program)


@pytest.fixture(scope='session')
def debug_program(root, build_dir):
    return str(build(root, build_dir, 'debug', '-DDEBUG=1'))


@pytest.fixture(scope='session')
def partition_program(root, build_dir):
    return str(build(root, build_dir, 'partitions', '-DNDEBUG', '-DPARTITION_MIN_CHUNKS=0'))


@pytest.fixture(scope='session')
def partition_debug_program(root, build_dir):
    return str(build(root, build_dir, 'partitions-debug', '-DDEBUG=1', '-DPARTITION_MIN_CHUNKS=0'))


@pytest.fixture(scope='session')
def release_program(root, build_dir):
    return str(build(root, build_dir, 'release', '-DNDEBUG', '-Werror'))


@pytest.fixture(scope='session')
def fallback_program(root, build_dir):
    return str(build(root, build_dir, 'fallback', '-DNDEBUG', '-Werror',
                     '-DPARTITION_MIN_CHUNKS=0'))


@pytest.fixture(scope='session')
def faults(root, build_dir):
    if sys.platform != 'linux':
        pytest.skip('Linux I/O fault injection')
    return str(build_faults(root, build_dir))


@pytest.fixture(scope='session')
def records_program(root, build_dir):
    program = build_dir / 'records'
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    sources = [str(path) for path in sorted((root / 'src').glob('*.c'))
               if path.name not in ('main.c', 'partition.c')]
    subprocess.run([*compiler, '-std=gnu99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(root / 'include'), str(root / 'tests/fixtures/records.c'),
                    *sources, '-lm', '-pthread', '-o', str(program)], check=True)
    return str(program)


@pytest.fixture(scope='session')
def status_program(root, build_dir):
    program = build_dir / 'status'
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    subprocess.run([*compiler, '-std=gnu99', '-O2', '-DNDEBUG', '-Wall', '-Wextra',
                    '-I' + str(root / 'include'), str(root / 'tests/fixtures/status.c'),
                    str(root / 'src/status.c'), str(root / 'src/cprintferr.c'),
                    '-pthread', '-lm', '-o', str(program)], check=True)
    return str(program)
