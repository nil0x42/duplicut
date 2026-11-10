import os
import shlex
import subprocess
from pathlib import Path


def build(root, directory, name, *flags):
    program = directory / name
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    subprocess.run([*compiler, '-std=gnu99', '-O2', '-Wall', '-Wextra',
                    *flags, '-I' + str(root / 'include'),
                    *map(str, sorted((root / 'src').glob('*.c'))),
                    '-lm', '-pthread', '-o', str(program)], check=True)
    return program


# Only fixed variants share builds; substitute headers always rebuild through build().
def build_debug(root, directory):
    shared = os.environ.get('DUPLICUT_TEST_BUILD_DIR')
    directory = Path(shared) if shared else directory
    program = directory / 'debug'
    if not shared or not program.is_file():
        build(root, directory, 'debug', '-DDEBUG=1')
    return program


def build_faults(root, directory):
    shared = os.environ.get('DUPLICUT_TEST_BUILD_DIR')
    directory = Path(shared) if shared else directory
    library = directory / 'faults.so'
    if not shared or not library.is_file():
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        subprocess.run([*compiler, '-shared', '-fPIC', '-Wall', '-Wextra',
                        str(root / 'test/scripts/partition-faults.c'), '-ldl',
                        '-o', str(library)], check=True)
    return library
