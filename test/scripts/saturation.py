#!/usr/bin/env python3

import os
import runpy
import shlex
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path


reference = runpy.run_path(str(Path(__file__).with_name('reference-dedupe.py')))
dedupe = reference['dedupe']
line_count = reference['line_count']


def check(program, directory, content, flags=()):
    source = directory / 'input'
    output = directory / 'output'
    duplicates = directory / 'duplicates'
    source.write_bytes(content)
    expected, removed = dedupe(content, 1024, '-p' in flags, '-c' in flags, '-C' in flags)
    counts = b'%d duplicates and %d filtered lines' % (
        line_count(removed), line_count(content) - line_count(expected) - line_count(removed))
    for threads in sorted({1, min(4, os.cpu_count() or 1)}):
        result = subprocess.run([program, str(source), '-o', str(output),
                                 '-D', str(duplicates), '-m', '64K', '-t', str(threads),
                                 *flags], stderr=subprocess.PIPE, timeout=5)
        assert result.returncode == 0, (threads, flags, result.stderr)
        assert counts in result.stderr, result.stderr
        assert output.read_bytes() == expected, (threads, flags)
        assert Counter(duplicates.read_bytes().split(b'\n')) == Counter(removed.split(b'\n'))
    result = subprocess.run([program, '-m', '64K', '-t', '1', *flags], input=content,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
    assert result.returncode == 0, result.stderr
    assert counts in result.stderr, result.stderr
    assert result.stdout == expected


def main():
    root = Path.cwd().resolve()
    program = str(Path(sys.argv[1]).resolve()) if len(sys.argv) > 1 else str(root / 'duplicut')
    with tempfile.TemporaryDirectory(prefix='duplicut-saturation-', dir=root) as folder:
        directory = Path(folder)
        words = b''.join(('%03x\n' % i).encode() for i in range(4096))
        check(program, directory, words)
        check(program, directory, words.rstrip(b'\n'))
        check(program, directory, words * 2)
        print('[+] full-table regression with three-byte words')
        alphabet = bytes(range(33, 127))
        for percent in (10, 50, 90):
            unique = 8000 * percent // 100
            words = [bytes((alphabet[i // len(alphabet)], alphabet[i % len(alphabet)]))
                     for i in range(unique)]
            content = b''.join(words[i % unique] + b'\n' for i in range(8000))
            check(program, directory, content)
        print('[+] short words, stable order and duplicate counts at 10/50/90 percent')
        words = b''.join(('%03x\n' % i).encode() for i in range(4096))
        content = b'\0hidden\n\n' + words.replace(b'\n', b'\r\n') + words + b'\0last'
        for flags in ((), ('-p',), ('-c',), ('-C',)):
            check(program, directory, content, flags)
        check(program, directory, words + words.rstrip(b'\n'))
        check(program, directory, b'\0ignored')
        check(program, directory, b'x' * 2048)
        print('[+] filters, case, CRLF, final lines and stdout')
        if len(sys.argv) == 1:
            include = directory / 'include'
            include.mkdir()
            constants = (root / 'include/const.h').read_text()
            (include / 'const.h').write_text(constants.replace(
                '#define HMAP_MAX_LOAD           (0.7)', '#define HMAP_MAX_LOAD           (0.25)'))
            (include / 'hash.h').write_text('#pragma once\n#include <stdint.h>\n'
                                          '#define HASH(line, size) ((uint64_t)0)\n')
            collision_program = str(directory / 'collisions')
            compiler = shlex.split(os.environ.get('CC', 'cc'))
            subprocess.run([*compiler, '-std=gnu99', '-O2', '-DNDEBUG', '-Wall', '-Wextra',
                            '-I' + str(include), '-I' + str(root / 'include'),
                            *map(str, sorted((root / 'src').glob('*.c'))),
                            '-lm', '-pthread', '-o', collision_program], check=True)
            words = [bytes((alphabet[i // len(alphabet) ** 2],
                            alphabet[i // len(alphabet) % len(alphabet)],
                            alphabet[i % len(alphabet)])) for i in range(8400)]
            content = b''.join(word + b'\n' for word in words + list(reversed(words)))
            check(collision_program, directory, content)
            print('[+] forced collisions and multiple passes within one chunk')


if __name__ == '__main__':
    main()
