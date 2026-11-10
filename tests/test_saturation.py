import os
import subprocess
from collections import Counter

from .reference import dedupe, line_count
from .build import build


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


def test_short_table_saturation(program, directory):
    words = b''.join(('%03x\n' % i).encode() for i in range(4096))
    check(program, directory, words)
    check(program, directory, words.rstrip(b'\n'))
    check(program, directory, words * 2)


def test_unique_ratios(program, directory):
    alphabet = bytes(range(33, 127))
    for percent in (10, 50, 90):
        unique = 8000 * percent // 100
        words = [bytes((alphabet[i // len(alphabet)], alphabet[i % len(alphabet)]))
                 for i in range(unique)]
        content = b''.join(words[i % unique] + b'\n' for i in range(8000))
        check(program, directory, content)


def test_filtered_saturation(program, directory):
    words = b''.join(('%03x\n' % i).encode() for i in range(4096))
    content = b'\0hidden\n\n' + words.replace(b'\n', b'\r\n') + words + b'\0last'
    for flags in ((), ('-p',), ('-c',), ('-C',)):
        check(program, directory, content, flags)
    check(program, directory, words + words.rstrip(b'\n'))
    check(program, directory, b'\0ignored')
    check(program, directory, b'x' * 2048)


def test_collision_overflow(directory, root):
    alphabet = bytes(range(33, 127))
    include = directory / 'include'
    include.mkdir()
    constants = (root / 'include/const.h').read_text()
    (include / 'const.h').write_text(constants.replace(
        '#define HMAP_MAX_LOAD           (0.7)', '#define HMAP_MAX_LOAD           (0.25)'))
    (include / 'hash.h').write_text('#pragma once\n#include <stdint.h>\n'
                                  '#define HASH(line, size) ((uint64_t)0)\n')
    collision_program = str(build(root, directory, 'collisions', '-DNDEBUG',
                                  '-I' + str(include)))
    words = [bytes((alphabet[i // len(alphabet) ** 2],
                    alphabet[i // len(alphabet) % len(alphabet)],
                    alphabet[i % len(alphabet)])) for i in range(8400)]
    content = b''.join(word + b'\n' for word in words + list(reversed(words)))
    check(collision_program, directory, content)
