import os
import shlex
import subprocess


def build(root, directory, name, *flags):
    program = directory / name
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    subprocess.run([*compiler, '-std=gnu99', '-O2', '-Wall', '-Wextra',
                    *flags, '-I' + str(root / 'include'),
                    *map(str, sorted((root / 'src').glob('*.c'))),
                    '-lm', '-pthread', '-o', str(program)], check=True)
    return program


def build_faults(root, directory):
    library = directory / 'faults.so'
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    subprocess.run([*compiler, '-shared', '-fPIC', '-Wall', '-Wextra',
                    str(root / 'tests/fixtures/partition-faults.c'), '-ldl',
                    '-o', str(library)], check=True)
    return library
